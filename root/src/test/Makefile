AS=nasm
CC=gcc
PREFIX=/usr/local

.PHONY: all install clean

all:
	$(AS) test.asm -f elf64 -o test.o
	$(CC) test.o -nostdlib -no-pie -T linker.ld -o test

install:
	mkdir -p $(PREFIX)/bin
	cp test $(PREFIX)/bin

clean:
	rm -f test.o test
