TOOLPREFIX ?= riscv64-linux-gnu-

CC      := $(TOOLPREFIX)gcc
READELF := $(TOOLPREFIX)readelf
NM      := $(TOOLPREFIX)nm
OBJDUMP := $(TOOLPREFIX)objdump
QEMU    ?= qemu-system-riscv64

BUILD  := build
KERNEL := $(BUILD)/kernel.elf
CPUS   ?= 2
CPPFLAGS := -include $(BUILD)/config.h

ARCHFLAGS := -march=rv64gc -mabi=lp64d -mcmodel=medany \
             -mno-relax -msmall-data-limit=0

CFLAGS := $(ARCHFLAGS) -std=gnu11 -Og -g \
          -Wall -Wextra -Werror \
          -ffreestanding -fno-builtin -fno-stack-protector \
          -fno-pie -no-pie -MMD -MP

ASFLAGS := $(ARCHFLAGS) -g \
           -ffreestanding -fno-pie -no-pie -MMD -MP

LDFLAGS := $(ARCHFLAGS) -nostdlib -static -no-pie \
           -Wl,--build-id=none,--no-relax,-T,kernel/kernel.ld \
           -Wl,-Map,$(BUILD)/kernel.map

C_SOURCES := kernel/start.c \
             kernel/sbi.c \
             kernel/main.c \
             kernel/uart.c \
             kernel/printf.c \
             kernel/spinlock.c

ASM_SOURCES := kernel/entry.S

OBJECTS := $(patsubst kernel/%.c,$(BUILD)/%.o,$(C_SOURCES)) \
           $(patsubst kernel/%.S,$(BUILD)/%.o,$(ASM_SOURCES))

DEPS := $(OBJECTS:.o=.d)

.DEFAULT_GOAL := build

.PHONY: build inspect run clean FORCE

build: $(KERNEL)

$(BUILD)/.dir:
	mkdir -p $(BUILD)
	touch $@

FORCE:

$(BUILD)/config.h: FORCE | $(BUILD)/.dir
	@printf '#define NCPU %s\n' '$(CPUS)' > $@.tmp
	@if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

$(BUILD)/%.o: kernel/%.c $(BUILD)/config.h | $(BUILD)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: kernel/%.S $(BUILD)/config.h | $(BUILD)/.dir
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJECTS) kernel/kernel.ld
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

inspect: build
	$(READELF) -h $(KERNEL)
	$(NM) -n $(KERNEL)
	$(OBJDUMP) -d $(KERNEL) > $(BUILD)/kernel.asm

run: build
	$(QEMU) \
		-machine virt \
		-bios default \
		-m 128M \
		-smp $(CPUS) \
		-nographic \
		-kernel $(KERNEL)

clean:
	rm -rf -- $(BUILD)

-include $(DEPS)
