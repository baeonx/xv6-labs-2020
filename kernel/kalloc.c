// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  // 页对齐（地址是 4096 的倍数）
  // 不小于内核末尾 end（不能释放内核代码/数据）
  // 小于物理内存末尾（不能释放超出内存的地址）
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  // 如果有人还拿着这页的指针，在释放后访问，会读到 0x01010101。
  memset(pa, 1, PGSIZE);

  // 空闲页头指针
  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist; // 新页的 next 指向原来的头
  kmem.freelist = r; // freelist 指向新页
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  // 从空闲链表取一页
  r = kmem.freelist;
  // 如果有空闲页直接取一夜
  // 页A -> 页B -> 0
  if(r)
    kmem.freelist = r->next; // freelist 指向页B（页A的 next）
  release(&kmem.lock);

  // 取出来了r空闲页
  // 为什么填垃圾：帮助发现 bug
  // 如果程序没初始化就用这页，会读到 0x05050505，一看就知道是垃圾
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  
  // 返回这页的地址
  return (void*)r;
}


uint64
collectFreePages(void)
{
  uint64 count = 0;
  struct run *r;
  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r) {
    r = r->next;
    count++;
  }
  release(&kmem.lock);
  return count;
}
