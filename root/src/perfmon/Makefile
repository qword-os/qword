
.PHONY: install

perfmon: main.c
	x86_64-qword-gcc -o $@ -O2 $<

install:
	mkdir -p $(DESTDIR)/bin
	install perfmon $(DESTDIR)/bin/perfmon

