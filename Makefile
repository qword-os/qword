# Directories and files.
SOURCEDIR := ./source

KERNEL    := qword
KERNELBIN := $(KERNEL).bin
KERNELELF := $(KERNEL).elf

CFILES    := $(shell find $(SOURCEDIR) -type f -name '*.c')
ASMFILES  := $(shell find $(SOURCEDIR) -type f -name '*.asm')
REALFILES := $(shell find $(SOURCEDIR) -type f -name '*.real')
BINS      := $(REALFILES:.real=.bin)
OBJ       := $(CFILES:.c=.o) $(ASMFILES:.asm=.o)

# User options.
DBGOUT = no
DBGSYM = no

PREFIX = $(shell pwd)

CC = gcc
AS = nasm

CFLAGS  = -O2 -pipe -Wall -Wextra
LDFLAGS = -O2

# Compile-time options.
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
	-I $(SOURCEDIR)

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

LDHARDFLAGS := $(LDFLAGS) -nostdlib -no-pie

.PHONY: all install uninstall clean

all: $(BINS) $(OBJ)
	$(CC) $(OBJ) $(LDHARDFLAGS) -T $(SOURCEDIR)/linker.ld     -o $(KERNELBIN)
	$(CC) $(OBJ) $(LDHARDFLAGS) -T $(SOURCEDIR)/linker-elf.ld -o $(KERNELELF)

install: all
	mkdir -p $(PREFIX)/boot
	mv $(KERNELBIN) $(PREFIX)/boot/
	mv $(KERNELELF) $(PREFIX)/boot/

uninstall:
	rm -f $(PREFIX)/$(KERNELBIN) $(PREFIX)/boot/$(KERNELELF)

%.o: %.c
	$(CC) $(CHARDFLAGS) -c $< -o $@

%.bin: %.real
	$(AS) $< -I$(SOURCEDIR) -f bin -o $@

%.o: %.asm
	$(AS) $< -I$(SOURCEDIR) -f elf64 -o $@

clean:
	rm -f $(OBJ) $(BINS) $(KERNELBIN) $(KERNELELF)
