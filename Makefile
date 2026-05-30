# ==========================================================================
# ZenithOS Master Makefile
# Builds bootloader, kernel modules, and combines them into a bootable disk image.
# ==========================================================================

# Cross-compilers and tools
CC = i686-elf-gcc
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy
ASM = nasm

# Build directories
BUILD_DIR = build
BOOT_DIR = boot
KERNEL_DIR = kernel

# Output binaries
STAGE1 = $(BUILD_DIR)/stage1.bin
STAGE2 = $(BUILD_DIR)/stage2.bin
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
BOOT_IMG = $(BUILD_DIR)/zenithboot.img
DISK_ZFS = $(BUILD_DIR)/zenithos.zfs

# Compiler and Assembler flags
CFLAGS = -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-builtin -fstack-protector-all -m32 -I$(KERNEL_DIR)
ASMFLAGS = -f bin

# Kernel object files list
KERNEL_OBJS = $(BUILD_DIR)/kernel.o \
              $(BUILD_DIR)/graphics.o \
              $(BUILD_DIR)/gdt.o \
              $(BUILD_DIR)/idt.o \
              $(BUILD_DIR)/interrupts.o \
              $(BUILD_DIR)/timer.o \
              $(BUILD_DIR)/keyboard.o \
              $(BUILD_DIR)/paging.o \
              $(BUILD_DIR)/ata.o \
              $(BUILD_DIR)/zenithfs.o \
              $(BUILD_DIR)/heap.o \
              $(BUILD_DIR)/task.o \
              $(BUILD_DIR)/syscall.o

.PHONY: all clean run

all: $(BUILD_DIR) $(BOOT_IMG) $(DISK_ZFS)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile Stage 1 Bootloader
$(STAGE1): $(BOOT_DIR)/stage1.asm
	$(ASM) $(ASMFLAGS) $< -o $@

# Compile Stage 2 Bootloader
$(STAGE2): $(BOOT_DIR)/stage2.asm
	$(ASM) $(ASMFLAGS) $< -o $@

# Compile Assembly Interrupt Stubs (ELF 32-bit format)
$(BUILD_DIR)/interrupts.o: $(KERNEL_DIR)/interrupts.asm
	$(ASM) -f elf32 $< -o $@

# Pattern rule to compile kernel C files to objects
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link Kernel ELF
$(KERNEL_ELF): $(KERNEL_OBJS) $(BOOT_DIR)/linker.ld
	$(LD) -T $(BOOT_DIR)/linker.ld $(KERNEL_OBJS) -o $@

# Convert Kernel ELF to raw binary
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# Combine into a bootable 1.44MB floppy image (2880 sectors of 512 bytes)
$(BOOT_IMG): $(STAGE1) $(STAGE2) $(KERNEL_BIN)
	@echo "Creating 1.44MB floppy disk image..."
	dd if=/dev/zero of=$(BOOT_IMG) bs=512 count=2880
	@echo "Writing Stage 1 (MBR) to Sector 1..."
	dd if=$(STAGE1) of=$(BOOT_IMG) bs=512 count=1 conv=notrunc
	@echo "Writing Stage 2 to Sector 2..."
	dd if=$(STAGE2) of=$(BOOT_IMG) bs=512 seek=1 conv=notrunc
	@echo "Writing Kernel to Sector 17..."
	dd if=$(KERNEL_BIN) of=$(BOOT_IMG) bs=512 seek=16 conv=notrunc

# Userland targets
SH_BIN = $(BUILD_DIR)/sh.bin
HELLO_BIN = $(BUILD_DIR)/hello.bin
CALC_BIN = $(BUILD_DIR)/calc.bin
BLASTER_BIN = $(BUILD_DIR)/blaster.bin
EXPLOIT_BIN = $(BUILD_DIR)/exploit.bin
USER_OBJS = $(BUILD_DIR)/crt0.o $(BUILD_DIR)/libc.o

$(BUILD_DIR)/crt0.o: user/crt0.asm
	$(ASM) -f elf32 $< -o $@

$(BUILD_DIR)/libc.o: user/libc.c user/libc.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sh.o: user/sh.c user/libc.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hello.o: user/hello.c user/libc.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/calc.o: user/calc.c user/libc.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/blaster.o: user/blaster.c user/libc.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/exploit.o: user/exploit.c user/libc.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SH_BIN): $(USER_OBJS) $(BUILD_DIR)/sh.o user/linker.ld
	$(LD) -T user/linker.ld $(BUILD_DIR)/crt0.o $(BUILD_DIR)/sh.o $(BUILD_DIR)/libc.o -o $(BUILD_DIR)/sh.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/sh.elf $@

$(HELLO_BIN): $(USER_OBJS) $(BUILD_DIR)/hello.o user/linker.ld
	$(LD) -T user/linker.ld $(BUILD_DIR)/crt0.o $(BUILD_DIR)/hello.o $(BUILD_DIR)/libc.o -o $(BUILD_DIR)/hello.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/hello.elf $@

$(CALC_BIN): $(USER_OBJS) $(BUILD_DIR)/calc.o user/linker.ld
	$(LD) -T user/linker.ld $(BUILD_DIR)/crt0.o $(BUILD_DIR)/calc.o $(BUILD_DIR)/libc.o -o $(BUILD_DIR)/calc.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/calc.elf $@

$(BLASTER_BIN): $(USER_OBJS) $(BUILD_DIR)/blaster.o user/linker.ld
	$(LD) -T user/linker.ld $(BUILD_DIR)/crt0.o $(BUILD_DIR)/blaster.o $(BUILD_DIR)/libc.o -o $(BUILD_DIR)/blaster.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/blaster.elf $@

$(EXPLOIT_BIN): $(USER_OBJS) $(BUILD_DIR)/exploit.o user/linker.ld
	$(LD) -T user/linker.ld $(BUILD_DIR)/crt0.o $(BUILD_DIR)/exploit.o $(BUILD_DIR)/libc.o -o $(BUILD_DIR)/exploit.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/exploit.elf $@

# Create ZenithFS storage image and populate it with files
$(DISK_ZFS): $(SH_BIN) $(HELLO_BIN) $(CALC_BIN) $(BLASTER_BIN) $(EXPLOIT_BIN)
	@echo "Creating and formatting ZenithFS storage image..."
	python3 zfs_tool.py $(DISK_ZFS) format
	@echo "Hello from ZenithFS File System!" > $(BUILD_DIR)/hello.txt
	python3 zfs_tool.py $(DISK_ZFS) add $(BUILD_DIR)/hello.txt hello.txt
	python3 zfs_tool.py $(DISK_ZFS) add $(SH_BIN) sh.bin
	python3 zfs_tool.py $(DISK_ZFS) add $(HELLO_BIN) hello.bin
	python3 zfs_tool.py $(DISK_ZFS) add $(CALC_BIN) calc.bin
	python3 zfs_tool.py $(DISK_ZFS) add $(BLASTER_BIN) blaster.bin
	python3 zfs_tool.py $(DISK_ZFS) add $(EXPLOIT_BIN) exploit.bin


# Run in QEMU Emulator
run: all
	qemu-system-i386 -drive format=raw,if=floppy,file=$(BOOT_IMG) -drive format=raw,if=ide,index=1,media=disk,file=$(DISK_ZFS) -vga std -display cocoa,zoom-to-fit=on -serial stdio

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
