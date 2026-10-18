# xv6 内核学习笔记 —— 内存布局 · 启动流程 · 逐模块代码

> 本文把当前带注释阅读的源码(`start.c / entry.S / main.c / riscv.h / vm.c / kalloc.c /
> spinlock.c / sleeplock.c / proc.c / bio.c / virtio_disk.c / fs.c / buf.h / memlayout.h`)
> 串成一条线:**先把要操作的内存画出来(地址、放了什么、谁映射),再讲代码对它做了什么**。
>
> 记号约定:
> - `pa` = 物理地址, `va` = 虚拟地址, `va==pa` 表示恒等映射。
> - 数字以 `_` 分组,高地址在上、低地址在下(地址增大 ↑)。

---

## 目录
0. [我的疑问 → 解答索引(笔记围绕它组织)](#0-我的疑问--解答索引)
1. [阅读地图:哪个文件讲什么](#1-阅读地图哪个文件讲什么)
2. [三个地址空间总览(先建立全局图)](#2-三个地址空间总览)
3. [启动流程:M 模式 → S 模式 → 第一个进程](#3-启动流程)
4. [分页机制:satp / PTE / 三级页表 walk](#4-分页机制)
5. [物理内存分配:kalloc / kfree](#5-物理内存分配)
6. [锁:自旋锁与睡眠锁](#6-锁)
7. [进程:proc[] / 内核栈 / trapframe / swtch / 调度](#7-进程)
8. [缓冲区缓存:bio.c(bget / bwrite / brelse)](#8-缓冲区缓存)
9. [磁盘驱动:virtio_disk_rw](#9-磁盘驱动)
10. [文件系统:inode 缓存与 namei](#10-文件系统)
11. [全流程时序总表](#11-全流程时序总表)

---

## 0. 我的疑问 → 解答索引

> 这份笔记是**围绕我一路问过的问题**组织的:每个小节都尽量从"我当时卡住的地方"讲起。
> 先把我问过的问题列在这里,并给出在本文中的解答位置。

### Q1. bget 回收路径:找到 `refcnt==0` 的 buf(很久没人用),为什么还要 `acquiresleep(&b->lock)`?
> 原文问法:"拿到一个很久没人用的 buf,但是如果没有获取到睡眠锁,说明有人在用,有人在用又怎么会没人用呢?"

- **答案**:这种"refcnt==0 却又占着 b->lock"的情况**不会发生**;回收路径的 `acquiresleep` **不会阻塞**。
- **为什么**:看两条顺序——`bget` 两条路径都**先**增/设 refcnt,**再** `acquiresleep`;`brelse` **先** `releasesleep`,**再** `refcnt--`。
  于是有不变式:**持有 `b->lock` ⟹ `refcnt≥1`;`refcnt==0` ⟹ `b->lock` 已释放**。
- **那为什么还加锁**:为了满足 `bget` 的契约——**"In either case, return locked buffer"(永远返回已加锁的 buffer)**,好让调用者 `bread/bwrite` 直接安全地读写 `b->data`,且 `brelse` 开头的 `holdingsleep` 检查能通过。
- **对比**:命中路径的 `acquiresleep` **才可能阻塞**(多个进程命中同一个缓存块,`refcnt` 会到 2、3…)。
- 👉 详见 [§8.3 bget](#83-bget核心两条路径) 与 [§0.Q4](#q4-命中路径的-acquiresleep-会阻塞而回收路径的不会为什么)。

### Q2. `virtio_disk_rw` 逐行解析
- 为什么会 `kvmpa(&buf0)`、`b->data` 却不用?为什么有**两个 `sleep`**、谁唤醒?`disk`/`refcnt`/`valid` 各是什么?
- 👉 详见 [§9 磁盘驱动](#9-磁盘驱动),尤其 [§9.2](#92-virtio_disk_rwb-write)。

### Q3. 内核如何启动并运行第一个进程(整个启动流程)
- `_entry → start → main → userinit → 调度 → initcode → ecall → sys_exec → /init → sh`。
- 👉 详见 [§3 启动流程](#3-启动流程) 与 [§11 全流程时序总表](#11-全流程时序总表)。

### Q4. 命中路径的 `acquiresleep` 会阻塞,而回收路径的不会,为什么?
- 命中路径:多个进程可能**共享同一个缓存块**(`refcnt` 2、3…),后来者要等先来者用完 → **可能睡**。
- 回收路径:`refcnt==0` 说明**没有任何人引用**这个 buf,自然**没人持有它的 b->lock** → **立即成功**。
- 两个方向一致:"有人在用" 就一定 `refcnt≥1`;`refcnt==0` 就一定没人用。

### Q5. `mret` 明明是"返回",为什么 `start` 不是在返回?
- `start` 并非真的从 S 模式被调用进 M 模式,而是**把 MPP/mepc 布置成"像是要返回"**,借 `mret` 完成 M→S 降级并跳到 `main`。
- 👉 详见 [§3.3 start()](#33-startkernelstartcm-模式--假装返回-到-s-模式)。

### Q6. 为什么要区分自旋锁和睡眠锁?读磁盘为什么不能忙等?
- 自旋锁**忙等**且**关中断**,只适合极短临界区;睡眠锁**睡出让 CPU**,适合可能耗时的操作(如读磁盘)。
- 👉 详见 [§6 锁](#6-锁) 与 [§9.2](#92-virtio_disk_rwb-write) 里"为什么用 sleep 而不是忙等"。

---

## 1. 阅读地图:哪个文件讲什么

| 文件 | 主题 | 关键函数 |
|------|------|----------|
| `kernel/entry.S` | 上电后第一段代码,建栈 | `_entry` |
| `kernel/start.c` | M 模式配置,`mret` 降到 S 模式 | `start` / `timerinit` |
| `kernel/main.c` | 初始化各子系统,造第一个进程,进入调度 | `main` |
| `kernel/memlayout.h` | 物理地址、设备 MMIO、TRAMPOLINE/KSTACK/TRAPFRAME 宏 | — |
| `kernel/riscv.h` | `satp / PTE / satp模式 / sfence.vma`、`MAXVA`、`PGSIZE` | — |
| `kernel/vm.c` | 内核页表、三级 walk、建立/拆除映射 | `kvminit / walk / mappages / uvm*` |
| `kernel/kalloc.c` | 物理页分配器(空闲链表) | `kinit / kfree / kalloc` |
| `kernel/spinlock.c` | 自旋锁(关中断、原子换) | `acquire / release / push_off / pop_off` |
| `kernel/sleeplock.c` | 睡眠锁(拿不到就睡) | `acquiresleep / releasesleep` |
| `kernel/proc.c` | 进程表、内核栈、调度、sleep/wakeup | `procinit / allocproc / userinit / scheduler / swtch / sleep / wakeup` |
| `kernel/buf.h` / `kernel/bio.c` | 缓冲区缓存(磁盘块的内存副本) | `binit / bget / bread / bwrite / brelse` |
| `kernel/virtio_disk.c` | virtio 磁盘驱动(描述符环) | `virtio_disk_rw / virtio_disk_intr` |
| `kernel/fs.c` / `kernel/fs.h` | inode 缓存、目录查找 | `iget / ilock / namei / skipelem` |

---

## 2. 三个地址空间总览

### 图 1:内核虚拟地址空间(高地址区,进程相关)

```
地址增大 ↑
0x40_0000_0000  MAXVA ──────────────────────────  (xv6 不用,避免符号扩展)
0x3F_FFFF_F000 ┌────────────────────────────────┐ TRAMPOLINE = MAXVA-PGSIZE
               │ TRAMPOLINE 页      R|X          │ [0x3FFFFFF000, 0x4000000000) 4KB
0x3F_FFFF_E000 ├────────────────────────────────┤ TRAPFRAME = MAXVA-2PGSIZE
               │ (内核页表不映射,空洞)           │ [0x3FFFFFE000, 0x3FFFFFF000) 4KB
0x3F_FFFF_D000 ├────────────────────────────────┤ KSTACK(0) = MAXVA-3PGSIZE
               │ 进程0 内核栈       R|W          │ [0x3FFFFFD000, 0x3FFFFFE000) 4KB
0x3F_FFFF_C000 ├────────────────────────────────┤
               │ 进程0 保护页(不映射)            │ [0x3FFFFFC000, 0x3FFFFFD000) 4KB
0x3F_FFFF_B000 ├────────────────────────────────┤ KSTACK(1) = MAXVA-5PGSIZE
               │ 进程1 内核栈                    │ [0x3FFFFFB000, 0x3FFFFFC000) 4KB
0x3F_FFFF_A000 │ 进程1 保护页(不映射)            │
0x3F_FFFF_9000 │ 进程2 内核栈                    │
               │            ......               │ 共 64 个进程
0x3F_FFFF_7F000│ 进程63 内核栈                   │
0x3F_FFFF7E000 │ 进程63 保护页(不映射)           │ ← 内核栈区最低处
               └────────────────────────────────┘
                 (以下一大片空闲,直到 KERNBASE 区)
```

### 图 2:内核低地址区 / BSS(恒等映射 va==pa)

```
0x8001_1D68    ┌────────────────────────────────┐ proc[64] (struct proc)
               │ (每个进程: context/kstack/...)  │
0x8000_9830    ├────────────────────────────────┤ stack0[] = 每CPU启动栈/调度器栈
               │ stack0[0] [0x80009830,0x8000A830)│ CPU0
               │ stack0[1] [0x8000A830,0x8000B830)│ CPU1
               │ ... 共 8 个 CPU                  │
0x8000_9030    ├────────────────────────────────┤ mscratch0[]  (M模式时钟保存区)
0x8000_9010    ├────────────────────────────────┤ kernel_pagetable (指针,8B)
0x8000_8830    ├────────────────────────────────┤ initcode[] (52字节)
0x8000_8000    ├────────────────────────────────┤ etext (内核代码结束)
0x8000_0000    └────────────────────────────────┘ KERNBASE / _entry (内核起始)

(0x0 ~ 0x80000000 之间:设备 MMIO 区,恒等映射)
   CLINT  0x02000000 / PLIC 0x0C000000 / UART0 0x10000000 / VIRTIO0 0x10001000
```

### 图 3:用户虚拟地址空间(每进程一张 `p->pagetable`)

```
地址增大 ↑
0x40_0000_0000  MAXVA ──────────────────────────
0x3F_FFFF_F000 ┌────────────────────────────────┐ TRAMPOLINE
               │ TRAMPOLINE 页      R|X          │ 切换代码(trampoline.S)
0x3F_FFFF_E000 ├────────────────────────────────┤ TRAPFRAME
               │ TRAPFRAME 页       R|W          │ 该进程用户寄存器现场
0x3F_FFFF_D000 ├────────────────────────────────┤
               ┊          (大片空闲)             ┊
               ├────────────────────────────────┤
               │ heap (sbrk 向上增长)             │
               │ 栈 (1页,向下增长)                │ ← exec 分配
               │ data                            │
0x0000_0000    │ text (代码,从0开始)   R|X|U      │
               └────────────────────────────────┘
```

### 图 4:物理地址空间

```
0x8800_0000  PHYSTOP ─────────────────────────────
             ┌────────────────────────────────────┐
             │ RAM (内核 + 所有进程的物理页)       │ 128MB
0x8000_0000  ├────────────────────────────────────┤ KERNBASE
             │                                    │
             │ 设备/IO (MMIO)                      │
             │  CLINT  0x02000000                 │
             │  PLIC   0x0C000000                 │
             │  UART0  0x10000000                 │
             │  VIRTIO0 0x10001000                │
0x0000_0000  └────────────────────────────────────┘
```

### 总表

| 名称 | 地址范围 | 大小 | 哪张页表 | 权限/说明 |
|------|----------|------|----------|-----------|
| TRAMPOLINE | `[0x3FFFFFF000, 0x4000000000)` | 4KB | 内核+用户都映射 | R\|X,切换代码 |
| TRAPFRAME | `[0x3FFFFFE000, 0x3FFFFFF000)` | 4KB | 仅用户页表(内核不映射) | R\|W,用户寄存器现场 |
| KSTACK(0) | `[0x3FFFFFD000, 0x3FFFFFE000)` | 4KB | 内核页表 | R\|W,进程0内核栈 |
| 进程0 保护页 | `[0x3FFFFFC000, 0x3FFFFFD000)` | 4KB | 不映射 | 栈溢出保护 |
| KSTACK(1) | `[0x3FFFFFB000, 0x3FFFFFC000)` | 4KB | 内核页表 | 进程1内核栈 |
| … | 每槽 −0x2000 | | | |
| KSTACK(63) | `[0x3FFFF7F000, 0x3FFFF80000)` | 4KB | 内核页表 | 进程63内核栈 |
| 内核栈整区 | `[0x3FFFF7E000, 0x3FFFFFE000)` | 512KB | | 64 ×(栈+保护) |
| stack0 | 0x80009830… 共 32KB | 4096×8 | 内核数据(BSS) | 每 CPU 一页 |
| — CPU0 栈 | `[0x80009830, 0x8000A830)` | 4KB | | sp 初值 0x8000A830 |
| — CPU1 栈 | `[0x8000A830, 0x8000B830)` | 4KB | | sp 初值 0x8000B830 |
| proc[] | 从 0x80011D68 | sizeof(proc)×64 | 内核数据(BSS) | 所有进程结构 |
| initcode | 0x80008830 | 52B | 内核数据 | 内嵌引导程序 |
| kernel_pagetable | 0x80009010 | 8B | 内核数据 | 根页表指针 |
| 内核代码 | `[0x80000000, 0x80008000)` | 32KB | 内核页表(恒等) | R\|X |
| 内核数据+RAM | `[0x80008000, 0x88000000)` | ~128MB | 内核页表(恒等) | R\|W |
| 设备 MMIO | CLINT/PLIC/UART0/VIRTIO0 | — | 内核页表(恒等) | R\|W |
| 用户 text/data/栈/堆 | `[0, sz)` | 动态 | 用户页表 | U\|R\|W(\|X) |
| 用户空闲 | `[sz, TRAPFRAME)` | 动态 | 用户页表 | 未映射 |

**三句话记住:**
1. 高地址区(`0x3FFFFxxxxx`):TRAMPOLINE、TRAPFRAME、KSTACK(0..63) —— **进程相关**。
2. 低地址 BSS(`0x8000xxxx`):stack0(每 CPU 启动/调度器栈)、proc[]、内核代码 —— **内核/CPU 相关**。
3. 物理 RAM 在 `[0x80000000, 0x88000000)`;设备 MMIO 在 `0x02000000~0x10001000`;两者在内核页表里**恒等映射**。

---

## 3. 启动流程

> ❓ **我的疑问(对应 Q3/Q5)**:上电后内核到底怎么一步步跑起来、跑起第一个进程的?`mret` 明明是"返回",为什么 `start` 却在用?"每一步操作了哪块内存"——下面每小节先画内存,再讲代码。

### 3.1 上电与 boot loader

**操作的内存:**

```
0x0 : 0x80000000   设备 MMIO 区(boot ROM 在 0x1000)
0x80000000         内核被加载到这里(_entry 起始)
分页:关闭 → 虚拟地址 == 物理地址
```

```c
// memlayout.h
#define KERNBASE 0x80000000L
#define PHYSTOP  (KERNBASE + 128*1024*1024)
```
- ROM 里的 boot loader(qemu 提供)把内核搬到 `0x80000000`。
- 内核不放 `0x0` 是因为 `0x0:0x80000000` 是 I/O 设备地址区。
- **此时分页关闭**,`va==pa`。

### 3.2 `_entry`(kernel/entry.S)—— 建栈,进入 C

**操作的内存:**

```
内核栈(stack0, 在 BSS):
0x8000_9830 ┌──────────────┐ stack0[0] 底
            │              │
0x8000_A830 ├──────────────┤ stack0[0] 顶 ← CPU0 的 sp
            │              │ stack0[1] ...
```

```asm
_entry:
    la sp, stack0          # 取 stack0 首地址
    li a0, 1024*4          # 4096 = 每 CPU 一页栈
    csrr a1, mhartid        # 读硬件线程号 hartid
    addi a1, a1, 1
    mul a0, a0, a1
    add sp, sp, a0          # sp = stack0 + (hartid+1)*4096 = 该 CPU 栈顶
    call start             # 进入 C
spin:
    j spin
```
- 栈**向低地址增长**,所以 sp 取**栈顶(高地址)**。
- 每个 CPU 各自一页,靠 `hartid` 区分。

### 3.3 `start()`(kernel/start.c,M 模式 → 假装"返回"到 S 模式)

**操作的内存/寄存器:**

```
CSR 寄存器(不是内存,但状态存在这里):
  mstatus.MPP = S      ← mret 后进 S 模式
  mepc        = main   ← mret 后 PC 跳到 main
  satp        = 0      ← S 模式也禁用分页
  medeleg/mideleg = 0xffff ← 异常/中断交给 S 模式
  sie        |= SEIE|STIE|SSIE
  pmpaddr0/pmpcfg0     ← 允许 S 模式访问全部物理内存
  tp(=mhartid)         ← 供 cpuid() 用
```

```c
// 设 MPP = S(清两位再置 S)
x = r_mstatus(); x &= ~MSTATUS_MPP_MASK; x |= MSTATUS_MPP_S; w_mstatus(x);
w_mepc((uint64)main);           // 返回地址=main
w_satp(0);                      // 关分页
w_medeleg(0xffff); w_mideleg(0xffff);   // 委托异常/中断给 S
w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
w_pmpaddr0(0x3fffffffffffffull); w_pmpcfg0(0xf);  // 全物理内存放行
timerinit();                    // 编程时钟中断(见下)
int id = r_mhartid(); w_tp(id); // tp = hartid
asm volatile("mret");           // ★ PC←mepc=main,S 模式,分页仍关
```
`mret` 本意"从 M 返回 S",`start` 并非真被 S 模式调用,而是**把状态布置成像是要返回** → 一跃进 `main`。

`timerinit()`(M 模式时钟):

```
CLINT_MTIMECMP(id) = CLINT_MTIME + interval(1000000 cycles ≈ 0.1s)
mscratch0[32*id + 4] = CLINT_MTIMECMP 地址
mscratch0[32*id + 5] = interval
mtvec = timervec        (kernelvec.S)
mstatus.MIE = 1; mie.MTIE = 1
```
> M 模式时钟由 `timervec` 处理,再转成软件中断给 S 模式的 `devintr()`(因为 S 模式管不了 M 时钟)。

### 3.4 `main()`(kernel/main.c)—— 初始化 + 造第一个进程

**操作的内存(按初始化顺序,全在 BSS/RAM):**

```
0x8000_0000 内核代码
    ...
  kinit()      → kmem.freelist 建好(把 [end,PHYSTOP) 全挂上)
  kvminit()    → 内核页表 kernel_pagetable 建好(恒等映射)
  kvminithart()-> ★ 打开分页(satp = MAKE_SATP(kernel_pagetable))
  procinit()   → proc[64] 每个开一页内核栈,映射到 KSTACK(i)
  ...
  userinit()   → 造第一个进程(initcode)
```

```c
if(cpuid() == 0){
    consoleinit(); printfinit(); ...
    kinit();          // 物理页分配器(freelist)
    kvminit();        // 建内核页表
    kvminithart();    // ★ 打开分页 satp=kernel_pagetable + sfence.vma
    procinit();       // proc[](每进程内核栈)
    trapinit(); trapinithart();
    plicinit(); plicinithart();
    binit();          // 缓冲区缓存
    iinit();          // inode 缓存
    fileinit();       // 文件表
    virtio_disk_init();
    userinit();       // ★ 第一个用户进程
    __sync_synchronize(); started = 1;
} else {
    while(started == 0) ;   // 其他核等待
    kvminithart(); trapinithart(); plicinithart();
}
scheduler();          // ★ 开始调度
```
- **CPU0** 包办初始化并造首进程;其他核等 `started` 后只做自己那份页表/trap/PLIC 设置。

### 3.5 `userinit()`(kernel/proc.c)—— 造出运行 initcode 的进程

**操作的内存:**

```
用户页表 p->pagetable:
  va 0x0  → 物理页 mem, 内容包括 initcode(52B), perm=W|R|X|U
             +----------------------+
             | initcode 机器码       |  ← 从 va=0 开始执行
             +----------------------+
  va TRAMPOLINE→ trampoline 代码(R|X)
  va TRAPFRAME → p->trapframe 数据页(R|W)

p->trapframe 内容(初值):
  epc = 0         ← 用户 PC,从 va=0 开始
  sp  = PGSIZE    ← 用户栈顶
p->sz = PGSIZE
p->state = RUNNABLE
p->name = "initcode"
```

```c
p = allocproc();                        // 分配 proc + trapframe + 用户页表
initproc = p;
uvminit(p->pagetable, initcode, sizeof(initcode));  // 拷 initcode 到 va=0,映射 0..PGSIZE
p->sz = PGSIZE;
p->trapframe->epc = 0;      // 用户下一条指令 = 用户地址 0
p->trapframe->sp  = PGSIZE; // 用户栈顶(就在同一页顶部)
safestrcpy(p->name, "initcode", sizeof(p->name));
p->cwd = namei("/");
p->state = RUNNABLE;        // 可被调度
release(&p->lock);
```

`allocproc()` 里为进程准备的关键现场:

```
p->kstack   = KSTACK(i)       虚拟地址,指向一页内核栈
p->context.ra = (uint64)forkret   ← 第一次被 swtch 跳到 forkret
p->context.sp = p->kstack + PGSIZE ← 内核栈顶
p->trapframe  = kalloc()       一页,存用户寄存器
p->pagetable  = proc_pagetable(p) 建用户页表(含 TRAMPOLINE/TRAPFRAME)
```

### 3.6 从调度到用户态执行 initcode

```
scheduler() 选中 p(state=RUNNABLE)
   → p->state=RUNNING; c->proc=p
   → swtch(&c->context, &p->context)   # 切到 p,ra=forkret
        ↓ 进程第一次运行,落在 forkret
forkret():
   release(&p->lock)
   if(first){ first=0; fsinit(ROOTDEV); }   # 首次:初始化文件系统
   usertrapret()                            # 准备返回用户态
        ↓ trampoline.S:userret
   sret → sepc = trapframe->epc = 0
        → 用户态从 va=0 开始执行 initcode
```

### 3.7 initcode → ecall → sys_exec → /init → sh

**initcode.S(汇编,已在 va=0):**
```asm
start:
    la a0, init          # a0 = "/init" 地址
    la a1, argv          # a1 = {"/init", 0}
    li a7, SYS_exec      # a7 = exec 调用号
    ecall                # ★ 陷入内核(第一次系统调用)
```
**内核处理:**
```
trap → usertrap() → syscall()
   num = p->trapframe->a7;
   p->trapframe->a0 = syscalls[num]();   # SYS_EXEC → sys_exec → exec("/init")
exec 用 /init 替换当前进程的内存与寄存器
   → 回到用户态执行 /init
```
**/init(user/init.c):**
```c
if(open("console", O_RDWR) < 0){ mknod("console", CONSOLE, 0); open("console", O_RDWR); }
dup(0);   // fd1 = stdout
dup(0);   // fd2 = stderr
for(;;){
    pid = fork();
    if(pid == 0) exec("sh", argv);   // 子进程跑 shell
    wait(0);                          // 等 shell 退出
}
```
系统就绪,出现 shell 提示符。

---

## 4. 分页机制

### 4.1 satp(riscv.h)

**satp 的位布局:**
```
 63        60 59                    44 43                       0
+-----------+------------------------+--------------------------+
| Sv39 = 8  |         (保留)         |      根页表的物理页号      |
+-----------+------------------------+--------------------------+
```
```c
#define SATP_SV39 (8L << 60)
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))
// 写 satp = 切换地址空间/页表
static inline void w_satp(uint64 x){ asm volatile("csrw satp, %0" : : "r"(x)); }
```
> satp 只存**根页表物理页号**,硬件从根逐级 walk 三级找到最终物理页。

### 4.2 PTE(riscv.h)

```
PTE 64 位:
 位:  63 ... 10 │ 9 │ 8 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0
      物理页号  │RSV│RSV│ D │ A │ G │ U │ X │ W │ R │ V
  V=位0 有效  R=位1  W=位2  X=位3  U=位4
```
```c
#define PTE_V (1L << 0)
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4)
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)   // 物理页号 → PTE
#define PTE2PA(pte) (((pte) >> 10) << 12)         // PTE → 物理地址
```
**约定:中间级页表项只设 V,不设 R/W/X。**

### 4.3 三级 walk(Sv39)

**虚拟地址划分:**
```
 63      39 38      30 29      21 20      12 11            0
+----------+----------+----------+----------+---------------+
|  必须0   |  L2(9b)  |  L1(9b)  |  L0(9b)  |  页内偏移(12b) |
+----------+----------+----------+----------+---------------+
```
**walk 过程(操作的内存:页表页):**
```
satp ─→ 根页表(L2)
  L2 索引 = va[38:30]  → PTE(带 V) → 下一级物理页
  L1 索引 = va[29:21]  → PTE(带 V) → 下一级物理页
  L0 索引 = va[20:12]  → PTE(叶子) → 物理页
  物理地址 = PTE2PA(叶子PTE) + va[11:0]
```
```c
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc){
  if(va >= MAXVA) panic("walk");
  for(int level = 2; level > 0; level--){
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V){
      pagetable = (pagetable_t)PTE2PA(*pte);   // 下一级页表物理地址
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;         // 中间项:只设 V
    }
  }
  return &pagetable[PX(0, va)];                 // 返回叶子 PTE 地址
}
```
- `TRAMPOLINE = 0x3FFFFFF000`,高 9 位全 1 → L2 索引 = 511 → 就是根页表第 511 项。

### 4.4 建立映射 `mappages` / 内核页表 `kvminit`

**操作的内存:内核页表 `kernel_pagetable`(一页,512 项),内容恒等映射:**

```
kernel_pagetable(根):
  [511] TRAMPOLINE → trampoline 代码
  ...
  低位项:
    UART0   0x10000000 → 0x10000000  R|W
    VIRTIO0 0x10001000 → 0x10001000  R|W
    CLINT   0x02000000 → ... 0x10000  R|W
    PLIC    0x0C000000 → ... 0x400000 R|W
    KERNBASE[0x80000000,etext) R|X   (内核代码)
    [etext, PHYSTOP)            R|W   (内核数据 + RAM)
```

```c
void kvminit(){
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);
  kvmmap(UART0,   UART0,   PGSIZE,   PTE_R|PTE_W);
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE,   PTE_R|PTE_W);
  kvmmap(CLINT,   CLINT,   0x10000,  PTE_R|PTE_W);
  kvmmap(PLIC,    PLIC,    0x400000, PTE_R|PTE_W);
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R|PTE_X);
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R|PTE_W);
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R|PTE_X);
}
```
```c
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm){
  uint64 a = PGROUNDDOWN(va), last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0) return -1;
    if(*pte & PTE_V) panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;         // 叶子项:V + perm
    if(a == last) break;
    a += PGSIZE; pa += PGSIZE;
  }
  return 0;
}
```

### 4.5 拆除映射(uvmunmap / freewalk / uvmfree)

```
uvmunmap: 逐个叶子 PTE 置 0(do_free 时同时 kfree 物理页)
freewalk: 递归整棵页表树,把中间页表页 kfree
uvmfree = uvmunmap(清叶子) + freewalk(清页表页)
```
```c
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free){
  for(uint64 a = va; a < va + npages*PGSIZE; a += PGSIZE){
    pte_t *pte = walk(pagetable, a, 0);
    if(pte == 0) panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0) panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V) panic("uvmunmap: not a leaf");
    if(do_free) kfree((void*)PTE2PA(*pte));
    *pte = 0;
  }
}
void freewalk(pagetable_t pagetable){
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){  // 中间项
      freewalk((pagetable_t)PTE2PA(pte));
      pagetable[i] = 0;
    } else if(pte & PTE_V) panic("freewalk: leaf");
  }
  kfree((void*)pagetable);
}
```

---

## 5. 物理内存分配

### 5.1 空闲链表结构

```
kmem{ lock, freelist }
freelist ─→ [页A | next] ─→ [页B | next] ─→ ... ─→ 0
            (每页开头当 struct run 用,只存 next)
范围:freerange(end, PHYSTOP)  →  [内核末尾 end, 0x88000000)
```

```c
struct run { struct run *next; };
struct { struct spinlock lock; struct run *freelist; } kmem;

void kinit(){ initlock(&kmem.lock,"kmem"); freerange(end,(void*)PHYSTOP); }
```

### 5.2 kfree / kalloc

**kfree(操作:往链表头插一页;并把整页填 `1`):**
```c
void kfree(void *pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  memset(pa, 1, PGSIZE);              // 填垃圾,帮助发现悬空指针
  struct run *r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;            // 头插
  kmem.freelist = r;
  release(&kmem.lock);
}
```
**kalloc(操作:从链表头摘一页;填 `5`):**
```c
void *kalloc(void){
  acquire(&kmem.lock);
  struct run *r = kmem.freelist;
  if(r) kmem.freelist = r->next;      // 摘头
  release(&kmem.lock);
  if(r) memset((char*)r, 5, PGSIZE);  // 填垃圾
  return (void*)r;
}
```
> 填垃圾的意义:若有人没初始化就用这页,会读到 `0x05050505`,一眼看出是未初始化。

---

## 6. 锁

### 6.1 自旋锁(忙等,关中断)

**结构 + 状态:**
```
struct spinlock { uint locked; char *name; struct cpu *cpu; }
      locked=0 空闲 / locked=1 被占用;cpu 记录持有者(便于 holding 检查)
```

```c
void acquire(struct spinlock *lk){
  push_off();                          // 关中断,避免死锁
  if(holding(lk)) panic("acquire");
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)  // amoswap 原子
    ;
  __sync_synchronize();                // 屏障:临界区访问锁之后
  lk->cpu = mycpu();
}
void release(struct spinlock *lk){
  if(!holding(lk)) panic("release");
  lk->cpu = 0;
  __sync_synchronize();                // 屏障:临界区写在解锁之前可见
  __sync_lock_release(&lk->locked);    // 原子置 0
  pop_off();
}
```
`push_off/pop_off` 成对,记录"进临界区前中断是否开着",支持嵌套:
```
push_off: old=intr_get(); intr_off(); if(noff==0) intena=old; noff++
pop_off : noff--; if(noff==0 && intena) intr_on();
```
> **自旋锁 vs 睡眠锁**:自旋锁**忙等**,适合短临界区且**必须关中断**;睡眠锁**睡眠让出 CPU**,适合可能长时间的临界区(如读磁盘),但不能在中断上下文用。

### 6.2 睡眠锁(拿不到就睡)

**结构:**
```
struct sleeplock { uint locked; struct spinlock lk; char *name; int pid; }
  locked: 是否被占;lk: 保护该结构的自旋锁;pid: 持有者
```
```c
void acquiresleep(struct sleeplock *lk){
  acquire(&lk->lk);
  while(lk->locked) sleep(lk, &lk->lk);   // 睡在 lk 上,睡眠时释放 lk->lk
  lk->locked = 1; lk->pid = myproc()->pid;
  release(&lk->lk);
}
void releasesleep(struct sleeplock *lk){
  acquire(&lk->lk);
  lk->locked = 0; lk->pid = 0;
  wakeup(lk);                             // 唤醒等待者
  release(&lk->lk);
}
int holdingsleep(struct sleeplock *lk){
  acquire(&lk->lk);
  int r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}
```
> `acquiresleep` 用 `sleep(lk, &lk->lk)`:睡眠期间释放 `lk->lk`(自旋锁),醒来时自动拿回——**不能拿着自旋锁睡觉,否则别人永远进不来**。

---

## 7. 进程

### 7.1 进程表与内核栈(procinit)

**操作的内存:**
```
proc[64](BSS)                    内核栈(高地址,每进程 2 页:栈+保护)
+------------------+             KSTACK(0) [0x3FFFFFD000,0x3FFFFFE000) R|W  ← p->kstack
| struct proc p[0] |  kstack ────→ 保护页  [0x3FFFFFC000,0x3FFFFFD000) 不映射
|  lock/state/...  |             KSTACK(1) [0x3FFFFFB000,0x3FFFFFC000) R|W
|  kstack=KSTACK(0)|             ...
|  trapframe=ptr ──┼──→ 一页(存用户寄存器)
|  context{ra,sp..}|             p->context.sp = p->kstack + PGSIZE(栈顶)
|  pagetable=ptr ──┼──→ 用户页表(根)
+------------------+
```

```c
void procinit(void){
  initlock(&pid_lock, "nextpid");
  for(p = proc; p < &proc[NPROC]; p++){
    initlock(&p->lock, "proc");
    char *pa = kalloc();                     // 一页物理内存当内核栈
    if(pa == 0) panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));     // 高地址虚拟地址
    kvmmap(va, (uint64)pa, PGSIZE, PTE_R|PTE_W);
    p->kstack = va;
  }
  kvminithart();
}
```
> 每个进程有**自己的 p->lock**,保证状态切换时互不干扰(CPU 资源隔离)。

### 7.2 分配进程(allocproc)

```
步骤:遍历 proc[] 找 UNUSED → 加锁
  p->pid = allocpid()
  p->trapframe = kalloc()        一页,存用户寄存器
  p->pagetable = proc_pagetable() 建用户页表(含 TRAMPOLINE/TRAPFRAME)
  memset(&p->context,0,...)
  p->context.ra = forkret        第一次被调度跳这里
  p->context.sp = p->kstack+PGSIZE
返回时 p->lock 仍持有
```

`proc_pagetable` 建立用户页表的两项固定映射:
```
va TRAMPOLINE → trampoline 代码   R|X
va TRAPFRAME  → p->trapframe      R|W
```

### 7.3 上下文切换 swtch(操作:context 结构)

**内存中的两个 context:**
```
调度器                                 进程 p
c->context { ra=... , sp=调度器栈, s0..s11 }   p->context { ra=forkret, sp=p->kstack+PGSIZE, s0..s11 }
        ↑ swtch(&c->context, &p->context) 把当前现场存进 c->context,再从 p->context 恢复
```
```c
// scheduler():(只持 p->lock)
p->state = RUNNING; c->proc = p;
swtch(&c->context, &p->context);   // 切到进程:ra=forkret → 进程首次运行落 forkret
c->proc = 0;
```
- `swtch` 保存**被调用者保存寄存器 + ra + sp**;进程"回来"时从 `p->context` 恢复,回到 `swtch` 之后。
- 进程首次运行 `ra=forkret`(见 3.6)。

### 7.4 sched / yield / sleep / wakeup(操作:proc 状态)

**状态机:**
```
UNUSED → allocproc → RUNNABLE → scheduler → RUNNING
RUNNING → yield/sleep → RUNNABLE/SLEEPING
RUNNING → exit → ZOMBIE → wait → (freeproc) UNUSED
```
```c
void sleep(void *chan, struct spinlock *lk){
  struct proc *p = myproc();
  if(lk != &p->lock){ acquire(&p->lock); release(lk); }  // 先锁自己,再放条件锁
  p->chan = chan; p->state = SLEEPING;
  sched();                        // 让出 CPU
  p->chan = 0;
  if(lk != &p->lock){ release(&p->lock); acquire(lk); }  // 醒后拿回条件锁
}
void wakeup(void *chan){
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->state == SLEEPING && p->chan == chan) p->state = RUNNABLE;
    release(&p->lock);
  }
}
```
> `sleep` 先持 `p->lock` 再放条件锁,保证不会漏掉 `wakeup`(`wakeup` 也要 `p->lock`)。

### 7.5 第一个进程与 shell 启动链

```
userinit → allocproc(建 trapframe/pagetable/context)
         → uvminit(拷 initcode 到 va=0)
         → epc=0, sp=PGSIZE, state=RUNNABLE
scheduler → swtch → forkret → fsinit(首次) → usertrapret → sret
用户态 va=0 执行 initcode → ecall → syscall → sys_exec → exec("/init")
/init → open("console") → dup(0)x2 → fork/exec("sh") → wait
```

---

## 8. 缓冲区缓存

### 8.1 结构(bio.c / buf.h)

**操作的内存:**
```
bcache{
  struct spinlock lock;
  struct buf buf[NBUF=30];     每个 buf 含 data[BSIZE=1024]
  struct buf head;             哨兵(双向循环链表头)
}
链表(head.next=最近用,head.prev=最久没用):
  head ⇄ buf₀ ⇄ buf₁ ⇄ ... ⇄ buf₂₉ ⇄ head(循环)
```
```c
struct buf {
  int valid;              // 数据是否已从磁盘读入
  int disk;               // 磁盘是否正"占用"这个 buf
  uint dev, blockno;      // 设备号 / 块号
  struct sleeplock lock;  // 数据锁(睡眠锁)
  uint refcnt;            // 引用计数
  struct buf *prev, *next;// LRU 双向链表
  uchar data[BSIZE];      // 实际缓存数据
};
```

### 8.2 binit(建链表)

```
head.next = head; head.prev = head;
for 每个 buf:头插 —— buf.next=head.next; buf.prev=head; initsleeplock();
```

### 8.3 bget(核心,两条路径)

> ❓ **我的疑问(对应 Q1)**:回收路径已经找到 `refcnt==0`(很久没人用)的 buf,为什么还要
> `acquiresleep(&b->lock)`?如果这把锁没拿到,不就说明"有人在用",那和"没人用(refcnt==0)"不就矛盾了吗?
> **先记住两条路径的加锁顺序,答案就在下面。**

**命中路径(操作)**:`refcnt++` → 放自旋锁 → 拿睡眠锁
```c
for(b = bcache.head.next; b != &bcache.head; b = b->next){
  if(b->dev == dev && b->blockno == blockno){
    b->refcnt++;
    release(&bcache.lock);     // 先放自旋锁
    acquiresleep(&b->lock);    // 再拿睡眠锁(可能阻塞:多人共享同一 buf)
    return b;
  }
}
```
**回收路径(操作)**:从左端找 `refcnt==0`,改写成目标块
```c
for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  if(b->refcnt == 0){
    b->dev = dev; b->blockno = blockno; b->valid = 0; b->refcnt = 1;
    release(&bcache.lock);
    acquiresleep(&b->lock);    // refcnt==0 → 锁空闲 → 不会阻塞
    return b;
  }
}
panic("bget: no buffers");
```

> **关键不变量(回答"refcnt==0 却又可能等睡眠锁?"的疑问):**
> - `bget` 两条路径都**先**增/设 refcnt,**再** `acquiresleep`;`brelse` **先** `releasesleep`,**再** `refcnt--`。
> - 所以:**持有 `b->lock` ⟹ `refcnt≥1`;`refcnt==0` ⟹ `b->lock` 已释放**。
> - 因此回收路径的 `acquiresleep` **不会阻塞**,它是为了满足 `bget` 的契约——**返回一个已加锁的 buffer**(`brelse` 开头会 `holdingsleep` 检查)。
> - 对比:命中路径 `refcnt` 可为 2、3…(**多进程共享同一缓存块**),那里的 `acquiresleep` **可能阻塞**。

### 8.4 bread / bwrite / brelse

```c
struct buf* bread(uint dev, uint blockno){
  struct buf *b = bget(dev, blockno);
  if(!b->valid){ virtio_disk_rw(b, 0); b->valid = 1; }  // 未读过 → 读盘
  return b;
}
void bwrite(struct buf *b){
  if(!holdingsleep(&b->lock)) panic("bwrite");
  virtio_disk_rw(b, 1);                                  // 写盘
}
void brelse(struct buf *b){
  if(!holdingsleep(&b->lock)) panic("brelse");
  releasesleep(&b->lock);        // ① 先放数据锁
  acquire(&bcache.lock);
  b->refcnt--;                   // ② 再减引用
  if(b->refcnt == 0){            // 没人等了 → 移到链表头(MRU)
    b->next->prev = b->prev; b->prev->next = b->next;
    b->next = bcache.head.next; b->prev = &bcache.head;
    bcache.head.next->prev = b; bcache.head.next = b;
  }
  release(&bcache.lock);
}
```
- `bpin/bunpin`:手动增/减 refcnt(防止被回收)。

---

## 9. 磁盘驱动

### 9.1 virtio 环结构

**操作的内存:**
```
struct disk {
  struct spinlock vdisk_lock;   保护整个环
  char free[NUM=8];             描述符是否空闲
  struct virtq_desc desc[NUM];  描述符环
  struct virtq_avail avail;     驱动→设备
  struct virtq_used used;       设备→驱动
  int used_idx;                 intr 用
  struct { struct buf *b; uint status; } info[NUM];
}
```
一个块操作 = **3 个描述符**(头 / 数据 / 状态);8 个槽最多 2 次并发,剩余不足就等。

### 9.2 virtio_disk_rw(b, write)

> ❓ **我的疑问(对应 Q2)**:`buf0` 为什么要 `kvmpa(&buf0)` 而 `b->data` 不用?为什么函数里有**两个 `sleep`**,分别睡在什么条件上、谁唤醒?`sector` 为什么是 `blockno*2`?
> 下面按"操作内存 → 代码"逐段回答。

`write==1` 写盘,`write==0` 读盘。一个块 = 2 个扇区(1024/512)。

```
sector = b->blockno * 2
acquire(&disk.vdisk_lock)                     ← 独占环
分配 3 个描述符(不足则 sleep(&disk.free[0], ...))
desc[0]: 头 virtio_blk_outhdr{type,sector}  (kvmpa 转物理地址,因为在内核栈)
desc[1]: 数据 b->data (BSIZE)  write? flags=0 : flags=WRITE
desc[2]: 状态 1 字节
b->disk = 1; disk.info[idx[0]].b = b          ← 供中断认领
disk.avail[...] = idx[0]; __sync_synchronize(); disk.avail[1]++
*R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0              ← 通知设备
while(b->disk == 1) sleep(b, &disk.vdisk_lock) ← 睡眠等中断
disk.info[idx[0]].b = 0; free_chain(idx[0]);
release(&disk.vdisk_lock)
```

**两个 sleep 与谁唤醒:**

| 位置 | 睡在哪(chan) | 等什么 | 谁唤醒 |
|------|--------------|--------|--------|
| 分配描述符 | `&disk.free[0]` | 有空闲描述符 | `free_desc` → `wakeup(&disk.free[0])` |
| 等 I/O | `b` | 磁盘完成 | `virtio_disk_intr` → `wakeup(b)` |

**中断处理:**
```c
void virtio_disk_intr(){
  acquire(&disk.vdisk_lock);
  while((disk.used_idx % NUM) != (disk.used->id % NUM)){
    int id = disk.used->elems[disk.used_idx].id;
    if(disk.info[id].status != 0) panic("virtio_disk_intr status");
    disk.info[id].b->disk = 0;      // 标记完成
    wakeup(disk.info[id].b);        // 唤醒等待的进程
    disk.used_idx = (disk.used_idx + 1) % NUM;
  }
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
  release(&disk.vdisk_lock);
}
```
**要点:**
- `kvmpa(&buf0)`:设备用**物理地址** DMA;`buf0` 在内核栈(非恒等映射)要转;`b->data` 在 bcache(恒等)直接用。
- `vdisk_lock` 保护环的**短操作**;睡眠时由 `sleep` 释放,醒后自动拿回。

---

## 10. 文件系统

### 10.1 磁盘布局(blocks)

```
 0        1        2                  inodestart         ...        FSSIZE-1
+--------+--------+------------------+------------------+----------+
| boot   | super  |  log blocks      | inode blocks     | data     |
+--------+--------+------------------+------------------+----------+
 superblock: size/nlog/inodestart/bmapstart/... (fs.h)
 IBLOCK(i) = sb.inodestart + i/IPB   (inode 号 → 磁盘块号)
```

### 10.2 inode 缓存(iget / ilock)

**操作的内存:icache(全局,inode 缓存,所有 CPU 共享):**
```
icache{
  struct spinlock lock;
  struct inode inode[NINODE=50];
}
每个 inode: ref(引用计数) / dev / inum / valid / lock(睡眠锁) / 磁盘字段
iget(dev,inum): 查缓存,命中 ref++;未命中取空槽,填 dev/inum, ref=1
ilock(ip)     : acquiresleep(&ip->lock); 若 !valid 从磁盘读 IBLOCK(inum)
```

### 10.3 namei / skipelem(路径解析)

```c
struct inode* namei(char *path){
  if(*path == '/') ip = iget(ROOTDEV, ROOTINO);   // 绝对路径:从根开始
  else             ip = idup(myproc()->cwd);       // 相对路径:从当前目录
  while((path = skipelem(path, name)) != 0){       // 逐段解析
    ilock(ip);
    if(ip->type != T_DIR) { ... return 0; }
    ip = dirlookup(ip, name, 0);                    // 目录里找下一段
    iunlockput(ip);
    if(ip == 0) return 0;
  }
  return ip;
}
```
- `skipelem`:`/home/cpp` → 第一段 `name="home"`,`path="cpp"`。

---

## 11. 全流程时序总表

```
[硬件] 上电 → boot loader → 内核加载到 0x80000000(分页关闭)
   │
[entry.S] _entry:sp = stack0+(hartid+1)*4096 → call start
   │
[start.c] start(M模式):
   │   mstatus.MPP=S; mepc=main; satp=0; medeleg/mideleg=0xffff; PMP; timerinit(); tp=hartid
   │   mret →(S模式) main
   │
[main.c] main():CPU0 → kinit/kvminit/kvminithart(开分页)/procinit/trap/plic/
   │              binit/iinit/fileinit/virtio_disk_init/userinit → started=1 → scheduler()
   │  其他CPU:等 started → 各自 kvminithart/trapinithart/plicinithart → scheduler()
   │
[proc.c] userinit():allocproc + uvminit(initcode→va0) + epc=0,sp=PGSIZE,state=RUNNABLE
   │
[proc.c] scheduler():RUNNABLE → swtch(&c->context,&p->context) ra=forkret
   │
[proc.c] forkret():release(p->lock); 首次 fsinit(ROOTDEV); usertrapret()
   │
[tramp] userret → sret → 用户态 va=0 执行 initcode
   │
[initcode] a7=SYS_exec; ecall
   │
[syscall] syscall():num=trapframe->a7 → syscalls[num]() → sys_exec → exec("/init")
   │
[init.c] /init:open("console"); dup(0)x2; fork(); exec("sh"); wait()
   │
[sh] shell 运行 → 系统就绪
```

> **一句话串联**:boot loader 把内核放到 `0x80000000` → `_entry` 建栈 → `start` 配好 M 模式并用 `mret` 假装"返回"到 S 模式的 `main` → `main` 初始化各子系统(打开分页)后 `userinit` 造出跑 `initcode` 的第一进程 → 调度进入用户态 → `ecall` 触发 `exec("/init")` → `/init` 起 `sh` → 系统就绪。

---

### 附:本次记录涉及的疑难(可回看)

| 编号 | 疑问(我的原话 / 直觉) | 结论 | 详见 |
|------|------------------------|------|------|
| Q1 | 回收路径拿到 `refcnt==0` 的 buf,若没拿到睡眠锁就说明有人在用,岂不矛盾? | 不会发生:`refcnt==0 ⟹ b->lock 空闲`;回收路径的 `acquiresleep` 不阻塞,只为"返回已加锁 buffer"的契约 | §8.3 |
| Q2 | `virtio_disk_rw` 为何 `kvmpa`、为何有两个 `sleep` | 设备 DMA 用物理地址(内核栈要转);两处 sleep 分别等"空闲描述符"和"I/O 完成" | §9.2 |
| Q3 | 内核如何启动并运行第一个进程 | `_entry→start→main→userinit→调度→initcode→ecall→exec("/init")→sh` | §3 / §11 |
| Q4 | 命中路径 `acquiresleep` 会阻塞、回收路径不会 | 命中时多人共享同一 buf(refcnt≥2);回收时无人引用 | §8.3 |
| Q5 | `mret` 是"返回",为何 `start` 用它 | 把 MPP/mepc 布置成"像是要返回",借 `mret` 降级到 S 模式并跳 `main` | §3.3 |
| Q6 | 自旋锁 / 睡眠锁区别,读盘为何不忙等 | 自旋锁忙等+关中断;睡眠锁睡出让 CPU,适合耗时操作 | §6 / §9.2 |
