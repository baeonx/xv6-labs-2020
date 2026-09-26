# 实验: Xv6 与 Unix 工具

本实验将让你熟悉 xv6 及其系统调用(system call)。

## 启动 xv6

你可以在 Athena 机器或自己的电脑上完成这些实验。
如果使用自己的电脑,请查看[实验工具页面](../tools.html)获取环境配置提示。

如果使用 Athena,你**必须**使用 x86 机器;也就是说,`uname -a` 的输出应该包含 `i386 GNU/Linux`、`i686 GNU/Linux` 或 `x86_64 GNU/Linux`。
你可以通过 `ssh -X athena.dialup.mit.edu` 登录到公共 Athena 主机。
我们已经在 Athena 上为你配置好了合适的编译器和模拟器。要使用它们,请运行 `add -f 6.828`。你必须在每次登录时运行此命令(或将其添加到 `~/.environment` 文件中)。如果在编译或运行 qemu 时遇到奇怪的错误,请检查你是否已添加课程 locker(course locker)。

获取本实验的 xv6 源码并切换到 util 分支:

```
$ git clone git://g.csail.mit.edu/xv6-labs-2020
Cloning into 'xv6-labs-2020'...
...
$ cd xv6-labs-2020
$ git checkout util
Branch 'util' set up to track remote branch 'util' from 'origin'.
Switched to a new branch 'util'
```

`xv6-labs-2020` 仓库与书中提到的 xv6-riscv 略有不同;它主要是额外增加了一些文件。如果你好奇,可以查看 git log:

```
$ git log
```

