# 实验: 锁(Locks)

在本实验中,你将获得重新设计代码以提高并行度(parallelism)的经验。多核机器上并行度不佳的一个常见症状是锁竞争(lock contention)严重。提高并行度通常需要同时改变数据结构和锁策略,以减少竞争。你将针对 xv6 的内存分配器(memory allocator)和块缓存(block cache)完成这项工作。

在编写代码之前,请确保阅读[xv6 手册](../xv6/book-riscv-rev1.pdf)的以下部分:

- 第 6 章:"Locking"及相应代码。

- 第 3.5 节:"Code: Physical memory allocator"

- 第 8.1 到 8.3 节:"Overview"、"Buffer cache layer"和"Code: Buffer cache"
```
  $ git fetch
  $ git checkout lock
  $ make clean
```

## Memory allocator

程序 `user/kalloctest` 会对 xv6 的内存分配器施加压力:三个进程不断增长和收缩它们的地址空间,导致大量对 `kalloc` 和 `kfree` 的调用。`kalloc` 和 `kfree` 需要获取 `kmem.lock`。kalloctest 会打印(以 "#fetch-and-add" 的形式)在 `acquire` 中由于试图获取一个已被其他核心持有的锁而导致的循环迭代次数,针对 kmem 锁以及其他几个锁。`acquire` 中的循环迭代次数是锁竞争程度的一个粗略度量。在你完成本实验之前,kalloctest 的输出与下面类似:
```
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 83375 #acquire() 433015
lock: bcache: #fetch-and-add 0 #acquire() 1260
--- top 5 contended locks:
lock: kmem: #fetch-and-add 83375 #acquire() 433015
lock: proc: #fetch-and-add 23737 #acquire() 130718
lock: virtio_disk: #fetch-and-add 11159 #acquire() 114
lock: proc: #fetch-and-add 5937 #acquire() 130786
lock: proc: #fetch-and-add 4080 #acquire() 130786
tot= 83375
test1 FAIL
```
`acquire` 为每个锁维护该锁被 `acquire` 调用的次数,以及 `acquire` 中循环尝试设置锁但失败的次数。kalloctest 调用一个系统调用,使内核打印这些计数:针对 kmem 和 bcache 锁(它们是本实验的重点)以及竞争最激烈的 5 个锁。如果存在锁竞争,`acquire` 循环迭代的次数会很大。该系统调用返回 kmem 和 bcache 锁的循环迭代次数之和。

对于本实验,你必须使用一台专用的、未被其他任务占用的多核机器。如果你使用一台正在做其他事情的机器,kalloctest 打印的计数将毫无意义。你可以使用专用的 Athena 工作站或你自己的笔记本电脑,但不要使用拨号(dialup)机器。

kalloctest 中锁竞争的根本原因是 `kalloc()` 只有一个空闲链表(free list),由一把锁保护。为了消除锁竞争,你将不得不重新设计内存分配器,以避免单把锁和单个链表。基本的想法是为每个 CPU 维护一个空闲链表,每个链表有自己的锁。不同 CPU 上的分配和释放可以并行运行,因为每个 CPU 操作的是不同的链表。主要的挑战是处理这种情况:一个 CPU 的空闲链表为空,而另一个 CPU 的链表有空闲内存;在这种情况下,那个 CPU 必须"偷取(steal)"另一个 CPU 空闲链表的一部分。偷取可能会引入锁竞争,但希望这种情况很少发生。

你的任务是实现每 CPU 的空闲链表,以及当一个 CPU 的空闲链表为空时的偷取。你必须给你所有的锁起以 "kmem" 开头的名字。也就是说,你应该为你每个锁调用 `initlock`,并传入一个以 "kmem" 开头的名字。运行 kalloctest 看看你的实现是否减少了锁竞争。为了检查它仍然能分配所有内存,运行 `usertests sbrkmuch`。你的输出将与下面显示的类似,kmem 锁上的竞争总量大大减少,尽管具体数字会有所不同。确保 usertests 中的所有测试都通过。`make grade` 应显示 kalloctests 通过。
```
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 42843
lock: kmem: #fetch-and-add 0 #acquire() 198674
lock: kmem: #fetch-and-add 0 #acquire() 191534
lock: bcache: #fetch-and-add 0 #acquire() 1242
--- top 5 contended locks:
lock: proc: #fetch-and-add 43861 #acquire() 117281
lock: virtio_disk: #fetch-and-add 5347 #acquire() 114
lock: proc: #fetch-and-add 4856 #acquire() 117312
lock: proc: #fetch-and-add 4168 #acquire() 117316
lock: proc: #fetch-and-add 2797 #acquire() 117266
tot= 0
test1 OK
start test2
total free number of pages: 32499 (out of 32768)
.....
test2 OK
$ usertests sbrkmuch
usertests starting
test sbrkmuch: OK
ALL TESTS PASSED
$ usertests
...
ALL TESTS PASSED
$
```
一些提示:

