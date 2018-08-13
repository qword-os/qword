MAKE = make

PREFIX = $(shell pwd)/root

.PHONY: all iso clean run run-kvm

all:
	$(MAKE) PREFIX=$(PREFIX) -C root/src

iso: all
	grub-mkrescue -o qword.iso root

run:
	qemu-system-x86_64 -hda qword.iso -smp sockets=1,cores=4,threads=1 -net none -serial stdio

run-kvm:
	qemu-system-x86_64 -hda qword.iso -smp sockets=1,cores=4,threads=1 -enable-kvm -net none -serial stdio

clean:
	$(MAKE) clean -C root/src
	rm -f qword.iso
