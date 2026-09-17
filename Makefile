# Nadir Kernel build.
#
# Toolchain: host gcc/binutils (x86-64) with strict freestanding flags.
# A dedicated x86-64-elf cross-compiler is the planned next step; until
# then CC/LD/OBJCOPY stay overridable so it drops in without edits.
# Requires GNU coreutils (truncate -s %N).

NASM    ?= nasm
CC      ?= gcc
LD      ?= ld
OBJCOPY ?= objcopy
QEMU    ?= qemu-system-x86_64

ARCH_DIR := kernel/arch/x86_64
BUILD    := build
IMG      := nadir.img

CFLAGS := -std=c17 -ffreestanding -nostdlib -fno-builtin \
          -m64 -mno-red-zone -mno-mmx -mno-sse \
          -fno-pic -fno-pie -fno-stack-protector \
          -Wall -Wextra -Werror -O2 -Iinclude

OBJS := $(BUILD)/entry.o $(BUILD)/kmain.o $(BUILD)/console.o

all: $(IMG)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/entry.o: $(ARCH_DIR)/entry.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@

$(BUILD)/kmain.o: kernel/core/kmain.c include/console.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/console.o: $(ARCH_DIR)/console.c include/console.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJS) $(ARCH_DIR)/link.ld
	$(LD) -m elf_x86_64 -T $(ARCH_DIR)/link.ld -o $@ $(OBJS)

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@
	truncate -s %512 $@

# The boot sector needs the kernel size up front, so it builds last.
$(BUILD)/boot.bin: $(ARCH_DIR)/boot.asm $(BUILD)/kernel.bin | $(BUILD)
	SECTORS=$$(( ($$(stat -c%s $(BUILD)/kernel.bin) + 511) / 512 )); \
	$(NASM) -f bin -D KERNEL_SECTORS=$$SECTORS $< -o $@

$(IMG): $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $^ > $@

run: $(IMG)
	$(QEMU) -fda $(IMG)

clean:
	rm -rf $(BUILD) $(IMG)

.PHONY: all run clean
