# 实验: mmap

mmap 和 munmap 系统调用允许 UNIX 程序对其地址空间进行精细控制。它们可以用于在进程之间共享内存、将文件映射到进程地址空间，以及作为用户级缺页方案的一部分，例如课堂上讨论的垃圾回收算法。在本实验中，你将把 mmap 和 munmap 添加到 xv6 中，重点关注内存映射文件（memory-mapped files）。

获取本实验的 xv6 源码并切换到 mmap 分支：

```
  $ git fetch
  $ git checkout mmap
  $ make clean
```

手册页（运行 man 2 mmap）给出了 mmap 的如下声明：

```
void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
```

mmap 可以通过多种方式调用，但本实验只需要实现与内存映射文件相关的一小部分功能。你可以假设 addr 始终为零，即由内核决定将文件映射到的虚拟地址。mmap 返回该地址，如果失败则返回 0xffffffffffffffff。length 是要映射的字节数，它可能不等于文件的长度。prot 表示该内存是否应按可读、可写和/或可执行方式映射；你可以假设 prot 是 PROT_READ 或 PROT_WRITE，或者两者都有。flags 要么是 MAP_SHARED，表示对映射内存的修改应写回文件，要么是 MAP_PRIVATE，表示不应写回。你不需要实现 flags 中的其他位。fd 是要映射的文件的打开文件描述符。你可以假设 offset 为零（它是文件中映射的起始点）。

多个进程映射同一个 MAP_SHARED 文件时**不**共享物理页是可以接受的。

munmap(addr, length) 应移除指定地址范围内的 mmap 映射。如果进程修改了内存并且是以 MAP_SHARED 方式映射的，修改应首先写回文件。一次 munmap 调用可能只覆盖一个 mmap 区域的一部分，但你可以假设它要么从区域的起始位置取消映射，要么从结束位置取消映射，要么取消映射整个区域（但不能在区域中间打一个洞）。

你应该实现足够的 mmap 和 munmap 功能，使 mmaptest 测试程序能够工作。如果 mmaptest 没有使用某个 mmap 特性，你就不需要实现该特性。

完成后，你应该看到如下输出：

```
$ mmaptest
mmap_test starting
test mmap f
test mmap f: OK
test mmap private
test mmap private: OK
test mmap read-only
test mmap read-only: OK
test mmap read/write
test mmap read/write: OK
test mmap dirty
test mmap dirty: OK
test not-mapped unmap
test not-mapped unmap: OK
test mmap two files
test mmap two files: OK
mmap_test: ALL OK
fork_test starting
fork_test OK
mmaptest: all tests succeeded
$ usertests
usertests starting
...
ALL TESTS PASSED
$
```

以下是一些提示：

- 首先将 _mmaptest 添加到 UPROGS 中，并添加 mmap 和 munmap 系统调用，以便 user/mmaptest.c 能够编译。目前，只需让 mmap 和 munmap 返回错误。我们已经在 kernel/fcntl.h 中为你定义了 PROT_READ 等。运行 mmaptest，它会在第一次 mmap 调用时失败。

- 惰性地填充页表，以响应缺页故障（page fault）。也就是说，mmap 不应该分配物理内存或读取文件。相反，应该在 usertrap 中（或其调用的代码）的缺页处理代码中完成这些工作，就像惰性页分配实验（lazy page allocation lab）那样。惰性的原因是确保对大文件的 mmap 足够快，并且允许 mmap 一个比物理内存还大的文件。

- 跟踪每个进程的 mmap 映射了哪些内容。定义一个与第 15 讲中描述的 VMA（虚拟内存区域，virtual memory area）相对应的结构体，记录 mmap 创建的虚拟内存范围的地址、长度、权限、文件等。由于 xv6 内核中没有内存分配器，可以声明一个固定大小的 VMA 数组，并在需要时从该数组中分配。大小为 16 应该足够。

- 实现 mmap：在进程的地址空间中找到一个未使用的区域来映射文件，并在进程的已映射区域表中添加一个 VMA。该 VMA 应包含一个指向被映射文件的 struct file 的指针；mmap 应该增加该文件的引用计数，这样当文件被关闭时结构体不会消失（提示：参见 filedup）。运行 mmaptest：第一次 mmap 应该成功，但对 mmap 内存的第一次访问会触发缺页故障并杀死 mmaptest。

