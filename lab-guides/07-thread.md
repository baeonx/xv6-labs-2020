# 实验: 多线程(Multithreading)

本实验将让你熟悉多线程(multithreading)。你将实现一个用户级线程包中的线程切换,使用多个线程来加速一个程序,并实现一个屏障(barrier)。

在编写代码之前,请确保你已阅读[xv6 手册](../xv6/book-riscv-rev1.pdf)中的"第 7 章:Scheduling",并研究了相应的代码。

要开始本实验,请切换到 thread 分支:
```
  $ git fetch
  $ git checkout thread
  $ make clean
```

## Uthread:线程切换

在本练习中,你将为一个用户级线程系统设计上下文切换(context switch)机制,并实现它。作为起点,你的 xv6 中有两个文件 `user/uthread.c` 和 `user/uthread_switch.S`,以及 Makefile 中一条构建 uthread 程序的规则。uthread.c 包含了用户级线程包的大部分代码,以及三个简单的测试线程的代码。该线程包缺少部分用于创建线程和在线程之间切换的代码。

你的任务是制定一个创建线程以及保存/恢复寄存器以在线程间切换的方案,并实现该方案。完成后,`make grade` 应显示你的解决方案通过了 uthread 测试。

完成后,在 xv6 上运行 uthread 时,你应该看到如下输出(三个线程可能以不同顺序启动):
```
$ make qemu
...
$ uthread
thread_a started
thread_b started
thread_c started
thread_c 0
thread_a 0
thread_b 0
thread_c 1
thread_a 1
thread_b 1
...
thread_c 99
thread_a 99
thread_b 99
thread_c: exit after 100
thread_a: exit after 100
thread_b: exit after 100
thread_schedule: no runnable threads
$
```
这段输出来自三个测试线程,每个线程都有一个循环,打印一行后让出 CPU 给其他线程。

然而,在当前没有上下文切换代码的情况下,你将会看到没有输出。

你需要在 `user/uthread.c` 中的 `thread_create()` 和 `thread_schedule()` 中添加代码,并在 `user/uthread_switch.S` 中添加 `thread_switch`。一个目标是确保当 `thread_schedule()` 第一次运行某个给定线程时,该线程在自己的栈上执行传给 `thread_create()` 的函数。另一个目标是确保 `thread_switch` 保存被切换出去的线程的寄存器、恢复被切换到的线程的寄存器,并返回到后一线程的指令中上次离开的地方。你需要决定在哪里保存/恢复寄存器;修改 `struct thread` 来保存寄存器是个好主意。你需要在 `thread_schedule` 中添加对 `thread_switch` 的调用;你可以把任何需要的参数传给 `thread_switch`,但意图是从线程 `t` 切换到 `next_thread`。

一些提示:

- `thread_switch` 只需要保存/恢复被调用者保存的寄存器(callee-save registers)。为什么?

- 你可以在 `user/uthread.asm` 中查看 uthread 的汇编代码,这对调试可能很有用。

- 为了测试你的代码,使用 `riscv64-linux-gnu-gdb` 单步调试 `thread_switch` 可能会有所帮助。你可以这样开始:
```
(gdb) file user/_uthread
Reading symbols from user/_uthread...
(gdb) b uthread.c:60
```
这会在 uthread.c 的第 60 行设置一个断点。该断点可能(也可能不会)在你运行 uthread 之前就被触发。怎么会这样呢?

