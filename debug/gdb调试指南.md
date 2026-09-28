# xv6 GDB 调试指南(调试用户程序 pingpong 等)

调试 xv6 用户程序(比如实验 1 的 `pingpong`、`primes`)最常用的方法。

## 1. 准备工作

- 容器里已装好 `gdb-multiarch`
- 需要**两个终端**(两个 SSH 会话连进容器,如 `ssh ubuntu@localhost -p 2233`)
- 代码先编译:`make`(确保 `user/_pingpong` 等二进制存在)

## 2. 启动调试(两个窗口)

**窗口 1 —— 启动带 gdb stub 的 qemu:**
```bash
cd /home/xv6
make qemu-gdb
```
- qemu 会**暂停启动**,停在第一个指令,等你连 gdb
- 监听端口 `26000`

**窗口 2 —— 启动 gdb:**
```bash
cd /home/xv6
gdb-multiarch
```
- 会自动读 `.gdbinit`,完成:设置 riscv 架构、连接 `127.0.0.1:26000`、加载 `kernel/kernel` 内核符号
- 连接后输入 `c`(continue)让系统跑起来

> 之后每次想重新调试:`Ctrl-c` 中断 gdb → `kill` → 再 `target remote 127.0.0.1:26000`(或直接重启两个窗口)。

## 3. 常用命令速查

| 命令 | 作用 |
|------|------|
| `c` | 继续执行 |
| `Ctrl-c` | 中断,回到 gdb 提示符 |
| `b <函数>` | 打断点,如 `b sys_pipe`、`b pingpong` |
| `b *0x地址` | 按地址打断点 |
| `n` | 单步跳过(不进入函数) |
| `s` | 单步进入 |
| `si` | 单条**指令**步进(跨 trampoline 时用) |
| `finish` | 跑完当前函数并返回 |
| `p <变量>` | 打印变量,如 `p p->name` |
| `p/x $a0` | 以十六进制打印寄存器 a0 |
| `info registers` | 查看所有寄存器 |
| `x/10i $pc` | 查看当前指令附近的 10 条指令 |
| `x/10x $pc` | 查看当前地址附近的内存 |
| `bt` | 查看调用栈(backtrace) |
| `info b` | 列出所有断点 |
| `d` | 删除断点 |
| `layout asm` / `layout src` | 打开汇编/源码视图 |

## 4. 调试用户程序的两种方法

### 方法 A:在内核系统调用处打断点(最常用)

用户程序(pingpong)的每个系统调用都会进入内核,停在内核函数里就能看到参数、进程名。

```gdb
b sys_pipe      # pingpong 第一次调用 pipe 时停住
b sys_fork      # fork 时停住
b sys_read
b sys_write
b sys_exit
b sys_getpid
c
```

停住后常用查看命令:

```gdb
p p->name        # 当前进程名,确认是 pingpong
p/x $a0          # 系统调用第 1 个参数
p/x $a1          # 系统调用第 2 个参数
bt               # 内核调用栈: sys_pipe <- syscall <- usertrap ...
```

在 `sys_pipe` 停住后单步看实现:`n`、`p fd[0]`、`p fd[1]`。

### 方法 B:直接调试用户程序自己的代码

如果要在 **pingpong 自己的 main 里**单步(而不是内核里):

```gdb
# 切换到用户程序的符号表(会覆盖内核符号)
file user/_pingpong

# 在用户程序的 main 打断点
b main

c
```

- 此时 `b main` 是 **pingpong 的 main**(因为内核 main 符号已被替换掉)
- 等到 qemu 里跑到 pingpong 进程时,断点命中
- 命中后 `n`、`s`、`p` 单步调试用户代码
- 想看回内核符号时:`file kernel/kernel`

> 注意:一个 gdb 会话默认只认一个符号文件。想同时保留内核+用户符号,用
> `add-symbol-file user/_pingpong` 追加(不覆盖)。

## 5. 调试 pingpong 的具体建议

pingpong 流程:`pipe(2 个)` → `fork` → 父子各自 `write/read` → `getpid` 打印 → `exit`。

常见问题与断点:

| 现象 | 怀疑点 | 断点 |
|------|--------|------|
| 卡住不输出 | 读写管道方向用反 / 没关没用到的 fd | `b sys_read`, `b sys_write`, 看 fd 号 |
| 输出顺序不对 | 缺同步,父没等子 | `b sys_wait` |
| 打印了错误 pid | getpid 返回值理解错 | `b sys_getpid` |
| 进程异常退出 | exit 调用问题 | `b sys_exit` |

跨进用户代码的关键:`si`(单条指令)。从内核 syscall 返回用户态经过 trampoline,
用 `si` 一步步跨过去就能进到用户代码;或直接用方法 B 在用户 main 打断点,省去这些。

## 6. 小技巧

- **确认断点命中的进程**:`p p->name` 是 `pingpong` 才是你的程序(init、sh 也会调这些 syscall)。
- **查看用户 PC**:在内核 syscall 里时,`$sepc` 是用户态下一条指令地址,`x/5i $sepc` 看用户代码。
- **kill 后重连**:
  ```gdb
  (gdb) kill
  (gdb) target remote 127.0.0.1:26000
  ```
- **改代码后**:`make qemu-gdb` 会重新编译,`user/_pingpong` 变了,`file user/_pingpong` 要重新加载。