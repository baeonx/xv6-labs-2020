# 实验: 页表

在本实验你将探索页表(page table)并修改它们,以简化把数据从用户空间复制到内核空间的函数。

在开始编码之前,请阅读 [xv6 书籍](../xv6/book-riscv-rev1.pdf)的第 3 章,以及相关文件:

- `kernel/memlayout.h`,它描述了内存的布局。

- `kernel/vm.c`,它包含了大部分虚拟内存(VM)代码。

- `kernel/kalloc.c`,它包含分配和释放物理内存的代码。

要开始实验,请切换到 pgtbl 分支:

```
  $ git fetch
  $ git checkout pgtbl
  $ make clean
```

## 打印一个页表

为了帮助你了解 RISC-V 页表,也许还能辅助将来的调试,你的第一个任务是编写一个打印页表内容的函数。

定义一个名为 vmprint() 的函数。它应接受一个 pagetable_t 参数,并按照下面描述的格式打印该页表。在 `exec.c` 中、`return argc` 之前插入 `if(p->pid==1) vmprint(p->pagetable)`,以打印第一个进程的页表。如果你通过 make grade 的 pte printout 测试,这项作业即可得满分。

现在,当你启动 xv6 时,它应该打印类似这样的输出,描述第一个进程在刚完成对 init 的 exec() 时其页表的情况:

```
page table 0x0000000087f6e000
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
.. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
.. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
.. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
.. .. ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000
..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
.. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
.. .. ..510: pte 0x0000000021fdd807 pa 0x0000000087f76000
.. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
```

第一行显示传给 vmprint 的参数。之后,每个 PTE 占一行,包括那些指向树中更深层页表页的 PTE。每个 PTE 行都以一定数量的 " .." 缩进,表示它在树中的深度。每个 PTE 行显示该 PTE 在其页表页中的索引、pte 的位,以及从 PTE 中提取出的物理地址。不要打印无效的 PTE。在上面的示例中,顶层页表页有条目 0 和 255 的映射。条目 0 的下一层只映射了索引 0,而该索引 0 的底层映射了条目 0、1 和 2。

你的代码输出的物理地址可能不同于上面显示的。条目数量和虚拟地址应该是相同的。

一些提示:

- 你可以把 vmprint() 放在 `kernel/vm.c` 中。

- 使用文件 `kernel/riscv.h` 末尾的宏。

- 函数 freewalk 可能会给你启发。

- 在 `kernel/defs.h` 中定义 vmprint 的原型,以便你能从 exec.c 调用它。

- 在 printf 调用中使用 %p 打印完整的 64 位十六进制 PTE 和地址,如示例所示。

结合课本中的图 3-4(Fig 3-4)解释 vmprint 的输出。第 0 页包含什么?第 2 页里有什么?在用户模式下运行时,进程能读/写第 1 页映射的内存吗?

## 每个进程一个内核页表

Xv6 有一个单一的内核页表,无论何时在内核中执行都使用它。内核页表是对物理地址的直接映射,因此内核虚拟地址 *x* 映射到物理地址 *x*。Xv6 还为每个进程的用户地址空间各有一个独立的页表,只包含该进程用户内存的映射,从虚拟地址零开始。由于内核页表不包含这些映射,用户地址在内核中无效。因此,当内核需要使用系统调用传入的用户指针(例如传给 write() 的缓冲区指针)时,内核必须先把该指针转换为物理地址。本节和下一节的目标是让内核能够直接解引用用户指针。

你的第一个任务是修改内核,使每个进程在内核中执行时都使用自己的内核页表副本。修改 struct proc 为每个进程维护一个内核页表,并修改调度器(scheduler)在切换进程时切换内核页表。在这一步,每个进程的内核页表应该与现有的全局内核页表完全相同。如果 usertests 运行正确,你就通过了本实验的这一部分。

阅读本作业开始时提到的书中章节和代码;在理解虚拟内存代码工作原理的基础上再修改它,会更容易正确完成。页表设置中的 bug 可能因为缺少映射而导致陷入(trap),可能导致加载和存储影响到物理内存中意外的页,也可能导致从内存中错误的页执行指令。

一些提示:

- 为 struct proc 添加一个字段,用于存放进程的内核页表。

