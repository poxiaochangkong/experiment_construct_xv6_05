#include "types.h"
#include "defs.h"
#include "riscv.h"
void main();
// BSS段的起始和结束符号
extern char sbss[];
extern char ebss[];
// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * 1];

// entry.S jumps here in machine mode on stack0.
//不要清零，否则stack0可能会被清零，并且BSS段似乎提前被qemu清零了
// 一个简单的 memset 函数实现
void
memset(void *dst, int c, unsigned int n)
{
  char *cdst = (char *) dst;
  for (unsigned int i = 0; i < n; i++){
    cdst[i] = c;
  }
}
void
start()
{
  // 1. 设置 mstatus.MPP 为 S-Mode (0b01)
  // mret 指令将会切换到 MPP 指定的模式
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK; // 清除 MPP 位
  x |= MSTATUS_MPP_S;    // 设置 MPP 为 S-Mode
  w_mstatus(x);

  // 2. 设置 mret 的跳转地址为 main 函数
  // mepc 是 M-Mode 的异常程序计数器
  w_mepc((uint64)main);

  // 3. 关闭 M-Mode 下的分页机制
  // 确保 S-Mode 可以自由设置 satp
  w_satp(0);

  // 4. 将所有中断和异常委托给 S-Mode 处理
  // 这是后续实验（中断、系统调用）的关键
  w_medeleg(0xffff);
  w_mideleg((1<<5)|(1<<9));// 委托时钟中断和外部中断

  // 5. 设置 PMP (物理内存保护)，允许 S-Mode 访问所有物理内存
  // 这是一个简化但有效的设置
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // (可选，实验四内容) 配置时钟中断
  intr_on();
  timer_init();
  w_mcounteren(0b111);
  w_stimecmp(r_time() + 10000);

  // 6. 执行 mret 指令，完成从 M-Mode 到 S-Mode 的切换
  // 这会使 CPU 跳转到 mepc (即 main 函数) 并将权限级别降为 S-Mode
  asm volatile("mret");


}