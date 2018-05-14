DEBUG = vga

BUILD_TIME = $(shell date)

CC = x86_64-elf-gcc
AS = nasm

CHARDFLAGS = -std=gnu99 -masm=intel \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \
    -ffreestanding \
    -fno-omit-frame-pointer \
    -I./include/

NASMFORMAT = elf64

ifeq ($(DEBUG), vga)
CHARDFLAGS := $(CHARDFLAGS) -D_KERNEL_VGA_
else ifeq ($(DEBUG), serial)
CHARDFLAGS := $(CHARDFLAGS) -D_KERNEL_SERIAL_
else ifeq ($(DEBUG), both)
CHARDFLAGS := $(CHARDFLAGS) -D_KERNEL_VGA_ -D_KERNEL_SERIAL_
else
$(error variable DEBUG invalid)
endif

CHARDFLAGS := $(CHARDFLAGS) -DBUILD_TIME='"$(BUILD_TIME)"'

REAL_FILES = $(shell find ./real -type f -name '*.real')
BINS = $(REAL_FILES:.real=.bin)
C_FILES = $(shell find ./src -type f -name '*.c')
ASM_FILES = $(shell find ./asm -type f -name '*.asm')
OBJ = $(C_FILES:.c=.o) $(ASM_FILES:.asm=.o)
H_FILES = $(shell find ./include -type f -name '*.h')

CLINKFLAGS = -nostdlib \
    -static-libgcc -lgcc

CFLAGS = -O2 -Wall -Wextra

.PHONY: clean

kernel.bin: $(BINS) $(OBJ) $(H_FILES)
	$(CC) $(OBJ) $(CLINKFLAGS) -T ./linker_syms.ld -o kernel.sym
	./gensyms.sh > kernelsym.asm
	$(AS) kernelsym.asm -f $(NASMFORMAT) -o kernelsym.o
	$(CC) $(OBJ) kernelsym.o $(CLINKFLAGS) -T ./linker.ld -o $@
	rm -f kernel.sym kernelsym.asm kernelsym.o

%.o: %.c
	$(CC) $(CFLAGS) $(CHARDFLAGS) -c $< -o $@

%.bin: %.real
	$(AS) $< -f bin -o $@

%.o: %.asm
	$(AS) $< -f $(NASMFORMAT) -o $@

clean:
	rm -f $(OBJ) $(BINS) kernel.bin
