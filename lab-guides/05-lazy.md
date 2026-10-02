# 实验: 惰性页分配(Lazy Allocation)

操作系统可以利用页表(page table)硬件实现的众多巧妙技巧之一,就是对用户空间堆内存进行惰性分配。Xv6 应用程序使用 sbrk() 系统调用向内核请求堆内存。在我们提供给您的内核中,sbrk() 会分配物理内存并将其映射到进程的虚拟地址空间中。对于一个大型请求,内核分配和映射内存可能要花费很长时间。例如,考虑一个千兆字节(gigabyte)由 262,144 个 4096 字节的页组成;即使每一页分配都很便宜,这也是数量庞大的分配。此外,有些程序分配的内存比它们实际使用的更多(例如,用于实现稀疏数组),或者在使用内存之前很久就提前分配。为了在这些情况下让 sbrk() 更快地完成,成熟的内核会惰性地分配用户内存。也就是说,sbrk() 不分配物理内存,而只是记住哪些用户地址已被分配,并在用户页表中将这些地址标记为无效。当进程首次尝试使用惰性分配内存的某一页时,CPU 会产生一个页故障(page fault),内核通过分配物理内存、将其清零并映射来处理该故障。在本实验中,你将把这个惰性分配特性添加到 xv6 中。

开始写代码之前,请阅读 [xv6 书籍](../xv6/book-riscv-rev1.pdf) 的第 4 章(特别是 4.6),以及你可能会修改的相关文件:

- kernel/trap.c

- kernel/vm.c

- kernel/sysproc.c

要开始本实验,请切换到 lazy 分支:
```
  $ git fetch
  $ git checkout lazy
  $ make clean
```

## 从 sbrk() 中消除分配

你的第一个任务是删除 sbrk(n) 系统调用实现中的页分配,即 sysproc.c 中的函数 sys_sbrk()。sbrk(n) 系统调用将进程的内存大小增加 n 字节,然后返回新分配区域的起始位置(即旧的大小)。你的新 sbrk(n) 应当只将进程的大小(myproc()->sz)增加 n 并返回旧的大小。它不应分配内存——所以你应该删除对 growproc() 的调用(但你仍然需要增加进程的大小!)。

试着猜测这一修改会产生什么结果:什么会被破坏?

进行这一修改,启动 xv6,并向 shell 输入 echo hi。你应该会看到类似这样的内容:
```
init: starting sh
$ echo hi
usertrap(): unexpected scause 0x000000000000000f pid=3
            sepc=0x0000000000001258 stval=0x0000000000004008
va=0x0000000000004000 pte=0x0000000000000000
panic: uvmunmap: not mapped
```
"usertrap(): ..." 消息来自 trap.c 中的用户 trap 处理器;它捕获了一个它不知道如何处理的异常。请确保你理解为什么会出现这个页故障。"stval=0x0..04008" 表明导致页故障的虚拟地址是 0x4008。

## 惰性分配

修改 trap.c 中的代码,使其对来自用户空间的页故障做出响应:在故障地址处映射一个新分配的物理内存页,然后返回到用户空间让进程继续执行。你应当把代码添加在产生 "usertrap(): ..." 消息的 printf 调用之前。修改你需要修改的任何其他 xv6 内核代码,使 echo hi 能够正常工作。

下面是一些提示:

- 你可以通过在 usertrap() 中检查 r_scause() 是否为 13 或 15,来判断一个故障是否是页故障。

- r_stval() 返回 RISC-V 的 stval 寄存器,其中包含导致页故障的虚拟地址。

- 从 vm.c 中的 uvmalloc() 里借用代码,它是 sbrk() 调用的(通过 growproc())。你需要调用 kalloc() 和 mappages()。

- 使用 PGROUNDDOWN(va) 将故障的虚拟地址向下舍入到页边界。

- uvmunmap() 会发生 panic;修改它,使某些页未映射时不要 panic。

- 如果内核崩溃,在 kernel/kernel.asm 中查找 sepc

- 使用你在 pgtbl 实验中的 vmprint 函数来打印页表的内容。

- 如果你看到错误 "incomplete type proc",请先包含 "spinlock.h",然后再包含 "proc.h"。

如果一切顺利,你的惰性分配代码应该能使 echo hi 正常工作。你应该会得到至少一个页故障(从而触发一次惰性分配),也可能有两个。

## Lazytests 和 Usertests

我们为你提供了 lazytests,这是一个 xv6 用户程序,用于测试一些可能给你的惰性内存分配器带来压力的特定情形。修改你的内核代码,使 lazytests 和 usertests 两者都能全部通过。

- 处理负数形式的 sbrk() 参数。

- 如果进程在高于任何用 sbrk() 分配的虚拟内存地址上发生页故障,则杀死该进程。

- 正确处理 fork() 中父进程到子进程的内存复制。

- 处理这种情况:进程向 read 或 write 这样的系统调用传递了一个来自 sbrk() 的合法地址,但该地址对应的内存尚未被分配。

- 正确处理内存不足:如果页故障处理器中的 kalloc() 失败,则杀死当前进程。

- 处理用户栈下方无效页上的故障。

如果你的内核通过了 lazytests 和 usertests,你的解决方案就可以接受:
```
$  lazytests
lazytests starting
running test lazy alloc
test lazy alloc: OK
running test lazy unmap...
usertrap(): ...
test lazy unmap: OK
running test out of memory
usertrap(): ...
test out of memory: OK
ALL TESTS PASSED
$ usertests
...
ALL TESTS PASSED
$
```

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

- 让惰性页分配与你上一个实验中简单的 copyin 一起工作。