- 你可以使用 `kernel/param.h` 中的常量 `NCPU`

- 让 `freerange` 把所有空闲内存交给运行 `freerange` 的 CPU。

- 函数 `cpuid` 返回当前核心号,但只有在中断关闭时调用它并使用其结果才是安全的。你应该使用 `push_off()` 和 `pop_off()` 来关闭和打开中断。

- 看看 `kernel/sprintf.c` 中的 `snprintf` 函数,了解字符串格式化的思路。把所有锁都命名为 "kmem" 也是可以的。

## Buffer cache

这一半任务与前半部分相互独立;无论你是否完成了前半部分,你都可以做这一半(并通过测试)。

如果多个进程密集使用文件系统,它们很可能会竞争 `bcache.lock`,该锁保护 `kernel/bio.c` 中的磁盘块缓存。bcachetest 创建几个进程,反复读取不同的文件,以在 `bcache.lock` 上产生竞争;它的输出看起来像这样(在你完成本实验之前):
```
$ bcachetest
start test0
test0 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 33035
lock: bcache: #fetch-and-add 16142 #acquire() 65978
--- top 5 contended locks:
lock: virtio_disk: #fetch-and-add 162870 #acquire() 1188
lock: proc: #fetch-and-add 51936 #acquire() 73732
lock: bcache: #fetch-and-add 16142 #acquire() 65978
lock: uart: #fetch-and-add 7505 #acquire() 117
lock: proc: #fetch-and-add 6937 #acquire() 73420
tot= 16142
test0: FAIL
start test1
test1 OK
```
你可能会看到不同的输出,但 bcache 锁的 `acquire` 循环迭代次数会很高。如果你查看 `kernel/bio.c` 中的代码,你会看到 `bcache.lock` 保护了缓存块缓冲区的链表、每个块缓冲区中的引用计数(`b->refcnt`),以及缓存块的标识(`b->dev` 和 `b->blockno`)。

修改块缓存,使得运行 bcachetest 时,bcache 中所有锁的 `acquire` 循环迭代次数都接近于零。理想情况下,块缓存涉及的所有锁的计数之和应该为零,但如果总和小于 500 也是可以的。修改 `bget` 和 `brelse`,使得对 bcache 中不同块的并发查找和释放不太可能在锁上冲突(例如,不必都等待 `bcache.lock`)。你必须保持这样一个不变量:每个块最多缓存一个副本。完成后,你的输出应与下面显示的类似(尽管不完全相同)。确保 usertests 仍然通过。完成后,`make grade` 应通过所有测试。
```
$ bcachetest
start test0
test0 results:
--- lock kmem/bcache stats
lock: kmem: #fetch-and-add 0 #acquire() 32954
lock: kmem: #fetch-and-add 0 #acquire() 75
lock: kmem: #fetch-and-add 0 #acquire() 73
lock: bcache: #fetch-and-add 0 #acquire() 85
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4159
lock: bcache.bucket: #fetch-and-add 0 #acquire() 2118
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4274
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4326
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6334
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6321
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6704
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6696
lock: bcache.bucket: #fetch-and-add 0 #acquire() 7757
lock: bcache.bucket: #fetch-and-add 0 #acquire() 6199
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4136
lock: bcache.bucket: #fetch-and-add 0 #acquire() 4136
lock: bcache.bucket: #fetch-and-add 0 #acquire() 2123
--- top 5 contended locks:
lock: virtio_disk: #fetch-and-add 158235 #acquire() 1193
lock: proc: #fetch-and-add 117563 #acquire() 3708493
lock: proc: #fetch-and-add 65921 #acquire() 3710254
lock: proc: #fetch-and-add 44090 #acquire() 3708607
lock: proc: #fetch-and-add 43252 #acquire() 3708521
tot= 128
test0: OK
start test1
test1 OK
$ usertests
  ...
ALL TESTS PASSED
$
```
请给你所有的锁起以 "bcache" 开头的名字。也就是说,你应该为你每个锁调用 `initlock`,并传入一个以 "bcache" 开头的名字。

减少块缓存中的竞争比 kalloc 更棘手,因为 bcache 缓冲区是真正在进程(以及 CPU)之间共享的。对于 kalloc,人们可以通过给每个 CPU 自己的分配器来消除大部分竞争;这对块缓存行不通。我们建议你用一个每哈希桶一把锁的哈希表在缓存中查找块号。

在某些情况下,你的解决方案存在锁冲突是没关系的:

- 当两个进程并发使用同一个块号时。bcachetest 的 test0 从不这样做。

