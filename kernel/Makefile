ARCH = x86_64

BUILD_TIME = $(shell date)

CC = $(ARCH)-elf-gcc

ifeq ($(ARCH), x86_64)
CHARDFLAGS = -std=gnu99 -masm=intel \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -nostdlib \
    -ffreestanding \
    -I./include/ \
    -D__X86_64__ \
    -DBUILD_TIME='"$(BUILD_TIME)"'
NASMFORMAT = elf64
REAL_FILES = $(shell find ./real -type f -name '*.real') \
             $(shell find ./real -type l -name '*.real')
BINS = $(REAL_FILES:.real=.bin)
ASM = nasm
else ifeq ($(ARCH), i386)
CHARDFLAGS = -std=gnu99 -masm=intel \
    -nostdlib \
    -ffreestanding \
    -I./include/ \
    -D__I386__ \
    -DBUILD_TIME='"$(BUILD_TIME)"'
NASMFORMAT = elf32
REAL_FILES = $(shell find ./real -type f -name '*.real') \
             $(shell find ./real -type l -name '*.real')
BINS = $(REAL_FILES:.real=.bin)
ASM = nasm
else
$(error invalid architecture)
endif

C_FILES = $(shell find ./src/common -type f -name '*.c') $(shell find ./src/arch/$(ARCH) -type f -name '*.c') \
          $(shell find ./src/common -type l -name '*.c') $(shell find ./src/arch/$(ARCH) -type l -name '*.c')
H_FILES = $(shell find . -type f -name '*.h') \
          $(shell find . -type l -name '*.h')
ASM_FILES = $(shell find ./src/common -type f -name '*.asm') $(shell find ./src/arch/$(ARCH) -type f -name '*.asm') \
            $(shell find ./src/common -type l -name '*.asm') $(shell find ./src/arch/$(ARCH) -type l -name '*.asm')
OBJ = $(C_FILES:.c=.o) $(ASM_FILES:.asm=.o)

CLINKFLAGS = -nostdlib \
    -static-libgcc -lgcc \
    -T ./linker.ld

CFLAGS = -O2

.PHONY: clean

kernel.bin: $(BINS) $(OBJ) $(H_FILES)
	$(CC) $(OBJ) $(CLINKFLAGS) -o $@

%.o: %.c
	$(CC) $(CHARDFLAGS) $(CFLAGS) -c $< -o $@

%.bin: %.real
	$(ASM) $< -f bin -o $@

%.o: %.asm
	$(ASM) $< -f $(NASMFORMAT) -o $@

clean:
	rm -f $(OBJ) $(BINS) kernel.bin