你在这个实验及后续实验中需要的文件是通过 [Git](http://www.git-scm.com/) 版本控制系统分发的。上面你切换到了一个分支(`git checkout util`),其中包含专为本实验定制的 xv6 版本。要了解更多关于 Git 的信息,请查看 [Git 用户手册](http://www.kernel.org/pub/software/scm/git/docs/user-manual.html),或者这篇 [CS 视角的 Git 概述](http://eagain.net/articles/git-for-computer-scientists/) 或许对你有用。Git 允许你跟踪对代码所做的更改。例如,如果你完成了某个练习,想对进度做一次检查点,可以通过运行以下命令*提交*(commit)你的更改:

```
$ git commit -am 'my solution for util lab exercise 1'
Created commit 60d2135: my solution for util lab exercise 1
 1 files changed, 1 insertions(+), 0 deletions(-)
$
```

你可以使用 `git diff` 命令跟踪更改。运行 `git diff` 会显示自上次提交以来你对代码所做的更改,而 `git diff origin/util` 会显示相对于初始 xv6-labs-2020 代码的更改。这里的 `origin/xv6-labs-2020` 是你为课程下载的初始代码所在 git 分支的名称。

构建并运行 xv6:

```
$ make qemu
riscv64-unknown-elf-gcc    -c -o kernel/entry.o kernel/entry.S
riscv64-unknown-elf-gcc -Wall -Werror -O -fno-omit-frame-pointer -ggdb -DSOL_UTIL -MD -mcmodel=medany -ffreestanding -fno-common -nostdlib -mno-relax -I. -fno-stack-protector -fno-pie -no-pie   -c -o kernel/start.o kernel/start.c
...
riscv64-unknown-elf-ld -z max-page-size=4096 -N -e main -Ttext 0 -o user/_zombie user/zombie.o user/ulib.o user/usys.o user/printf.o user/umalloc.o
riscv64-unknown-elf-objdump -S user/_zombie > user/zombie.asm
riscv64-unknown-elf-objdump -t user/_zombie | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$/d' > user/zombie.sym
mkfs/mkfs fs.img README  user/xargstest.sh user/_cat user/_echo user/_forktest user/_grep user/_init user/_kill user/_ln user/_ls user/_mkdir user/_rm user/_sh user/_stressfs user/_usertests user/_grind user/_wc user/_zombie
nmeta 46 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 1) blocks 954 total 1000
balloc: first 591 blocks have been allocated
balloc: write bitmap block at sector 45
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$
```

如果在提示符下输入 `ls`,你应该会看到类似下面的输出:

```
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2059
xargstest.sh   2 3 93
cat            2 4 24256
echo           2 5 23080
forktest       2 6 13272
grep           2 7 27560
init           2 8 23816
kill           2 9 23024
ln             2 10 22880
ls             2 11 26448
mkdir          2 12 23176
rm             2 13 23160
sh             2 14 41976
stressfs       2 15 24016
usertests      2 16 148456
grind          2 17 38144
wc             2 18 25344
zombie         2 19 22408
console        3 20 0
```

这些是 mkfs 包含在初始文件系统中的文件;其中大多数是你可以运行的程序。你刚才运行了其中之一:`ls`。

xv6 没有 ps 命令,但如果你输入 `Ctrl-p`,内核会打印每个进程的信息。如果你现在尝试,会看到两行:一行对应 init,一行对应 sh。

退出 qemu 请输入:`Ctrl-a x`。

## 评分与提交程序

你可以运行 `make grade` 用评分程序测试你的解决方案。助教(TA)将使用相同的评分程序为你的实验提交打分。另外,我们还会为实验安排检查确认(check-off)会议(参见[评分政策](../general.html#grading))。

实验代码附带了 GNU Make 规则,使提交更容易。在提交实验的最终更改后,输入 `make handin` 提交你的实验。有关如何提交的详细说明,请参见[下文](#submit)。

## sleep

为 xv6 实现 UNIX 程序 sleep;你的 sleep 应该暂停用户指定数量的 tick。tick 是 xv6 内核定义的一种时间概念,即定时器芯片两次中断之间的时间。你的解决方案应放在文件 `user/sleep.c` 中。

一些提示:

- 在开始编码之前,请阅读 [xv6 书籍](../xv6/book-riscv-rev1.pdf)的第 1 章。

- 查看 `user/` 目录下的其他程序(例如 `user/echo.c`、`user/grep.c` 和 `user/rm.c`),了解如何获取传递给程序的命令行参数。

- 如果用户忘记传递参数,sleep 应该打印一条错误消息。

- 命令行参数以字符串形式传递;你可以使用 atoi 将其转换为整数(参见 `user/ulib.c`)。

- 使用系统调用 sleep。

- 查看 `kernel/sysproc.c` 了解实现 sleep 系统调用的 xv6 内核代码(查找 `sys_sleep`),查看 `user/user.h` 了解从用户程序可调用的 sleep 的 C 定义,查看 `user/usys.S` 了解从用户代码跳转到内核执行 sleep 的汇编代码。

- 确保 main 调用 exit() 以退出你的程序。

- 将你的 sleep 程序添加到 Makefile 的 UPROGS 中;完成后,`make qemu` 将编译你的程序,你就能在 xv6 shell 中运行它了。

- 查阅 Kernighan 和 Ritchie 的著作 *The C programming language (second edition)* (K&R) 来学习 C 语言。

从 xv6 shell 运行该程序:

```
      $ make qemu
      ...
      init: starting sh
      $ sleep 10
      (nothing happens for a little while)
      $
```

如果你的程序如上所示运行时能够暂停,那么你的解决方案就是正确的。运行 `make grade` 查看你是否确实通过了 sleep 测试。

注意,`make grade` 会运行所有测试,包括下面作业的测试。如果你只想运行某一项作业的评分测试,请输入:

```
     $ ./grade-lab-util sleep
```

这将运行与 "sleep" 匹配的评分测试。或者,你也可以输入:

```
     $ make GRADEFLAGS=sleep grade
```

效果相同。

## pingpong

编写一个程序,使用 UNIX 系统调用在一对管道(pipe)上于两个进程之间**乒乓**(ping-pong)传递一个字节,每个方向各用一条管道。父进程应向子进程发送一个字节;子进程应打印 `"<pid>: received ping"`(其中 `<pid>` 是其进程 ID),将字节写入到父进程的管道,然后退出;父进程应从子进程读取该字节,打印 `"<pid>: received pong"`,然后退出。你的解决方案应放在文件 `user/pingpong.c` 中。

一些提示:

- 使用 pipe 创建管道。

- 使用 fork 创建子进程。

- 使用 read 从管道读取,使用 write 向管道写入。

- 使用 getpid 获取调用进程的进程 ID。

- 将程序添加到 Makefile 的 UPROGS 中。

- xv6 上的用户程序可用的库函数有限。你可以在 `user/user.h` 中查看列表;源代码(系统调用之外的)在 `user/ulib.c`、`user/printf.c` 和 `user/umalloc.c` 中。

从 xv6 shell 运行该程序,它应该产生以下输出:

```
    $ make qemu
    ...
    init: starting sh
    $ pingpong
    4: received ping
    3: received pong
    $
```

如果你的程序在两个进程之间交换了一个字节并产生如上所示的输出,那么你的解决方案就是正确的。

## primes

使用管道编写质数筛(prime sieve)的并发版本。这个想法源于 Doug McIlroy,Unix 管道的发明者。[这个页面](http://swtch.com/~rsc/thread/) 中途的图片及其周围文字解释了具体做法。你的解决方案应放在文件 `user/primes.c` 中。

你的目标是使用 pipe 和 fork 建立管道流水线。第一个进程把数字 2 到 35 送入管道。对于每个质数,你要安排创建一个进程,它通过一条管道从左边的邻居读取,再通过另一条管道向右边的邻居写入。由于 xv6 的文件描述符和进程数量有限,第一个进程可以在 35 处停止。

一些提示:

- 注意关闭进程不需要的文件描述符,否则在第一个进程到达 35 之前,你的程序就会耗尽 xv6 的资源。

- 一旦第一个进程到达 35,它应该等待整个流水线终止,包括所有子进程、孙进程等。因此,主 primes 进程应该只在所有输出都已打印、并且所有其他 primes 进程都已退出之后才退出。

- 提示:当管道的写端关闭时,read 返回零。

- 最简单的方法是直接向管道写入 32 位(4 字节)的 int,而不是使用格式化的 ASCII I/O。

- 你应该只在需要时才创建流水线中的进程。

- 将程序添加到 Makefile 的 UPROGS 中。

如果你的解决方案实现了基于管道的筛法并产生以下输出,那就是正确的:

```
    $ make qemu
    ...
    init: starting sh
    $ primes
    prime 2
    prime 3
    prime 5
    prime 7
    prime 11
    prime 13
    prime 17
    prime 19
    prime 23
    prime 29
    prime 31
    $
```

## find

编写 UNIX find 程序的一个简单版本:在一个目录树中查找所有具有特定名称的文件。你的解决方案应放在文件 `user/find.c` 中。

一些提示:

- 查看 `user/ls.c` 了解如何读取目录。

- 使用递归让 find 能够进入子目录。

- 不要递归进入 "." 和 ".."。

- 对文件系统的更改会在 qemu 的多次运行之间持久保留;要获得干净的文件系统,运行 `make clean` 然后再 `make qemu`。

- 你需要使用 C 字符串。请查看 K&R(那本 C 语言书),例如第 5.5 节。

- 注意 `==` 不能像 Python 那样比较字符串。请改用 strcmp()。

- 将程序添加到 Makefile 的 UPROGS 中。

如果产生以下输出(在文件系统包含文件 b 和 a/b 的情况下),你的解决方案就是正确的:

```
    $ make qemu
    ...
    init: starting sh
    $ echo > b
    $ mkdir a
    $ echo > a/b
    $ find . b
    ./b
    ./a/b
    $
```

## xargs

编写 UNIX xargs 程序的一个简单版本:从标准输入读取行,并对每一行运行一个命令,把该行作为参数提供给命令。你的解决方案应放在文件 `user/xargs.c` 中。

下面的示例说明了 xargs 的行为:

```
    $ echo hello too | xargs echo bye
    bye hello too
    $
```

注意这里的命令是 "echo bye",额外参数是 "hello too",组合成的命令是 "echo bye hello too",其输出为 "bye hello too"。

请注意,UNIX 上的 xargs 会做一种优化:一次向命令提供多个参数。我们不要求你做这个优化。为了让 UNIX 上的 xargs 按本实验期望的方式运行,请使用 `-n` 选项并设为 1。例如:

```
    $ echo "1\n2" | xargs -n 1 echo line
    line 1
    line 2
    $
```

一些提示:

- 使用 fork 和 exec 对每行输入调用命令。在父进程中使用 wait 等待子进程完成命令。

- 要读取单行输入,请一次读取一个字符,直到出现换行符 ('\n')。

- `kernel/param.h` 声明了 MAXARG,如果你需要声明 argv 数组,它可能很有用。

- 将程序添加到 Makefile 的 UPROGS 中。

- 对文件系统的更改会在 qemu 的多次运行之间持久保留;要获得干净的文件系统,运行 `make clean` 然后再 `make qemu`。

xargs、find 和 grep 可以很好地组合使用:

```
  $ find . b | xargs grep hello
```

将对 "." 下所有名为 b 的文件运行 "grep hello"。

要测试你的 xargs 解决方案,请运行 shell 脚本 `xargstest.sh`。如果你的解决方案产生以下输出,那就是正确的:

```
  $ make qemu
  ...
  init: starting sh
  $ sh < xargstest.sh
  $ $ $ $ $ $ hello
  hello
  hello
  $ $
```

你可能需要回头修复 find 程序中的 bug。输出中有很多 `$`,是因为 xv6 shell 没有意识到它处理的是来自文件而非控制台的命令,并且会为文件中的每条命令打印一个 `$`。

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

- 编写一个 uptime 程序,使用 uptime 系统调用以 tick 为单位打印运行时间(uptime)。

- 为 find 的名称匹配支持正则表达式。grep.c 对正则表达式有一些基本的支持。

- xv6 shell(`user/sh.c`)只是另一个用户程序,你可以改进它。它是一个极简的 shell,缺少真实 shell 中许多功能。例如:修改 shell,使其在处理来自文件的 shell 命令时不打印 `$`;修改 shell 支持 wait;修改 shell 支持以 ";" 分隔的命令列表;通过实现 "(" 和 ")" 修改 shell 支持子 shell;修改 shell 支持 tab 补全;修改 shell 保留已执行 shell 命令的历史记录;或任何其他你希望你的 shell 能做到的事情。(如果你非常有雄心,可能需要修改内核以支持你所需的内核特性;xv6 支持的功能不多。)