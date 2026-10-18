#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];
// entry.S needs one stack per CPU.
// 给我留一块 4096 * NCPU 大小的内存，起名叫 stack0
// NCPU是8
// aligned (16)：16字节对齐，要求stack0的起始地址必须是 % 16字节 == 0
// 默认stack0的地址是 0x80000000，stack0以后的地址分配给栈
// 0x0:0x80000000 这段地址范围已经被分配用于访问 I/O 设备。
// scratch area for timer interrupt, one per CPU.
uint64 mscratch0[NCPU * 32];

// assembly code in kernelvec.S for machine-mode timer interrupt.
extern void timervec();

// entry.S jumps here in machine mode on stack0.
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  // 从mstatus（Machine Status Register，机器状态寄存器）读取mstatus寄存器的值到内存
  // MPP（Machine Previous Privilege，机器模式先前的特权级别）
  // 读取特权指令到内存
  unsigned long x = r_mstatus();
  // 把 x 中控制 MPP 的那两位清零
  x &= ~MSTATUS_MPP_MASK;
  // 把 MPP 设置为 S 监管模式
  x |= MSTATUS_MPP_S;
  // 写回mstatus
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  // 把 main 函数的地址（比如 0x80001000），写入 CPU 的 mepc 寄存器
  w_mepc((uint64)main);

  // disable paging for now.
  // satp 是控制“分页（虚拟内存）”的总开关。
  // 关闭分页机制
  // 让 CPU 回到最原始的物理寻址模式，防止cpu误认为是虚拟地址
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  // medeleg = Machine Exception Delegation（机器模式异常委托寄存器）
  // 每一位设置为1，如果发生异常全交由 S(内核模式)处理
  w_medeleg(0xffff);
  // mideleg = Machine Interrupt Delegation（机器模式中断委托寄存器）
  // 每一位设置为1，如果发生中断全交由 S(内核模式)处理
  w_mideleg(0xffff);
  // sie = Supervisor Interrupt Enable（监管模式中断使能寄存器）。
  // 它控制着 S Mode 下，具体允许哪些类型的中断打断当前执行。
  // SIE_SEIE 允许外部设备（如磁盘、网卡）中断
  // SIE_STIE 允许时钟中断（用于进程调度）
  // SIE_SSIE 允许软件触发的中断（用于 CPU 间通信）
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.(允许操作所有物理内存)
  // ull：Unsigned Long Long 64位无符号整数
  // 把 PMP 第 0 号区域的结束地址，设置为物理地址空间的最高处（约 0xFFFFFFFFFFFFFC）
  w_pmpaddr0(0x3fffffffffffffull);
  // 设置0号区域的模式为tor
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  // tp（Thread Pointer，线程指针）是 RISC-V 的一个通用寄存器（x4）。
  // 在操作系统中，它通常被用来存放当前 CPU 核心的编号（hartid）
  // 读取当前 CPU 的硬件线程 ID（hartid）
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  // 从 M Mode 降级到 S Mode，并跳转到 main() 函数开始执行
  // CPU 的下一条指令地址（PC），被强制替换成了 mepc 寄存器里存的值
  asm volatile("mret");
}

// set up to receive timer interrupts in machine mode,
// which arrive at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..3] : space for timervec to save registers.
  // scratch[4] : address of CLINT MTIMECMP register.
  // scratch[5] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &mscratch0[32 * id];
  scratch[4] = CLINT_MTIMECMP(id);
  scratch[5] = interval;
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  w_mie(r_mie() | MIE_MTIE);
}
