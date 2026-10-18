struct buf {
  // 数据是否已从磁盘读入。
  int valid;   // has data been read from disk?
  // 磁盘是否正在"占用"这个 buf
  int disk;    // does disk "own" buf?
  // 设备号（哪个磁盘）
  uint dev;
  // 块号（磁盘上的第几块）
  uint blockno;
  // 这个 buffer 的数据锁（睡眠锁）
  struct sleeplock lock;
  // 引用计数
  uint refcnt;
  // 双向链表的指针（前一个、后一个）
  struct buf *prev; // LRU cache list
  struct buf *next;
  // 实际的缓存数据
  uchar data[BSIZE];
};

