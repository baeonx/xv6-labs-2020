# 实验: 系统调用

在上一实验你使用系统调用编写了一些工具。在本实验你将为 xv6 添加一些新的系统调用,这将帮助你理解它们的工作原理,并让你接触到 xv6 内核的一些内部机制。在后续实验中你还会添加更多的系统调用。

在开始编码之前,请阅读 [xv6 书籍](../xv6/book-riscv-rev1.pdf)的第 2 章、第 4 章的 4.3 和 4.4 节,以及相关的源文件:

- 系统调用的用户空间代码位于 `user/user.h` 和 `user/usys.pl`。

- 内核空间代码位于 `kernel/syscall.h` 和 `kernel/syscall.c`。

- 与进程相关的代码位于 `kernel/proc.h` 和 `kernel/proc.c`。

要开始实验,请切换到 syscall 分支:

```
  $ git fetch
  $ git checkout syscall
  $ make clean
```

如果运行 `make grade`,你会看到评分脚本无法执行 trace 和 sysinfotest。你的任务是添加必要的系统调用和存根(stub),使它们能够工作。

## 系统调用跟踪(System call tracing)

在这个作业中,你将添加一个系统调用跟踪功能,它可能会在调试后续实验时对你有帮助。你将创建一个新的 trace 系统调用来控制跟踪。它应该接受一个参数:一个整数 "mask"(掩码),其位用来指定要跟踪哪些系统调用。例如,要跟踪 fork 系统调用,程序调用 `trace(1 << SYS_fork)`,其中 SYS_fork 是来自 `kernel/syscall.h` 的系统调用编号。你必须修改 xv6 内核,使得当某个系统调用的编号在掩码中被置位时,在该系统调用即将返回时打印一行。这一行应包含进程 ID、系统调用的名称和返回值;你不需要打印系统调用参数。trace 系统调用应该为调用它的进程以及它随后 fork 的所有子进程启用跟踪,但不应该影响其他进程。

我们提供了一个 trace 用户级程序,它可以在启用跟踪的情况下运行另一个程序(参见 `user/trace.c`)。完成后,你应该看到类似下面的输出:

```
$ trace 32 grep hello README
3: syscall read -> 1023
3: syscall read -> 966
3: syscall read -> 70
3: syscall read -> 0
$
$ trace 2147483647 grep hello README
4: syscall trace -> 0
4: syscall exec -> 3
4: syscall open -> 3
4: syscall read -> 1023
4: syscall read -> 966
4: syscall read -> 70
4: syscall read -> 0
4: syscall close -> 0
$
$ grep hello README
$
$ trace 2 usertests forkforkfork
usertests starting
test forkforkfork: 407: syscall fork -> 408
408: syscall fork -> 409
409: syscall fork -> 410
410: syscall fork -> 411
409: syscall fork -> 412
410: syscall fork -> 413
409: syscall fork -> 414
411: syscall fork -> 415
...
$
```

在上面的第一个示例中,trace 调用 grep 时只跟踪 read 系统调用。32 就是 1<<SYS_read。在第二个示例中,trace 在跟踪所有系统调用的情况下运行 grep;2147583647 的 31 个低位全部置位。在第三个示例中,程序没有被跟踪,因此没有打印跟踪输出。在第四个示例中,usertests 中 forkforkfork 测试的所有后代的 fork 系统调用都被跟踪了。如果你的程序行为如上所示(尽管进程 ID 可能不同),你的解决方案就是正确的。

一些提示:

- 将 `$U/_trace` 添加到 Makefile 的 UPROGS 中。

- 运行 `make qemu`,你会看到编译器无法编译 `user/trace.c`,因为该系统调用的用户空间存根还不存在:需要向 `user/user.h` 添加系统调用原型、向 `user/usys.pl` 添加存根、向 `kernel/syscall.h` 添加系统调用编号。Makefile 调用 perl 脚本 `user/usys.pl`,它生成 `user/usys.S`,即实际的系统调用存根,这些存根使用 RISC-V 的 ecall 指令转换到内核。修复编译问题后,运行 `trace 32 grep hello README`;它会失败,因为你还没有在内核中实现该系统调用。

