# 实验: 写时复制(Copy-on-Write)Fork

虚拟内存提供了一层间接性(indirection):内核可以通过将 PTE 标记为无效或只读来截获内存引用,从而产生页故障,并且可以通过修改 PTE 来改变地址的含义。计算机系统中有句俗话:任何系统问题都可以用一层间接性来解决。惰性分配实验提供了一个例子。本实验探索另一个例子:写时复制(copy-on-write)fork。

要开始本实验,请切换到 cow 分支:
```
$ git fetch
$ git checkout cow
$ make clean
```

## 问题

xv6 中的 fork() 系统调用将父进程的所有用户空间内存复制给子进程。如果父进程很大,复制可能需要很长时间。更糟的是,这些工作往往大部分被浪费了;例如,子进程中 fork() 之后紧跟 exec() 会导致子进程丢弃复制来的内存,很可能从来不会使用其中大部分内容。另一方面,如果父进程和子进程都使用某一页,并且其中一方或双方都要写入它,那么复制确实是必要的。

## 解决方案

写时复制(COW)fork() 的目标是推迟为子进程分配和复制物理内存页,直到确实需要这些副本时才进行(如果有那么一天的话)。

COW fork() 只为子进程创建一张页表,其中用户内存的 PTE 指向父进程的物理页。COW fork() 将父进程和子进程中的所有用户 PTE 都标记为不可写。当任一进程试图写入这些 COW 页中的某一页时,CPU 会强制产生一个页故障。内核的页故障处理器检测到这种情况,为发生故障的进程分配一页物理内存,将原始页复制到新页中,并修改故障进程中相关的 PTE 使其指向新页,这一次将 PTE 标记为可写。当页故障处理器返回时,用户进程将能够写入它自己的那一页副本。

COW fork() 使实现用户内存的物理页的释放变得稍微棘手一些。一个给定的物理页可能被多个进程的页表引用,并且只有当最后一个引用消失时,它才应该被释放。

## 实现写时复制

你的任务是在 xv6 内核中实现写时复制 fork。如果你的修改后的内核能够同时成功运行 cowtest 和 usertests 这两个程序,你就完成了。

为了帮助你测试你的实现,我们提供了一个名为 cowtest 的 xv6 程序(源码在 user/cowtest.c)。cowtest 运行各种测试,但即使第一个测试在未修改的 xv6 上也会失败。因此,最初你会看到:
```
$ cowtest
simple: fork() failed
$
```
"simple" 测试分配了超过可用物理内存一半的内存,然后调用 fork()。fork 失败是因为没有足够的空闲物理内存来给子进程一份父进程内存的完整副本。

当你完成时,你的内核应该通过 cowtest 和 usertests 中的所有测试。即:
```
$ cowtest
simple: ok
simple: ok
three: zombie!
ok
three: zombie!
ok
three: zombie!
ok
file: ok
ALL COW TESTS PASSED
$ usertests
...
ALL TESTS PASSED
$
```
下面是一个合理的实施方案。

- 修改 uvmcopy() 将父进程的物理页映射到子进程,而不是分配新页。清除子进程和父进程的 PTE 中的 PTE_W。

- 修改 usertrap() 使其识别页故障。当在 COW 页上发生页故障时,用 kalloc() 分配一个新页,将旧页复制到新页,并将新页安装到 PTE 中并设置 PTE_W。

- 确保每个物理页在最后一个对它引用的 PTE 消失时才被释放——但不要更早。一个不错的方法是:为每个物理页保存一个"引用计数"(reference count),记录引用该页的用户页表数量。当 kalloc() 分配一个页时,将它的引用计数设为 1。当 fork 导致子进程共享该页时,增加该页的引用计数,并且每当任何进程从其页表中删除该页时,减少该页的计数。kfree() 只有在引用计数为零时才应该将页放回空闲列表。把这些计数保存在一个固定大小的整数数组中是可以的。你必须设计一个方案来决定如何索引该数组以及如何选择其大小。例如,你可以用页的物理地址除以 4096 来索引数组,并给数组的元素个数等于 kalloc.c 中 kinit() 放到空闲列表上的任何页的最高物理地址。

- 修改 copyout(),当它遇到 COW 页时,使用与页故障相同的方案。

一些提示:

- 惰性页分配实验很可能已经让你熟悉了与写时复制相关的 xv6 内核代码的大部分内容。但是,你不应该以你的惰性分配解决方案为基础来完成本实验;相反,请按照上面的说明从一份全新的 xv6 开始。

- 有一种办法记录每个 PTE 是否是 COW 映射可能会很有用。你可以使用 RISC-V PTE 中保留给软件使用(RSW,reserved for software)的位来做到这一点。

- usertests 会探索 cowtest 没有测试的场景,所以不要忘记检查两者所有测试都通过。

- 页表标志的一些有用宏和定义在 kernel/riscv.h 的末尾。

- 如果发生 COW 页故障且没有空闲内存,进程应该被杀死。

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

- 修改 xv6,使其同时支持惰性页分配和 COW。

- 衡量你的 COW 实现减少了多少 xv6 复制的字节数和分配的物理页数。找出并利用进一步减少这些数量的机会。