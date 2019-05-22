PREFIX =

DBGOUT = no
DBGSYM = no
ACPI = yes

CC = x86_64-qword-gcc
AS = nasm

CFLAGS = -O2 -pipe -Wall -Wextra

BUILD_TIME := $(shell date)

CHARDFLAGS := \
    -std=gnu99 \
    -masm=intel \
    -fno-pic \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \
    -ffreestanding \
    -fno-stack-protector \
    -I./ \
    -I./acpi/lai/include/

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

ifeq ($(ACPI), yes)
CHARDFLAGS := $(CHARDFLAGS) -D_ACPI_
endif

CHARDFLAGS := $(CHARDFLAGS) -DBUILD_TIME='"$(BUILD_TIME)"'
CLINKFLAGS := -nostdlib -no-pie

REAL_FILES := $(shell find ./ -type f -name '*.real')
BINS := $(REAL_FILES:.real=.bin)
C_FILES := $(shell find ./ -type f -name '*.c')
ASM_FILES := $(shell find ./ -type f -name '*.asm')
OBJ := $(C_FILES:.c=.o) $(ASM_FILES:.asm=.o)
H_FILES := $(shell find ./ -type f -name '*.h')

.PHONY: all install clean

all: kernel.bin

kernel.bin: $(BINS) $(OBJ) $(H_FILES)
	$(CC) $(OBJ) $(CLINKFLAGS) -T ./linker.ld -o $@
	$(CC) $(OBJ) $(CLINKFLAGS) -T ./linker-elf.ld -o kernel.elf

%.o: %.c
	$(CC) $(CFLAGS) $(CHARDFLAGS) -c $< -o $@

%.bin: %.real
	$(AS) $< -f bin -o $@

%.o: %.asm
	$(AS) $< -f elf64 -o $@

install:
	mkdir -p $(PREFIX)/boot
	cp kernel.bin $(PREFIX)/boot
	cp kernel.bin $(PREFIX)/

clean:
	rm -f $(OBJ) $(BINS) kernel.bin
