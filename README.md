# xv6-labs-2020

MIT 6.S081 (Fall 2020) 操作系统课程 **xv6 实验仓库**。

本仓库基于 MIT 官方 `xv6-labs-2020`(作者 Frans Kaashoek / Robert Morris /
Russ Cox),包含课程全部 11 个 lab 的**纯净起始代码,不含参考答案**,每个分支
对应一个实验:

| 分支 | 实验 | 说明 |
|------|------|------|
| `util` | Xv6 and Unix utilities | 01-util.md |
| `syscall` | System calls | 02-syscall.md |
| `pgtbl` | Page tables | 03-pgtbl.md |
| `traps` | Trap | 04-traps.md |
| `lazy` | Lazy allocation | 05-lazy.md |
| `cow` | Copy-on-Write fork | 06-cow.md |
| `thread` | Multithreading | 07-thread.md |
| `lock` | Locks | 08-lock.md |
| `fs` | File system | 09-fs.md |
| `mmap` | mmap | 10-mmap.md |
| `net` | Networking | 11-net.md |

每个实验的完整官方说明见 `lab-guides/` 目录。

> 环境适配说明:每个分支在官方代码基础上仅有一处修改——在 `start.c` /
> `riscv.h` 中增加了 PMP(物理内存保护)配置,与 MIT 后期官方代码中的修复
> 完全一致,用于兼容新版 QEMU 8.x。**不包含任何 lab 答案。**

## 构建与运行

```bash
make qemu        # 编译并启动 xv6(退出按 Ctrl-a x)
make grade       # 运行当前 lab 的官方评分脚本
make qemu-gdb    # 启动带 gdb stub 的 qemu(调试)
make clean       # 清理编译产物
```

## 实验流程

```bash
git checkout util        # 实验 1:Unix utilities
# ... 阅读 lab-guides/01-util.md,编写代码 ...
make grade               # 通过评分
git commit -am "lab util: done"
git push                 # 推送到自己的 GitHub 备份
git checkout syscall     # 开始实验 2
```

## 致谢

xv6 代码版权归 Frans Kaashoek, Robert Morris, and Russ Cox
(Copyright 2006-2020),来源于 MIT PDOS
(https://pdos.csail.mit.edu/6.S081/)。