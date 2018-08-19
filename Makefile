MAKE = make

PREFIX = $(shell pwd)/root

.PHONY: all iso img clean run run-kvm run-img run-img-kvm

all:
	$(MAKE) PREFIX=$(PREFIX) -C root/src

iso: all
	grub-mkrescue -o qword.iso root

img: all
	cp root/src/qloader/qloader.bin ./qword.img
	dd bs=32768 count=32768 if=/dev/zero >> ./qword.img
	truncate --size=-4096 ./qword.img
	echfs-utils ./qword.img format 32768
	./copy-root-to-img.sh

run:
	qemu-system-x86_64 -drive file=qword.iso,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1 -net none -serial stdio

run-kvm:
	qemu-system-x86_64 -drive file=qword.iso,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1 -enable-kvm -net none -serial stdio

run-img:
	qemu-system-x86_64 -drive file=qword.img,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1 -net none -serial stdio

run-img-kvm:
	qemu-system-x86_64 -drive file=qword.img,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1 -enable-kvm -net none -serial stdio

clean:
	$(MAKE) clean -C root/src
	rm -f qword.iso qword.img
