# Globals and files to compile.
KERNEL    := qword
KERNELBIN := $(KERNEL).bin
KERNELELF := $(KERNEL).elf

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

CFLAGS  = -O2 -pipe -Wall -Wextra
LDFLAGS = -O2

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
	-I.

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

.PHONY: all install uninstall clean run

all: $(BINS) $(OBJ)
	$(CC) $(OBJ) $(LDHARDFLAGS) -o $(KERNELELF)
	$(OBJCOPY) -O binary $(KERNELELF) $(KERNELBIN)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/boot
	cp $(KERNELBIN) $(DESTDIR)$(PREFIX)/boot/
	cp $(KERNELELF) $(DESTDIR)$(PREFIX)/boot/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/$(KERNELBIN) $(DESTDIR)$(PREFIX)/boot/$(KERNELELF)

%.o: %.c
	$(CC) $(CHARDFLAGS) -c $< -o $@

%.bin: %.real
	$(AS) $< -f bin -o $@

%.o: %.asm
	$(AS) $< -f elf64 -o $@

clean:
	rm -f $(OBJ) $(BINS) $(KERNELBIN) $(KERNELELF)

run:
	qemu-system-x86_64 -net nic,macaddr=00:00:00:11:11:11 -kernel qword.bin -debugcon stdio -smp 4