- 在 `kernel/sysproc.c` 中添加一个 `sys_trace()` 函数来实现新系统调用,方法是在 proc 结构体(参见 `kernel/proc.h`)的一个新变量中记住它的参数。从用户空间获取系统调用参数的函数位于 `kernel/syscall.c` 中,你可以在 `kernel/sysproc.c` 中看到它们的使用示例。

- 修改 fork()(参见 `kernel/proc.c`),把跟踪掩码从父进程复制到子进程。

- 修改 `kernel/syscall.c` 中的 syscall() 函数以打印跟踪输出。你需要添加一个系统调用名称数组供其索引。

## Sysinfo

在这个作业中,你将添加一个系统调用 `sysinfo`,它收集有关正在运行的系统信息。该系统调用接受一个参数:一个指向 struct sysinfo 的指针(参见 `kernel/sysinfo.h`)。内核应填满该结构体的字段:`freemem` 字段应设为空闲内存的字节数,`nproc` 字段应设为状态不是 UNUSED 的进程数。我们提供了一个测试程序 sysinfotest;如果它打印出 "sysinfotest: OK",你就通过了这项作业。

一些提示:

- 将 `$U/_sysinfotest` 添加到 Makefile 的 UPROGS 中。

- 运行 `make qemu`;`user/sysinfotest.c` 将编译失败。按照上一作业相同的步骤添加系统调用 sysinfo。要在 `user/user.h` 中声明 sysinfo() 的原型,你需要预先声明 struct sysinfo 的存在:

```
    struct sysinfo;
    int sysinfo(struct sysinfo *);
```

修复编译问题后,运行 sysinfotest;它会失败,因为你还没有在内核中实现该系统调用。

- sysinfo 需要把 struct sysinfo 复制回用户空间;请查看 sys_fstat()(`kernel/sysfile.c`)和 filestat()(`kernel/file.c`)以了解如何使用 copyout() 实现这一点。

- 要收集空闲内存量,请在 `kernel/kalloc.c` 中添加一个函数。

- 要收集进程数,请在 `kernel/proc.c` 中添加一个函数。

## 提交实验

**本实验到此完成。** 确保你通过了所有 `make grade` 测试。如果本实验有问答题,别忘了把你的答案写在 `answers-*lab-name*.txt` 中。提交你的更改(包括添加 `answers-*lab-name*.txt`)并在实验目录中输入 `make handin` 提交实验。

### 花费的时间

创建一个新文件 `time.txt`,在其中填入一个整数,即你花在本实验上的小时数。别忘了 `git add` 和 `git commit` 这个文件。

### 提交

你将通过[提交网站](https://6828.scripts.mit.edu/2020/handin.py/)提交作业。在提交任何作业或实验之前,你需要先向提交网站申请一次 API key。

在提交实验的最终更改后,输入 `make handin` 提交你的实验。

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

`make handin` 会将你的 API key 存储在 *myapi.key* 中。如果你需要更改 API key,只需删除该文件并让 `make handin` 重新生成(*myapi.key* 中不能包含换行符)。

如果你运行 `make handin` 时存在未提交的更改或未跟踪的文件,你会看到类似下面的输出:

```
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

检查上面的行,确保你的实验解决方案所需的所有文件都被跟踪,即没有出现在以 `??` 开头的行中。你可以使用 `git add filename` 让 git 跟踪你创建的新文件。

如果 `make handin` 不能正常工作,请尝试用 curl 或 Git 命令修复问题。或者你可以运行 `make tarball`,这会为你生成一个 tar 文件,然后你可以通过我们的[网页界面](https://6828.scripts.mit.edu/2020/handin.py/)上传。

- 请运行 `make grade` 以确保你的代码通过所有测试。

- 在运行 `make handin` 之前提交任何修改过的源代码。

- 你可以在 [https://6828.scripts.mit.edu/2020/handin.py/](https://6828.scripts.mit.edu/2020/handin.py/) 查看提交状态并下载已提交的代码。

## 可选挑战练习

- 打印被跟踪系统调用的系统调用参数。

- 计算负载平均值(load average)并通过 sysinfo 导出它。