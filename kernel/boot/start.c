
void main();
// BSS段的起始和结束符号
extern char sbss[];
extern char ebss[];
// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * 1];

// entry.S jumps here in machine mode on stack0.
//不要清零，否则stack0可能会被清零，并且BSS段似乎提前被qemu清零了
// 一个简单的 memset 函数实现
// void
// memset(void *dst, int c, unsigned int n)
// {
//   char *cdst = (char *) dst;
//   for (unsigned int i = 0; i < n; i++){
//     cdst[i] = c;
//   }
// }
void
start()
{
  // 在进入main函数前，清零BSS段
  //memset(sbss, 1, ebss - sbss);
 
  main();


}