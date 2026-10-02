# 实验: Trap(陷阱)

本实验探究系统调用是如何通过 trap 实现的。你将首先做一个关于栈(stack)的热身练习,然后实现一个用户态 trap 处理的示例。

开始写代码之前,请阅读 [xv6 书籍](../xv6/book-riscv-rev1.pdf) 的第 4 章,以及相关的源文件:

- kernel/trampoline.S:用于在用户空间与内核空间之间切换的相关汇编代码

- kernel/trap.c:处理所有中断(interrupt)的代码

要开始本实验,请切换到 trap 分支:
```
  $ git fetch
  $ git checkout traps
  $ make clean
```

## RISC-V 汇编

理解一点 RISC-V 汇编很重要,你在 6.004 课程中已经接触过。在你的 xv6 仓库中有一个文件 user/call.c。make fs.img 会编译它,并在 user/call.asm 中生成该程序的可读汇编版本。

阅读 call.asm 中 g、f 和 main 这三个函数的代码。RISC-V 的指令手册在[参考页面](../reference.html)上。下面是一些你应当回答的问题(将答案保存在文件 answers-traps.txt 中):

函数调用时参数存放在哪些寄存器中?例如,在 main 调用 printf 时,哪个寄存器存放了 13?

在 main 的汇编代码中,对函数 f 的调用在哪里?对 g 的调用在哪里?(提示:编译器可能内联(inline)函数。)

函数 printf 位于什么地址?

在 main 中 jalr 到 printf 之后,寄存器 ra 中的值是什么?

