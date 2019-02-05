CC=gcc
PREFIX=/usr/local
CFLAGS=-Ofast -Wall -Wextra -pipe

.PHONY: all clean install

all:
	$(CC) $(CFLAGS) echfs-utils.c -o echfs-utils

clean:
	rm -f echfs-utils

install:
	mkdir -p $(PREFIX)/bin
	cp echfs-utils $(PREFIX)/bin
