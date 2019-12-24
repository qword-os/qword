# Globals and files to compile.
KERNEL    := qword
KERNELBIN := $(KERNEL).bin
KERNELELF := $(KERNEL).elf

LAI_URL := https://github.com/qword-os/lai.git
LAI_DIR := acpi/lai

CFILES    := $(shell find . -type f -name '*.c')
ASMFILES  := $(shell find . -type f -name '*.asm')
REALFILES := $(shell find . -type f -name '*.real')
BINS      := $(REALFILES:.real=.bin)
OBJ       := $(CFILES:.c=.o) $(ASMFILES:.asm=.o)

# User options.
DBGOUT = no
DBGSYM = no

PREFIX = $(shell pwd)

CC      = gcc
AS      = nasm
OBJCOPY = objcopy
QEMU    = qemu-system-x86_64

CFLAGS    = -O2 -pipe -Wall -Wextra
LDFLAGS   = -O2
QEMUFLAGS = -m 2G -enable-kvm -smp 4

# Flags for compilation.
BUILD_TIME := $(shell date)

CHARDFLAGS := $(CFLAGS) \
	-DBUILD_TIME='"$(BUILD_TIME)"' \
	-std=gnu99                     \
	-masm=intel                    \
	-fno-pic                       \
	-mno-sse                       \
	-mno-sse2                      \
	-mno-red-zone                  \
	-mcmodel=kernel                \
	-ffreestanding                 \
	-fno-stack-protector           \
	-I.                            \
	-I$(LAI_DIR)/include

ifeq ($(DBGOUT), tty)
CHARDFLAGS := $(CHARDFLAGS) -D_DBGOUT_TTY_
else ifeq ($(DBGOUT), qemu)
CHARDFLAGS := $(CHARDFLAGS) -D_DBGOUT_QEMU_
else ifeq ($(DBGOUT), both)
CHARDFLAGS := $(CHARDFLAGS) -D_DBGOUT_TTY_ -D_DBGOUT_QEMU_
endif

ifeq ($(DBGSYM), yes)
CHARDFLAGS := $(CHARDFLAGS) -g -D_DEBUG_
endif

LDHARDFLAGS := $(LDFLAGS) -nostdlib -no-pie -T linker.ld

QEMUHARDFLAGS := $(QEMUFLAGS)          \
	-kernel $(KERNELBIN)               \
	-debugcon stdio                    \
	-net nic,macaddr=00:00:00:11:11:11 \

.PHONY: all prepare build install uninstall clean run

all:
	$(MAKE) prepare
	$(MAKE) build

prepare:
	git clone $(LAI_URL) $(LAI_DIR) || ( cd $(LAI_DIR) && git pull )

build: $(BINS) $(OBJ)
	$(CC) $(OBJ) $(LDHARDFLAGS) -o $(KERNELELF)
	$(OBJCOPY) -O binary $(KERNELELF) $(KERNELBIN)

install: all
	install -d $(DESTDIR)$(PREFIX)/boot
	install $(KERNELBIN) $(DESTDIR)$(PREFIX)/boot/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/boot/$(KERNELBIN)

%.o: %.c
	$(CC) $(CHARDFLAGS) -c $< -o $@

%.bin: %.real
	$(AS) $< -f bin -o $@

%.o: %.asm
	$(AS) $< -f elf64 -o $@

clean:
	rm -f $(OBJ) $(BINS) $(KERNELBIN) $(KERNELELF)

run:
	$(QEMU) $(QEMUHARDFLAGS)
