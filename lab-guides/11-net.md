# 实验: 网络(Networking)

在本实验中，你将为一个网络接口卡（NIC）编写一个 xv6 设备驱动程序。

获取本实验的 xv6 源码并切换到 net 分支：

```
  $ git fetch
  $ git checkout net
  $ make clean
```

## 背景

在编写代码之前，你可能觉得回顾 [xv6 手册](https://pdos.csail.mit.edu/6.S081/2020/xv6/book-riscv-rev1.pdf)中的"第 5 章：中断与设备驱动程序"会有所帮助。

你将使用一个名为 E1000 的网络设备来处理网络通信。对 xv6（以及你编写的驱动程序）来说，E1000 看起来就像一块连接在真实以太网局域网（LAN）上的真实硬件。事实上，你的驱动程序与之通信的 E1000 是 qemu 提供的模拟设备，连接到的局域网也是由 qemu 模拟的。在这个模拟局域网上，xv6（"guest"）的 IP 地址是 10.0.2.15。qemu 还安排运行 qemu 的计算机以 IP 地址 10.0.2.2 出现在局域网上。当 xv6 使用 E1000 向 10.0.2.2 发送数据包时，qemu 会将该数据包传递给运行 qemu 的（真实）计算机上的相应应用程序（"host"）。

你将使用 QEMU 的"用户态网络栈（user-mode network stack）"。QEMU 的文档在[这里](https://www.qemu.org/docs/master/system/net.html#using-the-user-mode-network-stack)有更多关于用户态网络栈的介绍。我们已经更新了 Makefile 以启用 QEMU 的用户态网络栈和 E1000 网卡。

Makefile 配置 QEMU 将所有传入和传出的数据包记录到实验目录下的 packets.pcap 文件中。查看这些记录可能有助于确认 xv6 是否按预期发送和接收数据包。要显示记录的数据包：

```
tcpdump -XXnr packets.pcap
```

我们为这个实验在 xv6 仓库中添加了一些文件。文件 kernel/e1000.c 包含 E1000 的初始化代码，以及用于发送和接收数据包的空函数，你将填充这些函数。kernel/e1000_dev.h 包含 E1000 定义的寄存器与标志位的定义，在 Intel E1000 [《软件开发者手册》](../readings/8254x_GBe_SDM.pdf)中有描述。kernel/net.c 和 kernel/net.h 包含一个实现了 [IP](https://en.wikipedia.org/wiki/Internet_Protocol)、[UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol) 和 [ARP](https://en.wikipedia.org/wiki/Address_Resolution_Protocol) 协议的简单网络栈。这些文件还包含一个用于存放数据包的灵活数据结构，称为 mbuf。最后，kernel/pci.c 包含在 xv6 启动时在 PCI 总线上搜索 E1000 网卡的代码。

## 你的任务

你的任务是完成 kernel/e1000.c 中的 e1000_transmit() 和 e1000_recv()，使驱动程序能够发送和接收数据包。当 make grade 显示你的解决方案通过所有测试时，你就完成了。

编写代码时，你会发现需要参考 E1000 [《软件开发者手册》](../readings/8254x_GBe_SDM.pdf)。以下章节尤其有帮助：

- 第 2 章是必需的，给出了整个设备的概览。

- 第 3.2 章给出了数据包接收的概览。

- 第 3.3 章与第 3.4 章一起给出了数据包发送的概览。

- 第 13 章给出了 E1000 所用寄存器的概览。

- 第 14 章可能帮助你理解我们提供的初始化代码。

浏览 E1000 [《软件开发者手册》](../readings/8254x_GBe_SDM.pdf)。该手册涵盖了多个紧密相关的以太网控制器。QEMU 模拟的是 82540EM。现在浏览一下第 2 章，感受一下这个设备。要编写你的驱动程序，你需要熟悉第 3 章和第 14 章，以及第 4.1 章（不包括 4.1 的子章节）。你还需要将第 13 章作为参考。其他章节主要涵盖你的驱动程序不需要与之交互的 E1000 组件。一开始不要担心细节；只需感受一下文档的结构，以便你以后能快速找到内容。E1000 有很多高级特性，其中大部分你可以忽略。完成本实验只需要一小部分基本特性。

我们在 e1000.c 中提供的 e1000_init() 函数配置 E1000 从 RAM 中读取要发送的数据包，并将接收到的数据包写入 RAM。这种技术称为 DMA，即直接内存访问（direct memory access），指的是 E1000 硬件直接对 RAM 写入和读取数据包这一事实。

因为突发的数据包可能比驱动程序处理它们的速度更快，e1000_init() 为 E1000 提供了多个缓冲区，E1000 可以向其中写入数据包。E1000 要求这些缓冲区由 RAM 中的一个"描述符（descriptor）"数组来描述；每个描述符包含一个 RAM 地址，E1000 可以在该地址写入接收到的数据包。struct rx_desc 描述了描述符的格式。描述符数组被称为接收环（receive ring），或称接收队列（receive queue）。之所以说是环形，是因为当网卡或驱动程序到达数组末尾时，它会回绕到开头。e1000_init() 使用 mbufalloc() 分配 mbuf 数据包缓冲区，供 E1000 进行 DMA 写入。还有一个发送环（transmit ring），驱动程序将要 E1000 发送的数据包放入其中。e1000_init() 将两个环配置为大小为 RX_RING_SIZE 和 TX_RING_SIZE。

当 net.c 中的网络栈需要发送数据包时，它会调用 e1000_transmit()，并传入一个持有待发送数据包的 mbuf。你的发送代码必须将指向数据包数据的指针放到 TX（发送）环的一个描述符中。struct tx_desc 描述了描述符的格式。你需要确保每个 mbuf 最终都被释放，但只能在 E1000 完成数据包发送之后（E1000 会在描述符中设置 E1000_TXD_STAT_DD 位来表示这一点）。

当 E1000 从以太网接收到每个数据包时，它首先将数据包 DMA 到下一个 RX（接收）环描述符所指向的 mbuf，然后产生一个中断。你的 e1000_recv() 代码必须扫描 RX 环，并通过调用 net_rx() 将每个新数据包的 mbuf 交付给（net.c 中的）网络栈。然后你需要分配一个新的 mbuf 并将其放入描述符中，这样当 E1000 再次到达 RX 环中的那个位置时，它会找到一个可以 DMA 新数据包的新的缓冲区。

除了读写 RAM 中的描述符环之外，你的驱动程序还需要通过 E1000 的内存映射控制寄存器（memory-mapped control registers）与 E1000 交互，以检测何时有接收到的数据包可用，并告知 E1000 驱动程序已在一些 TX 描述符中填入了要发送的数据包。全局变量 regs 持有指向 E1000 第一个控制寄存器的指针；你的驱动程序可以通过将 regs 作为数组索引来访问其他寄存器。你尤其需要使用索引 E1000_RDT 和 E1000_TDT。

要测试你的驱动程序，在一个窗口中运行 make server，在另一个窗口中运行 make qemu，然后在 xv6 中运行 nettests。nettests 中的第一个测试尝试向宿主操作系统发送一个 UDP 数据包，目标地址是 make server 运行的程序。如果你还没有完成实验，E1000 驱动程序不会真正发送数据包，那么什么也不会发生。

完成实验后，E1000 驱动程序会发送数据包，qemu 会将其交付给你的宿主计算机，make server 会看到它，它会发送一个响应数据包，然后 E1000 驱动程序和 nettests 会看到响应数据包。在宿主发送响应之前，它会向 xv6 发送一个"ARP"请求数据包，以查明它的 48 位以太网地址，并期望 xv6 以 ARP 应答响应。一旦你完成了 E1000 驱动程序的编写，kernel/net.c 将处理这个问题。如果一切顺利，nettests 会打印 testing ping: OK，make server 会打印 a message from xv6!。

tcpdump -XXnr packets.pcap 应该产生类似如下的输出：

```
reading from file packets.pcap, link-type EN10MB (Ethernet)
15:27:40.861988 IP 10.0.2.15.2000 > 10.0.2.2.25603: UDP, length 19
        0x0000:  ffff ffff ffff 5254 0012 3456 0800 4500  ......RT..4V..E.
        0x0010:  002f 0000 0000 6411 3eae 0a00 020f 0a00  ./....d.>.......
        0x0020:  0202 07d0 6403 001b 0000 6120 6d65 7373  ....d.....a.mess
        0x0030:  6167 6520 6672 6f6d 2078 7636 21         age.from.xv6!
15:27:40.862370 ARP, Request who-has 10.0.2.15 tell 10.0.2.2, length 28
        0x0000:  ffff ffff ffff 5255 0a00 0202 0806 0001  ......RU........
        0x0010:  0800 0604 0001 5255 0a00 0202 0a00 0202  ......RU........
        0x0020:  0000 0000 0000 0a00 020f                 ..........
15:27:40.862844 ARP, Reply 10.0.2.15 is-at 52:54:00:12:34:56, length 28
        0x0000:  ffff ffff ffff 5254 0012 3456 0806 0001  ......RT..4V....
        0x0010:  0800 0604 0002 5254 0012 3456 0a00 020f  ......RT..4V....
        0x0020:  5255 0a00 0202 0a00 0202                 RU........
15:27:40.863036 IP 10.0.2.2.25603 > 10.0.2.15.2000: UDP, length 17
        0x0000:  5254 0012 3456 5255 0a00 0202 0800 4500  RT..4VRU......E.
        0x0010:  002d 0000 0000 4011 62b0 0a00 0202 0a00  .-....@.b.......
        0x0020:  020f 6403 07d0 0019 3406 7468 6973 2069  ..d.....4.this.i
        0x0030:  7320 7468 6520 686f 7374 21              s.the.host!
```

你的输出会略有不同，但它应该包含字符串 "ARP, Request"、"ARP, Reply"、"UDP"、"a.message.from.xv6" 和 "this.is.the.host"。

nettests 还会执行一些其他测试，最终会通过（真实的）互联网向 Google 的一个域名服务器发送 DNS 请求。你应该确保你的代码通过所有这些测试，之后你应该看到如下输出：

```
$ nettests
nettests running on port 25603
testing ping: OK
testing single-process pings: OK
testing multi-process pings: OK
testing DNS
DNS arecord for pdos.csail.mit.edu. is 128.52.129.126
DNS OK
all tests passed.
```

你应该确保 make grade 也认可你的解决方案通过了。

## 提示

首先在 e1000_transmit() 和 e1000_recv() 中添加打印语句，然后运行 make server 和（在 xv6 中）nettests。你应该能从打印语句中看到 nettests 生成了对 e1000_transmit 的调用。

实现 e1000_transmit 的一些提示：

- 首先通过读取 E1000_TDT 控制寄存器，向 E1000 询问它期望下一个数据包的 TX 环索引。

- 然后检查环是否溢出。如果 E1000_TDT 索引的描述符中没有设置 E1000_TXD_STAT_DD，说明 E1000 尚未完成对应的上一次发送请求，因此返回错误。

- 否则，使用 mbuffree() 释放上一次从该描述符发送的 mbuf（如果有的话）。

- 然后填充描述符。m->head 指向数据包在内存中的内容，m->len 是数据包长度。设置必要的 cmd 标志（查看 E1000 手册第 3.3 章），并保存指向该 mbuf 的指针以便稍后释放。

- 最后，通过将 E1000_TDT 加一并取模 TX_RING_SIZE 来更新环位置。

- 如果 e1000_transmit() 成功地将 mbuf 添加到环中，返回 0。失败时（例如没有可用的描述符来发送该 mbuf），返回 -1，以便调用者知道要释放该 mbuf。

实现 e1000_recv 的一些提示：

- 首先通过获取 E1000_RDT 控制寄存器并加一取模 RX_RING_SIZE，向 E1000 询问下一个等待接收的数据包（如果有的话）所在的环索引。

- 然后通过检查描述符状态部分中的 E1000_RXD_STAT_DD 位来判断是否有新的数据包可用。如果没有，就停止。

- 否则，将 mbuf 的 m->len 更新为描述符中报告的长度。使用 net_rx() 将该 mbuf 交付给网络栈。

- 然后使用 mbufalloc() 分配一个新的 mbuf，替换刚交给 net_rx() 的那个。将它的数据指针（m->head）写入描述符。将描述符的状态位清零。

- 最后，将 E1000_RDT 寄存器更新为已处理的最后一个环描述符的索引。

- e1000_init() 用 mbuf 初始化了 RX 环，你会想看看它是如何做的，也许可以借用代码。

- 在某个时刻，累计到达的数据包总数会超过环的大小（16）；确保你的代码能够处理这种情况。

你需要使用锁来应对 xv6 可能从多个进程使用 E1000，或者当中断到达时可能在某个内核线程中使用 E1000 的情况。

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

## 可选挑战：

下面挑战练习的一些好处只能在真实的高性能硬件上衡量/测试，也就是基于 x86 的计算机。

- 在本实验中，网络栈使用中断来处理入站（ingress）数据包的处理，但不处理出站（egress）数据包。一个更复杂的策略是在软件中对出站数据包进行排队，并且每次只向 NIC 提供有限数量的数据包。然后你可以依靠 TX 中断来补充发送环。使用这种技术，就可以对不同类型的出站流量进行优先级排序。

- 提供的网络代码只部分支持 ARP。实现一个完整的 [ARP 缓存](https://tools.ietf.org/html/rfc826)，并将其接入 net_tx_eth()。

- E1000 支持多个 RX 和 TX 环。配置 E1000 为每个核提供一对环，并修改你的网络栈以支持多个环。这样做有可能提高你的网络栈能够支持的吞吐量，并减少锁竞争。但难以测试/衡量。

- sockrecvudp() 使用单链表来查找目标套接字，这效率低下。尝试使用哈希表和 RCU 来提高性能。但严肃的实现难以测试/衡量。

- [ICMP](https://tools.ietf.org/html/rfc792) 可以提供网络流失败的通知。检测这些通知，并通过套接字系统调用接口将它们作为错误传播出去。

- E1000 支持多种无状态硬件卸载（offload），包括校验和计算、RSC 和 GRO。使用其中一种或多种卸载来提高网络栈的吞吐量。但难以测试/衡量。

- 本实验中的网络栈容易遭受接收活锁（receive livelock）。利用课堂内容和阅读作业中的材料，设计并实现一个解决方案来修复它。但难以测试。

- 为 xv6 实现一个 UDP 服务器。

- 实现一个最小化的 TCP 栈并下载一个网页。

如果你尝试解决某个挑战问题，无论是否与网络相关，请告知课程工作人员！