- 为新进程生成内核页表的一个合理方法是实现 kvminit 的修改版本,让它新建一个页表而不是修改 kernel_pagetable。你会想从 allocproc 调用这个函数。

- 确保每个进程的内核页表都有该进程内核栈的映射。在未修改的 xv6 中,所有内核栈都是在 procinit 中建立的。你需要把这项功能的一部分或全部移到 allocproc 中。

- 修改 scheduler(),把进程的内核页表加载到核心的 satp 寄存器中(参考 kvminithart 获取灵感)。别忘了在调用 w_satp() 之后调用 sfence_vma()。

- 当没有进程在运行时,scheduler() 应使用 kernel_pagetable。

- 在 freeproc 中释放进程的内核页表。

- 你需要一种释放页表但不释放叶子物理内存页的方法。

- 调试页表时 vmprint 可能会派上用场。

- 修改 xv6 的函数或添加新函数都是可以的;你可能至少需要在 `kernel/vm.c` 和 `kernel/proc.c` 中这样做。(但是,不要修改 `kernel/vmcopyin.c`、`kernel/stats.c`、`user/usertests.c` 和 `user/stats.c`。)

- 缺少页表映射很可能导致内核遇到页错误(page fault)。它会打印一个包含 `sepc=0x00000000XXXXXXXX` 的错误。你可以通过在 `kernel/kernel.asm` 中搜索 XXXXXXXX 来找出错误发生的位置。

## 简化 copyin/copyinstr

内核的 copyin 函数读取用户指针指向的内存。它通过把这些指针转换为物理地址来做到这一点,而物理地址内核可以直接解引用。它通过在软件中遍历进程页表来完成这种转换。你在本部分实验的任务是,向每个进程的内核页表(上一节中创建的)添加用户映射,使 copyin(以及相关的字符串函数 copyinstr)能够直接解引用用户指针。

把 `kernel/vm.c` 中 copyin 的函数体替换为对 copyin_new(在 `kernel/vmcopyin.c` 中定义)的调用;对 copyinstr 和 copyinstr_new 也做同样的处理。向每个进程的内核页表添加用户地址的映射,使 copyin_new 和 copyinstr_new 能够工作。如果 usertests 运行正确且所有 make grade 测试都通过,你就通过了这项作业。

这个方案依赖于用户虚拟地址范围不与内核用于自身指令和数据的那段虚拟地址范围重叠。Xv6 对用户地址空间使用从零开始的虚拟地址,幸运的是内核的内存起始于更高的地址。然而,这个方案确实把用户进程的最大尺寸限制为小于内核的最低虚拟地址。内核启动后,该地址在 xv6 中是 0xC000000,即 PLIC 寄存器的地址;参见 `kernel/vm.c` 中的 kvminit()、`kernel/memlayout.h` 以及课本中的图 3-4。你需要修改 xv6,以防止用户进程增长到超过 PLIC 地址。

一些提示:

- 先把 copyin() 替换为对 copyin_new 的调用并使其工作,然后再处理 copyinstr。

- 在内核改变进程用户映射的每个位置,用同样的方式改变进程的内核页表。这些位置包括 fork()、exec() 和 sbrk()。

- 别忘了在 userinit 中把第一个进程的用户页表包含到其内核页表中。

- 在进程的内核页表中,用户地址的 PTE 需要什么权限?(置位 PTE_U 的页在内核模式下无法访问。)

- 别忘了上面提到的 PLIC 限制。

Linux 使用了与你所实现的类似的技术。直到几年前,许多内核在用户空间和内核空间都使用同一个每进程页表,同时包含用户和内核地址的映射,以避免在用户空间和内核空间之间切换时切换页表。然而,这种设置允许诸如 Meltdown 和 Spectre 之类的侧信道(side-channel)攻击。

解释为什么 copyin_new() 中的第三个测试 `srcva + len < srcva` 是必要的:给出 srcva 和 len 的值,使前两个测试失败(即它们不会导致返回 -1),但第三个测试为真(从而导致返回 -1)。

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

- 使用超级页(super-page)减少页表中的 PTE 数量。

- 扩展你的解决方案以支持尽可能大的用户程序;也就是说,消除用户程序必须小于 PLIC 的限制。

- 取消用户进程第一页的映射,使解引用空指针会导致错误。你必须把用户文本段(text segment)从 0 开始改为例如 4096 开始。