运行下面的代码。
```
	unsigned int i = 0x00646c72;
	printf("H%x Wo%s", 57616, &i);
```
输出是什么?[这里有一个 ASCII 表](http://web.cs.mun.ca/~michael/c/ascii-table.html),它将字节映射为字符。

输出依赖于 RISC-V 是小端(little-endian)这一事实。如果 RISC-V 改为大端(big-endian),为了得到相同的输出,你应当将 `i` 设置为多少?你需要把 `57616` 改成不同的值吗?

[这里有一个关于小端和大端的描述](http://www.webopedia.com/TERM/b/big_endian.html)以及[一个更随意的描述](http://www.networksorcery.com/enp/ien/ien137.txt)。

在下面的代码中,`'y='` 之后会打印出什么?(注意:答案不是一个具体的值。)为什么会发生这种情况?
```
	printf("x=%d y=%d", 3);
```

## Backtrace

在调试时,backtrace(回溯)通常很有用:它是错误发生点上方的栈中函数调用的列表。

在 kernel/printf.c 中实现一个 backtrace() 函数。在 sys_sleep 中插入对该函数的调用,然后运行调用 sys_sleep 的 bttest。你的输出应当如下所示:
```
backtrace:
0x0000000080002cda
0x0000000080002bb6
0x0000000080002898
```
在 bttest 之后退出 qemu。在你的终端中:地址可能略有不同,但如果你运行 addr2line -e kernel/kernel(或 riscv64-unknown-elf-addr2line -e kernel/kernel),并按如下方式剪切粘贴上述地址:
```
    $ addr2line -e kernel/kernel
    0x0000000080002de2
    0x0000000080002f4a
    0x0000000080002bfc
    Ctrl-D
```
你应该会看到类似这样的内容:
```
    kernel/sysproc.c:74
    kernel/syscall.c:224
    kernel/trap.c:85
```
编译器会在每个栈帧(stack frame)中放入一个帧指针(frame pointer),它保存着调用者帧指针的地址。你的 backtrace 应当利用这些帧指针向上遍历栈,并打印每个栈帧中保存的返回地址。

一些提示:

- 在 kernel/defs.h 中添加 backtrace 的原型(prototype),以便你可以在 sys_sleep 中调用 backtrace。

- GCC 编译器将当前正在执行函数的帧指针存放在寄存器 s0 中。将下面的函数添加到 kernel/riscv.h:
```
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}
```
并在 backtrace 中调用这个函数来读取当前的帧指针。这个函数使用[内联汇编](https://gcc.gnu.org/onlinedocs/gcc/Using-Assembly-Language-with-C.html)来读取 s0。

- 这些[讲义](https://pdos.csail.mit.edu/6.828/2020/lec/l-riscv-slides.pdf)中有栈帧布局的图示。注意:返回地址位于距栈帧帧指针固定的偏移(-8)处,而保存的帧指针位于距帧指针固定的偏移(-16)处。

- Xv6 为 xv6 内核中的每个栈分配一个页(page),地址按 PAGE 对齐。你可以使用 PGROUNDDOWN(fp) 和 PGROUNDUP(fp) 计算栈页的顶部和底部地址(见 kernel/riscv.h)。这些数字有助于 backtrace 终止其循环。

一旦你的 backtrace 正常工作,请在 kernel/printf.c 的 panic 中调用它,这样当内核发生 panic 时你就可以看到内核的 backtrace。

## Alarm

在这个练习中,你将给 xv6 添加一个特性:当一个进程使用 CPU 时间时,周期性地提醒该进程。这对于想要限制自己占用多少 CPU 时间的计算密集型进程很有用,或者对于想要在计算的同时周期性地采取某些动作的进程很有用。更一般地说,你将要实现一种原始形式的用户级中断/故障处理器(fault handler);例如,你可以用类似的东西在应用程序中处理页故障。如果你的解决方案通过 alarmtest 和 usertests,它就是正确的。

你应该添加一个新的 sigalarm(interval, handler) 系统调用。如果应用程序调用 sigalarm(n, fn),那么每经过程序消耗的 n 个 CPU 时间"tick"后,内核就应当使应用程序的函数 fn 被调用。当 fn 返回时,应用程序应当从它中断的地方继续执行。tick 在 xv6 中是一个相当任意的计时单位,由硬件定时器产生中断的频率决定。如果应用程序调用 sigalarm(0, 0),内核就应当停止产生周期性的 alarm 调用。

你会在 xv6 仓库中找到文件 user/alarmtest.c。把它添加到 Makefile 中。在添加 sigalarm 和 sigreturn 系统调用(见下文)之前,它无法正确编译。

alarmtest 在 test0 中调用 sigalarm(2, periodic) 来请求内核每 2 个 tick 强制调用一次 periodic(),然后空转(spin)一段时间。你可以在 user/alarmtest.asm 中看到 alarmtest 的汇编代码,这在调试时可能会很方便。当 alarmtest 产生如下输出且 usertests 也能正确运行时,你的解决方案就正确了:
```
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
$ usertests
...
ALL TESTS PASSED
$
```
当你完成时,你的解决方案只有几行代码,但要做到正确可能很棘手。我们将使用原始仓库中的 alarmtest.c 版本来测试你的代码。你可以修改 alarmtest.c 来帮助调试,但要确保原始的 alarmtest 表明所有测试都通过。

### test0:调用处理器(invoke handler)

首先修改内核,使其跳转到用户空间的 alarm 处理器,这将使 test0 打印出 "alarm!"。先不要担心 "alarm!" 输出之后会发生什么;目前如果你的程序在打印 "alarm!" 之后崩溃,也是可以的。下面是一些提示:

- 你需要修改 Makefile,使 alarmtest.c 被编译为一个 xv6 用户程序。

- 放在 user/user.h 中的正确声明是:
```
    int sigalarm(int ticks, void (*handler)());
    int sigreturn(void);
```
- 更新 user/usys.pl(它生成 user/usys.S)、kernel/syscall.h 和 kernel/syscall.c,以允许 alarmtest 调用 sigalarm 和 sigreturn 系统调用。

- 目前,你的 sys_sigreturn 应当直接返回零。

- 你的 sys_sigalarm() 应当将 alarm 间隔和指向处理器函数的指针存储在 proc 结构体(在 kernel/proc.h 中)的新字段中。

- 你需要跟踪自上次调用一个进程的 alarm 处理器以来经过了多少个 tick(或距离下一次调用还剩多少个 tick);你也需要在 struct proc 中添加一个新字段来记录这一点。你可以在 proc.c 的 allocproc() 中初始化 proc 的字段。

- 每个 tick,硬件时钟都会强制产生一次中断,它在 kernel/trap.c 的 usertrap() 中被处理。

- 只有当存在定时器中断时,你才需要操作进程的 alarm tick;你需要类似这样的代码:
```
    if(which_dev == 2) ...
```
- 仅当进程有一个未处理的定时器时才调用 alarm 函数。注意,用户的 alarm 函数的地址可能为 0(例如,在 user/alarmtest.asm 中,periodic 位于地址 0 处)。

- 你需要修改 usertrap(),使得当进程的 alarm 间隔到期时,用户进程执行处理器函数。当 RISC-V 上的 trap 返回用户空间时,是什么决定了用户空间代码恢复执行的指令地址?

- 如果你告诉 qemu 只使用一个 CPU,那么在 gdb 中查看 trap 会更容易,你可以通过运行下面的命令做到这一点:
```
    make CPUS=1 qemu-gdb
```
- 如果 alarmtest 打印出 "alarm!",你就成功了。

### test1/test2():恢复被中断的代码(resume interrupted code)

alarmtest 很可能在 test0 或 test1 中打印 "alarm!" 之后崩溃,或者 alarmtest(最终)打印出 "test1 failed",或者 alarmtest 在未打印 "test1 passed" 的情况下退出。要修复这个问题,你必须确保:当 alarm 处理器完成后,控制权返回到用户程序最初被定时器中断的那条指令处。你必须确保寄存器内容恢复到中断时的值,这样用户程序就能在 alarm 之后不受干扰地继续执行。最后,你应当在每次 alarm 计数器触发后重新"武装"(re-arm)它,以便处理器被周期性地调用。

作为起点,我们已经为你做了一个设计决策:用户 alarm 处理器在完成时必须调用 sigreturn 系统调用。看看 alarmtest.c 中的 periodic 作为示例。这意味着你可以在 usertrap 和 sys_sigreturn 中添加协作的代码,使用户进程在处理完 alarm 后正确恢复。

一些提示:

- 你的解决方案需要你保存和恢复寄存器——你需要保存和恢复哪些寄存器才能正确恢复被中断的代码?(提示:会很多。)

- 让 usertrap 在定时器触发时在 struct proc 中保存足够的状态,以便 sigreturn 能正确返回到被中断的用户代码。

- 防止对处理器的重入(re-entrant)调用——如果一个处理器尚未返回,内核就不应再次调用它。test2 会测试这一点。

一旦你通过了 test0、test1 和 test2,请运行 usertests,以确保你没有破坏内核的其他部分。

## 提交实验

**本实验到此完成。** 确保你通过所有的 make grade 测试。如果本实验有问题,不要忘记将你对问题的回答写在 answers-*lab-name*.txt 中。提交你的更改(包括添加 answers-*lab-name*.txt),并在实验目录下输入 make handin 来提交你的实验。

### 花费的时间

创建一个新文件 time.txt,在其中放入一个整数,即你花在本实验上的小时数。不要忘记 git add 并 git commit 这个文件。

### 提交

你将使用[提交网站](https://6828.scripts.mit.edu/2020/handin.py/)提交你的作业。在提交任何作业或实验之前,你需要先从提交网站申请一次 API key。

在提交你对实验的最终更改后,输入 make handin 提交你的实验。
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
make handin 会将你的 API key 存储在 *myapi.key* 中。如果你需要更改 API key,只需删除这个文件,然后让 make handin 重新生成它(*myapi.key* 不能包含换行符)。

如果你运行 make handin 时有未提交的更改或未跟踪的文件,你将看到类似下面的输出:
```
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```
检查上面的行,确保你的实验解决方案需要的所有文件都被跟踪,即没有出现在以 ?? 开头的行中。你可以使用 git add filename 让 git 跟踪你创建的新文件。

如果 make handin 不能正常工作,尝试用 curl 或 Git 命令修复问题。或者你可以运行 make tarball。这会为你生成一个 tar 文件,然后你可以通过我们的[web 界面](https://6828.scripts.mit.edu/2020/handin.py/)上传它。

- 请运行 `make grade` 以确保你的代码通过所有测试

- 在运行 `make handin` 之前,提交所有修改过的源代码

- 你可以在 [https://6828.scripts.mit.edu/2020/handin.py/](https://6828.scripts.mit.edu/2020/handin.py/) 查看你提交的状态并下载已提交的代码

## 可选挑战练习

- 在 backtrace() 中打印函数名和行号,而不是数字地址。