一旦你的 xv6 shell 运行起来,输入 "uthread",gdb 就会在第 60 行中断。现在你可以输入如下命令来检查 uthread 的状态:
```
  (gdb) p/x *next_thread
```
使用 "x",你可以检查一个内存位置的内容:
```
  (gdb) x/x next_thread->stack
```
你可以这样跳到 `thread_switch` 的开头:
```
   (gdb) b thread_switch
   (gdb) c
```
你可以这样单步执行汇编指令:
```
   (gdb) si
```
gdb 的在线文档在[这里](https://sourceware.org/gdb/current/onlinedocs/gdb/)。

## Using threads

在本任务中,你将使用哈希表(hash table)来探索线程和锁的并行编程。你应该在一台真实的、具有多核的 Linux 或 MacOS 计算机上(不是 xv6,也不是 qemu)完成此任务。大多数现代笔记本电脑都有多核处理器。

本任务使用 UNIX 的 pthread 线程库。你可以通过手册页 `man pthreads` 找到相关信息,也可以在网上查找,例如[这里](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_mutex_lock.html)、[这里](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_mutex_init.html)和[这里](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_create.html)。

文件 `notxv6/ph.c` 包含一个简单的哈希表,如果从单个线程使用是正确的,但从多个线程使用时则不正确。在你的主 xv6 目录(也许是 `~/xv6-labs-2020`)中,输入:
```
$ make ph
$ ./ph 1
```
请注意,为了构建 ph,Makefile 使用的是你操作系统的 gcc,而不是 6.S081 的工具。ph 的参数指定了在哈希表上执行 put 和 get 操作的线程数。运行一小段时间后,`ph 1` 将产生类似如下的输出:
```
100000 puts, 3.991 seconds, 25056 puts/second
0: 0 keys missing
100000 gets, 3.981 seconds, 25118 gets/second
```
你看到的数字可能与这个示例输出相差两倍或更多,这取决于你计算机的速度、是否有多个核心,以及它是否忙于做其他事情。

ph 运行两个基准测试。首先,它通过调用 `put()` 向哈希表添加大量键,并打印每秒的 put 速率。然后,它用 `get()` 从哈希表中获取键。它打印那些本应因 put 而出现在哈希表中却缺失的键的数量(本例中为零),并打印它达到的每秒 get 次数。

你可以通过给 ph 一个大于 1 的参数,让它同时从多个线程使用哈希表。试试 `ph 2`:
```
$ ./ph 2
100000 puts, 1.885 seconds, 53044 puts/second
1: 16579 keys missing
0: 16579 keys missing
200000 gets, 4.322 seconds, 46274 gets/second
```
这个 `ph 2` 输出的第一行表明,当两个线程同时向哈希表添加条目时,它们达到的总速率为每秒 53,044 次插入。这大约是运行 `ph 1` 时单线程速率的两倍。这是一个很好的约 2 倍的"并行加速(parallel speedup)",正如人们可能期望的那样(即两倍多的核心在单位时间内产生两倍多的工作)。

然而,显示 16579 keys missing 的两行表明,大量本应出现在哈希表中的键却不在那里。也就是说,put 本应将那些键添加到哈希表中,但出了点问题。看看 `notxv6/ph.c`,特别是 `put()` 和 `insert()`。

为什么 2 个线程时会有缺失的键,而 1 个线程时却没有?找出 2 个线程时可能导致键缺失的一系列事件。把你的事件序列和简短的解释提交到 `answers-thread.txt` 中。

为了避免这一系列事件,在 `notxv6/ph.c` 的 `put` 和 `get` 中插入 lock 和 unlock 语句,这样在 2 个线程时缺失的键数量始终为 0。相关的 pthread 调用是:
```
pthread_mutex_t lock;            // declare a lock
pthread_mutex_init(&lock, NULL); // initialize the lock
pthread_mutex_lock(&lock);       // acquire lock
pthread_mutex_unlock(&lock);     // release lock
```
当 `make grade` 显示你的代码通过了 ph_safe 测试(该测试要求 2 个线程时缺失的键为零)时,你就完成了。此时未能通过 ph_fast 测试是没关系的。

别忘了调用 `pthread_mutex_init()`。先用 1 个线程测试你的代码,然后用 2 个线程测试。它正确吗(即你是否消除了缺失的键)?双线程版本相对于单线程版本是否实现了并行加速(即单位时间内完成的总工作量更多)?

有些情况下,并发的 `put()` 在哈希表中读取或写入的内存没有重叠,因此不需要锁来互相保护。你能修改 ph.c,利用这种情况为某些 `put()` 获得并行加速吗?提示:每个哈希桶(bucket)一把锁怎么样?

修改你的代码,使某些 put 操作在保持正确性的同时并行运行。当 `make grade` 显示你的代码同时通过了 ph_safe 和 ph_fast 测试时,你就完成了。ph_fast 测试要求两个线程的每秒 put 次数至少是一个线程的 1.25 倍。

## Barrier

在本任务中,你将实现一个[屏障(barrier)](http://en.wikipedia.org/wiki/Barrier_(computer_science)):应用程序中的一个点,所有参与线程必须在这里等待,直到所有其他参与线程也到达该点。你将使用 pthread 条件变量(condition variable),这是一种序列协调技术,类似于 xv6 的 sleep 和 wakeup。

你应该在一台真实的计算机上(不是 xv6,也不是 qemu)完成此任务。

文件 `notxv6/barrier.c` 包含一个损坏的屏障。
```
$ make barrier
$ ./barrier 2
barrier: notxv6/barrier.c:42: thread: Assertion `i == t' failed.
```
参数 2 指定了在屏障上同步的线程数(`barrier.c` 中的 nthread)。每个线程执行一个循环。在每次循环迭代中,线程调用 `barrier()`,然后休眠一个随机的微秒数。断言被触发,是因为一个线程在另一个线程到达屏障之前就离开了屏障。期望的行为是,每个线程都在 `barrier()` 中阻塞,直到所有 nthreads 个线程都调用了 `barrier()`。

你的目标是实现期望的屏障行为。除了你在 ph 任务中见过的锁原语之外,你还需要以下新的 pthread 原语;详情请查看[这里](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_cond_wait.html)和[这里](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_cond_broadcast.html)。
```
pthread_cond_wait(&cond, &mutex);  // go to sleep on cond, releasing lock mutex, acquiring upon wake up
pthread_cond_broadcast(&cond);     // wake up every thread sleeping on cond
```
确保你的解决方案通过 make grade 的 barrier 测试。

`pthread_cond_wait` 在被调用时会释放互斥锁(mutex),并在返回前重新获取该互斥锁。

我们已经给了你 `barrier_init()`。你的任务是实现 `barrier()`,使 panic 不再发生。我们已经为你定义了 `struct barrier`;它的字段供你使用。

有两个问题使你的任务复杂化:

- 你必须处理一连串的 barrier 调用,每一次我们称之为一轮(round)。`bstate.round` 记录当前轮次。每当所有线程都到达屏障时,你应该递增 `bstate.round`。

- 你必须处理某个线程在其他人离开屏障之前就绕回循环的情况。特别是,你会在不同轮次之间复用 `bstate.nthread` 变量。确保一个离开屏障并绕回循环的线程,不会在前一轮还在使用 `bstate.nthread` 时递增它。

用 1 个、2 个和超过 2 个线程测试你的代码。

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

## uthread 的可选挑战

用户级线程包在几个方面与操作系统交互不良。例如,如果一个用户级线程在系统调用中阻塞,另一个用户级线程将不会运行,因为用户级线程调度器不知道它的某个线程已被 xv6 调度器取消调度。再举一个例子,两个用户级线程不会在不同的核心上并发运行,因为 xv6 调度器不知道存在多个可以并行运行的线程。请注意,如果两个用户级线程真正并行运行,由于多处竞争(例如,不同处理器上的两个线程可能并发调用 `thread_schedule`,选择同一个可运行的线程,并在不同的处理器上都运行它),这个实现将无法工作。

有几种方法可以解决这些问题。一种是使用[调度器激活(scheduler activations)](http://en.wikipedia.org/wiki/Scheduler_activations),另一种是为每个用户级线程使用一个内核线程(就像 Linux 内核所做的那样)。在 xv6 中实现其中一种方法。这并不容易做到正确;例如,在为多线程用户进程更新页表时,你将需要实现 TLB shootdown。

为你的线程包添加锁、条件变量、屏障等。