# 1. 设置交叉编译工具链前缀
#    
TOOLPREFIX = riscv64-unknown-elf-
LDSCRIPT = scripts/kernel.ld
# 2. 定义编译、链接等命令
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJDUMP = $(TOOLPREFIX)objdump

# 3. 设置编译参数 (从 xv6 的 Makefile 中精简而来)
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib
CFLAGS += -I. -Iinclude# 允许 #include "uart.h" 这样的写法

# 4. 设置链接器参数
LDFLAGS = -z max-page-size=4096

# 5. 定义所有需要编译的目标文件 (.o)
OBJS = \
	kernel/boot/entry.o \
	kernel/boot/start.o \
	kernel/main.o \
	kernel/uart.o \
	kernel/printf.o \
	kernel/console.o \
	kernel/mm/kalloc.o \
	kernel/mm/vm.o \

# 6. 定义最终目标：内核文件 kernel
#    它依赖于所有的 .o 文件和一个链接器脚本
kernel.elf: $(OBJS) $(LDSCRIPT)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o kernel.elf $(OBJS)
	$(OBJDUMP) -S kernel.elf > kernel.asm
	$(OBJDUMP) -t kernel.elf > kernel.sym

# 7. 通用规则：如何从 .c 文件生成 .o 文件
#    $< 代表第一个依赖文件 (即 a.c)
#    $@ 代表目标文件 (即 a.o)
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 8. 通用规则：如何从 .S (汇编) 文件生成 .o 文件
%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

# 9. 定义 `clean` 规则，用于删除所有生成的文件，保持目录干净
clean:
	# 使用 find 命令递归删除所有 .o 和 .d 文件
	find . -name "*.o" -delete
	find . -name "*.d" -delete
	# 删除根目录下的其他生成文件
	rm -f kernel.elf kernel.asm kernel.sym

# 10. 定义运行 QEMU 的规则
QEMU = qemu-system-riscv64
QEMUOPTS = -machine virt -bios none -kernel kernel.elf -m 128M -nographic

run: kernel.elf
	$(QEMU) $(QEMUOPTS)

# 告诉 make，clean 和 run 不是文件名，而是动作
.PHONY: clean run