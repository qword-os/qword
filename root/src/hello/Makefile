CC=x86_64-qword-gcc
PREFIX=/usr/local

.PHONY: all install clean

all:
	$(CC) hello.c -o hello

install:
	mkdir -p $(PREFIX)/bin
	cp hello $(PREFIX)/bin

clean:
	rm -f hello
