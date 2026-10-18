#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
// 内核页表
// 因为 xv6 内核是恒等映射(虚拟地址 = 物理地址),所以这个指针的值 既是虚拟地址、也是物理地址
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
// 但 satp 只需要存"根页"的物理页号,硬件会自己从根开始逐级 walk 三级找到最终的物理页。所以你看到:
void
kvminithart()
{
  // 因为 xv6 内核是恒等映射(虚拟地址 = 物理地址),所以这个指针的值 既是虚拟地址、也是物理地址
  w_satp(MAKE_SATP(kernel_pagetable));
  // 清空tlb
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
// walk(pagetable, a, 1)
// a = PGROUNDDOWN(va); 
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  // MAXVA用户虚拟地址最大值
  if(va >= MAXVA)
    panic("walk");
  // level从 2 到 1 两级中间页表
  for(int level = 2; level > 0; level--) {
    // level=2的时候 va左移12+2*9=30, 留出最高9位为一级索引
    // 4096B 512个页表项
    // TRAMPOLINE = 0x3FFFFFF000 高9位全1 为2^9-1=511
    // 得到第511个页表项的地址
    pte_t *pte = &pagetable[PX(level, va)];
    // 查看第0位是否，有效位，看看物理地址是否存在
    if(*pte & PTE_V) {
      // 存在的话拿出物理地址
      // PTE2PA(*pte)：从 PTE 取出物理地址（下一级页表的地址）
      // 左移10位跳过低位（有效位那些）再右移12位，为虚拟地址的最后12位物理地址腾空间
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      // 调用者不允许创建（alloc=0）→ 返回 0（失败）
      // 如果分配失败返回0
      // 将原来的页表pagetable覆盖成新的页表物理地址
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      // kalloc返回的正式新页的物理地址
      // 去掉物理偏移地址，转成虚拟地址
      // 把该表项存入新页的虚拟地址
      // pagetable物理地址 pa：  [物理页号][页内偏移 12位]
      // PTE：  [物理页号][权限 10位]
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }

  // 左移12位&最低位9位得到，三级页表的511的表项
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  // kernel_pagetable全局的第一级页表
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(kernel_pagetable, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  // mappages(pagetable, TRAMPOLINE, PGSIZE,
  //             (uint64)trampoline, PTE_R | PTE_X)
  // TRAMPOLINE是256GB-1页的初始地址
  uint64 a, last;
  pte_t *pte;
  // 把 a 的低 12 位清零，也就是向下对齐到 4096 的倍数
  // TRAMPOLINE是256GB-1页的初始地址
  a = PGROUNDDOWN(va);
  // 256GB那会的地址
  last = PGROUNDDOWN(va + size - 1);
  // 虚拟地址 va 开始的那一页 映射 物理地址 trampoline 开始的那一页。
  for(;;){
    // 通过页表搜寻建立一个三级页表的511项的pte
    // pte是页表项的地址，如果没有这个地址，说明分配页表页失败（内存耗尽）
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    // 如果该页表项存在且页表项的有效位也对，说明找到的页表不对
    // 这个虚拟地址是否已经映射过了
    if(*pte & PTE_V)
      panic("remap");
    // 拿到物理页号，设置后9位为perm,若perm为PTE_R | PTE_X说明可读可写
    // 还设置有效位为1
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
// uvmunmap(pagetable, TRAMPOLINE, 1, 0);
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    // 找到pte, alloc=0：因为要解除映射，页表应该已经存在，不需要创建。
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    // 要解除的映射应该存在
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    // PTE_FLAGS(*pte)：取出 PTE 的权限位 只有 V，没有 R/W/X，说明返回的不是叶子节点
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    // do_free = 1：释放物理页
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    // 把 PTE 设为 0
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  // 用 uint64 * 指向页表开头，就能用 pt[i] 方便地访问第 i 个页表项
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  // 页表制空
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  // 分配一页物理内存,把 initcode 拷进去,然后映射到用户虚拟地址 0
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  // mappages已经把联系建好了，memmove，把initcode放进去
  // initcode 是一段极小的汇编程序,编在内核镜像里
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
// freewalk 的职责是"释放页表页"
// freewalk 就是"暴力递归"整棵页表树，把每一页页表页都 kfree 掉
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // 含义：PTE 有效，但没有读写执行权限，说明它指向下一级页表（不是叶子映射）
    // RISC-V 的约定：中间级 PTE 只设 V，不设 R/W/X说明是中间叶子节点
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      // 得到该二级页表的物理页地址
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      // 清空该页的第i项目的pte
      pagetable[i] = 0;
      // 如果 PTE 有效，且有 R/W/X（是叶子映射），panic。
      // 因为调用者应该先清除所有叶子映射，再调 freewalk。
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  // kfree 释放的是该页表占用的"一整页物理内存"
  // 释放页表本身的页
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  // sz = 0：没有用户内存，跳过 uvmunmap，直接 freewalk
  // PGROUNDUP(sz)：把 sz 向上对齐到页边界/PGSIZE得到页数
  // sz = 进程用户内存的大小（字节数）
  // sz > 0一次调用 uvmfree，完成两步:uvmunmap清叶子，freewalk清页表页
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}