- 当两个进程并发在缓存中未命中,并且需要找到一个未使用的块来替换时。bcachetest 的 test0 从不这样做。

- 当两个进程并发使用在你划分块和锁的任何方案中相互冲突的块时;例如,如果两个进程使用的块的块号在哈希表中哈希到同一个槽位。bcachetest 的 test0 可能会这样做,具体取决于你的设计,但你应该尝试调整你的方案细节以避免冲突(例如,改变哈希表的大小)。

bcachetest 的 test1 使用比缓冲区数量更多的不同块,并锻炼了大量文件系统代码路径。

以下是一些提示:

- 阅读 xv6 手册中关于块缓存的描述(第 8.1-8.3 节)。

- 可以使用固定数量的桶,不必动态调整哈希表大小。使用素数数量的桶(例如 13)以减少哈希冲突的可能性。

- 在哈希表中查找缓冲区,以及在找不到该缓冲区时为其分配一个条目,这两步必须是原子的。

- 移除所有缓冲区的链表(`bcache.head` 等),改为使用缓冲区上次使用的时间(即 `kernel/trap.c` 中的 ticks)为缓冲区打时间戳。有了这个改变,`brelse` 就不需要获取 bcache 锁,而 `bget` 可以根据时间戳选择最近最少使用(least-recently used)的块。

- 在 `bget` 中串行化逐出(eviction)是可以的(即在查找未命中时选择要复用的缓冲区的那部分 `bget`)。

- 在某些情况下,你的解决方案可能需要持有两把锁;例如,在逐出期间,你可能需要持有 bcache 锁和每桶一把的锁。确保避免死锁(deadlock)。

- 在替换块时,你可能需要把一个 `struct buf` 从一个桶移动到另一个桶,因为新块哈希到了不同的桶。你可能会遇到一个棘手的情况:新块可能哈希到与旧块相同的桶。确保在这种情况下避免死锁。

- 一些调试技巧:先实现桶锁,但保留 `bget` 开头/结尾处对全局 `bcache.lock` 的 acquire/release 来串行化代码。一旦你确信它在没有竞争条件的情况下是正确的,再移除全局锁并处理并发问题。你也可以运行 `make CPUS=1 qemu` 用单个核心进行测试。

## 提交实验

**实验到此完成。** 确保你通过了所有 make grade 测试。如果本实验有问题,别忘了在 `answers-*lab-name*.txt` 中写下你对问题的解答。提交你的更改(包括添加 `answers-*lab-name*.txt`),并在实验目录中键入 `make handin` 来提交你的实验。

### 花费的时间

创建一个新文件 `time.txt`,并在其中放入一个整数,即你花在本实验上的小时数。别忘了 git add 和 git commit 该文件。

### 提交

你将通过[提交网站](https://6828.scripts.mit.edu/2020/handin.py/)提交你的作业。在能够提交任何作业或实验之前,你需要从提交网站申请一次 API key。

在提交你对实验的最终更改之后,键入 `make handin` 来提交你的实验。
```
$ git commit -am "ready to submit my lab"
[util c2e3c8b] ready to submit my lab
 2 files changed, 18 insertions(+), 2 deletions(-)

$ make handin
tar: Removing leading `/' from member names
Get an API key for yourself by visiting https://6828.scripts.mit.edu/2020/handin.py/
Please enter your API key: XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                 Dload  Upload   Total   Spent    Left  Speed
100 79258  100   239  100 79019    853   275k --:--:-- --:--:-- --:--:--  276k
$
```
`make handin` 会将你的 API key 存储在 `*myapi.key*` 中。如果你需要更改 API key,只需删除这个文件并让 `make handin` 重新生成它(`*myapi.key*` 不能包含换行符)。

如果你运行 `make handin` 并且有未提交的更改或未跟踪的文件,你会看到类似如下的输出:
```
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```
检查上面的行,确保你的实验解决方案需要的所有文件都被跟踪,即没有出现在以 `??` 开头的行中。你可以使用 `git add filename` 让 git 跟踪你创建的新文件。

如果 `make handin` 不能正常工作,尝试用 curl 或 Git 命令来解决问题。或者你可以运行 `make tarball`。这会为你生成一个 tar 文件,然后你可以通过我们的[网页界面](https://6828.scripts.mit.edu/2020/handin.py/)上传它。

- 请运行 `make grade` 以确保你的代码通过所有测试

- 在运行 `make handin` 之前,提交任何修改过的源代码

- 你可以在 [https://6828.scripts.mit.edu/2020/handin.py/](https://6828.scripts.mit.edu/2020/handin.py/) 查看你的提交状态并下载已提交的代码

## 可选挑战练习

- 使缓冲区缓存中的查找(lookup)无锁化。提示:使用 gcc 的 `__sync_*` 函数。你如何说服自己你的实现是正确的?