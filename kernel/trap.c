#include "types.h"
#include "defs.h"
#include "riscv.h"
#include "memlayout.h"

extern void kernelvec();  //defined in kernelvec.S

uint ticks;

void
trapinithart(void) //call by main to initialize stvec for this hart
{
  w_stvec((uint64)kernelvec);
}

void
intr_on(void)
{
  // 使能 S-Mode 的全局中断 (SSTATUS.SIE)
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

void
timer_init(void)
{
  // 使能 S-Mode 的时钟中断 (SIE.STIE)
  w_sie(r_sie() | SIE_STIE);
  
  // enable the sstc extension (i.e. stimecmp).
  w_menvcfg(r_menvcfg() | (1L << 63)); 
  
  // allow supervisor to use stimecmp and time.
  w_mcounteren(r_mcounteren() | 2);
}

void
timer_interrupt()//与pdf不同，由于start.c中的设置，如果想要使用SBI，会陷入Smode->Mmode->Smode->...的死循环，所以直接抄xv6
{
  
    ticks++;
    //wakeup(&ticks);
    //printf("tick %d\n", ticks);
  

    // ask for the next timer interrupt. this also clears
    // the interrupt request. 1000000 is about a tenth
    // of a second.
    w_stimecmp(r_time() + 10000000);
}

int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uart_intr();
    } else if(irq == VIRTIO0_IRQ){
      //virtio_disk_intr();   
      panic("virtio_disk_intr not implemented in lab 4");
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    timer_interrupt();
    return 2;
  } else {
    return 0;
  }
}

void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();

  printf("In kerneltrap\n");
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    // interrupt or trap from an unknown source
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
//   if(which_dev == 2 && myproc() != 0)
//     yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

