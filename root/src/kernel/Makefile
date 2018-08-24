PREFIX = 

DEBUG = vga

CC = gcc
AS = nasm

CFLAGS = -O2 -pipe -Wall -Wextra

BUILD_TIME := $(shell date)

CHARDFLAGS := -std=gnu99 -masm=intel \
    -fno-pic \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \
    -ffreestanding \
    -fno-omit-frame-pointer \
	-fno-stack-protector \
    -I./include/

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
CLINKFLAGS := -nostdlib -no-pie

REAL_FILES := $(shell find ./real -type f -name '*.real')
BINS := $(REAL_FILES:.real=.bin)
C_FILES := $(shell find ./src -type f -name '*.c')
ASM_FILES := $(shell find ./asm -type f -name '*.asm')
OBJ := $(C_FILES:.c=.o) $(ASM_FILES:.asm=.o)
H_FILES := $(shell find ./include -type f -name '*.h')

.PHONY: all install clean

all: kernel.bin

kernel.bin: $(BINS) $(OBJ) $(H_FILES)
	@printf "  LD [s]  kernel.sym\n"
	@$(CC) $(OBJ) $(CLINKFLAGS) -T ./linker_syms.ld -o kernel.sym
	@printf "  GENSYMS kernelsym.asm\n"
	@./gensyms.sh > kernelsym.asm
	@printf "  AS      kernelsym.o\n"
	@$(AS) kernelsym.asm -f elf64 -o kernelsym.o
	@printf "  LD      $@\n"
	@$(CC) $(OBJ) kernelsym.o $(CLINKFLAGS) -T ./linker.ld -o $@
	@printf "  CLEANUP\n"
	@rm -f kernel.sym kernelsym.asm kernelsym.o

%.o: %.c
	@printf "  CC      $@\n"
	@$(CC) $(CFLAGS) $(CHARDFLAGS) -c $< -o $@

%.bin: %.real
	@printf "  AS [r]  $@\n"
	@$(AS) $< -f bin -o $@

%.o: %.asm
	@printf "  AS      $@\n"
	@$(AS) $< -f elf64 -o $@

install:
	mkdir -p $(PREFIX)/boot
	cp kernel.bin $(PREFIX)/boot
	cp kernel.bin $(PREFIX)/

clean:
	rm -f $(OBJ) $(BINS) kernel.bin
