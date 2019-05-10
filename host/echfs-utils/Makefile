CC=cc
PREFIX=/usr/local
CFLAGS=-O3 -Wall -Wextra -pipe

.PHONY: all clean install

echfs-utils:
	$(CC) $(CFLAGS) echfs-utils.c -o echfs-utils

all: echfs-utils

clean:
	rm -f echfs-utils

install:
	mkdir -p $(PREFIX)/bin
	cp echfs-utils $(PREFIX)/bin
