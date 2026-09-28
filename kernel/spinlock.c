// Mutual exclusion spin locks.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void
acquire(struct spinlock *lk)
{
  // acquire 想拿锁，逻辑是：
  // 如果 locked == 0（空闲）→ 我把它设成 1，拿到锁
  // 如果 locked == 1（被占）→ 我拿不到，继续等
  // 关中断
  push_off(); // disable interrupts to avoid deadlock.

  // holding(lk)如果该cpu重复加锁，则返回1
  if(holding(lk))
    panic("acquire");

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  // __sync_lock_test_and_set是一个原子操作，保证设置加锁（lk->locked=1）的过程是原子性的
  // amoswap.w.aq a5, a5, (s1)
  // ① 先读：old = *s1       ← 此刻读出旧值（0）
  // ② 后写：*s1 = 源 a5 = 1  ← 此刻才把内存改成 1
  // ③ 最后：目标 a5 = old    ← 用第①步读出的旧值

  // 如果locked的旧值是0那直接出循环
  // 如果locked的旧值是1，阻塞等待
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  // 现代 CPU 和编译器会乱序执行/重排，可能把临界区里的读写提前到加锁之前
  // acquire(&lk);  x = 1;//临界区里的写  release(&lk);
  // x = 1;//临界区里的写  acquire(&lk);  release(&lk);
  // 抢锁成功
  //    ↓
  // __sync_synchronize()   ← 屏障
  //    ↓
  // 临界区代码（受保护）
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  // 把该锁持有者是哪个cpu进行记录
  lk->cpu = mycpu();
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
// 检查当前cpu是否持有该锁
int
holding(struct spinlock *lk)
{
  int r;
  // 如果当前有自旋锁，且当前负责锁的cpu是当前cpu返回1
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
  // 看现在的内核模式是否支持中断
  int old = intr_get();

  // 将现在的内核模式的中断强制关闭
  // 真正关中断
  intr_off();

  if(mycpu()->noff == 0)
    mycpu()->intena = old; // 只在第一次记录"原来的状态"
  mycpu()->noff += 1; // 关中断的加1
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible"); // 如果发现现在的中断状态是1，不该在中断开时调用
  if(c->noff < 1)
    panic("pop_off"); // 如果发现cpu里面根本没有调用关中断的，还开中断直接panic
  c->noff -= 1; // 简单的直接去除关中断的进程
  if(c->noff == 0 && c->intena) // 只有最后到了最后一个关中断的进程才能开中断
    intr_on();                  // c->intena意愿：最初关中断前的状态该不该还原成开
}
