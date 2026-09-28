// Mutual exclusion lock.
// 自旋锁（spinlock），用来解决多个 CPU 同时访问共享数据的问题
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  // 锁的名字，用于debug
  char *name;        // Name of lock.
  // 具体拿到锁的cpu的信息
  struct cpu *cpu;   // The cpu holding the lock.
};