- 添加代码，使 mmap 区域中的缺页故障分配一页物理内存，从相关文件中读取 4096 字节到该页中，并将其映射到用户地址空间。使用 readi 读取文件，它接受一个要读取的文件内偏移量参数（但你必须在传递给 readi 之前对该 inode 加锁/解锁）。不要忘记在页上正确设置权限。运行 mmaptest；它应该进行到第一次 munmap。

- 实现 munmap：找到地址范围对应的 VMA 并取消映射指定的页面（提示：使用 uvmunmap）。如果 munmap 移除了先前 mmap 的所有页面，它应该递减相应 struct file 的引用计数。如果某个被取消映射的页面已被修改，并且文件是以 MAP_SHARED 方式映射的，将该页面写回文件。参考 filewrite 以获得灵感。

- 理想情况下，你的实现应该只写回程序实际修改过的 MAP_SHARED 页面。RISC-V 页表项（PTE）中的脏位（D）指示一个页面是否被写入过。然而，mmaptest 不检查未脏页面是否被写回，因此即使不检查 D 位就把页面写回去也是可以的。

- 修改 exit，使其像调用了 munmap 一样取消映射进程的映射区域。运行 mmaptest；mmap_test 应该通过，但 fork_test 可能不会通过。

- 修改 fork，确保子进程拥有与父进程相同的映射区域。不要忘记为 VMA 的 struct file 增加引用计数。在子进程的缺页处理程序中，分配一个新的物理页而不是与父进程共享一页是可以接受的。后者更酷，但需要更多的实现工作。运行 mmaptest；它应该同时通过 mmap_test 和 fork_test。

运行 usertests 以确保一切仍然正常工作。

## 提交实验

**本实验到此完成。** 确保你通过所有 make grade 测试。如果本实验有问题，不要忘记在 answers-*lab-name*.txt 中写下你对问题的回答。提交你的更改（包括添加 answers-*lab-name*.txt），然后在实验目录中输入 make handin 以提交实验。

### 花费的时间

创建一个新文件 time.txt，并在其中写入一个整数，即你花在本实验上的小时数。不要忘记 git add 和 git commit 该文件。

### 提交

你将通过[提交网站](https://6828.scripts.mit.edu/2020/handin.py/)提交你的作业。在提交任何作业或实验之前，你需要从提交网站申请一次 API key。

在提交实验的最终更改之后，输入 make handin 以提交你的实验。

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

make handin 会将你的 API key 存储在 *myapi.key* 中。如果你需要更改 API key，只需删除此文件并让 make handin 重新生成它（*myapi.key* 不能包含换行符）。

如果你运行 make handin 时有未提交的更改或未跟踪的文件，你会看到类似以下的输出：

```
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

检查上面的行，确保你的实验解决方案所需的所有文件都被跟踪，即没有出现在以 ?? 开头的行中。你可以使用 git add filename 让 git 跟踪你创建的新文件。

如果 make handin 不能正常工作，尝试用 curl 或 Git 命令修复问题。或者你可以运行 make tarball。这会为你生成一个 tar 文件，然后你可以通过我们的[网页界面](https://6828.scripts.mit.edu/2020/handin.py/)上传。

- 请运行 `make grade` 以确保你的代码通过所有测试

- 在运行 `make handin` 之前提交任何已修改的源代码

- 你可以在 [https://6828.scripts.mit.edu/2020/handin.py/](https://6828.scripts.mit.edu/2020/handin.py/) 查看你的提交状态并下载已提交的代码

## 可选挑战

- 如果两个进程映射了同一个文件（如在 fork_test 中），让它们共享物理页。你需要为物理页设置引用计数。

- 你的解决方案可能为从 mmap 文件读取的每一页分配一个新的物理页，即使数据也存在于内核内存中的缓冲区缓存（buffer cache）里。修改你的实现以使用那些物理内存，而不是分配新页。这要求文件块的大小与页的大小相同（将 BSIZE 设置为 4096）。你需要将 mmap 的块固定（pin）在缓冲区缓存中。你需要考虑引用计数。

- 消除惰性分配实现与 mmap 文件实现之间的冗余。（提示：为惰性分配区域创建一个 VMA。）

- 修改 exec，为二进制的不同段使用 VMA，从而获得按需分页（on-demand-paged）的可执行文件。这会使程序的启动更快，因为 exec 将不必从文件系统读取任何数据。

- 实现页换出和页换入（page-out 和 page-in）：让内核在物理内存不足时将进程的某些部分移到磁盘上。然后，当进程引用换出的内存时，将其换入。