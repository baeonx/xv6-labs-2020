# MIT 6.S081 (Fall 2020) 实验说明

本目录存放课程官网每个 lab 的完整实验说明(抓取自 pdos.csail.mit.edu/6.S081/2020),已翻译为中文。代码块、命令、文件名与系统调用名均保留原文。

## 实验清单与对应分支

| 编号 | 实验 | 分支 | 说明文件 |
|------|------|------|----------|
| 1 | Xv6 与 Unix 工具 | `util` | 01-util.md |
| 2 | 系统调用 | `syscall` | 02-syscall.md |
| 3 | 页表 | `pgtbl` | 03-pgtbl.md |
| 4 | Trap(陷阱) | `traps` | 04-traps.md |
| 5 | 惰性页分配 | `lazy` | 05-lazy.md |
| 6 | 写时复制 Fork | `cow` | 06-cow.md |
| 7 | 多线程 | `thread` | 07-thread.md |
| 8 | 锁 | `lock` | 08-lock.md |
| 9 | 文件系统 | `fs` | 09-fs.md |
| 10 | mmap | `mmap` | 10-mmap.md |
| 11 | 网络 | `net` | 11-net.md |

## 使用方法

```bash
git checkout util        # 切到第 1 个实验
# 阅读 lab-guides/01-util.md 了解要做什么
# ...写代码...
make grade               # 运行官方评分脚本
```

做完一个 lab 后:

```bash
git commit -am "lab util: done"
git push                 # 推到自己的 GitHub
git checkout syscall     # 切到下一个 lab
```

> 每个分支是独立的官方起始代码,切换后前一个 lab 的修改只保留在其对应分支上。