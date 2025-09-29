        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
        la a0, sbss      # a0 = BSS 段起始地址
        la a1, ebss      # a1 = BSS 段结束地址
bss_loop:
        sd zero, (a0)    # 将 8 字节的 0 写入 a0 指向的地址
        addi a0, a0, 8   # a0 指针增加 8 字节
        blt a0, a1, bss_loop # 如果 a0 < a1，则继续循环
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + ((hartid + 1) * 4096)
        la sp, stack0
        li a0, 1024*4
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # jump to start() in start.c
        call start
spin:
        j spin
