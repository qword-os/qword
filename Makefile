MAKE = make

PREFIX = $(shell pwd)/root

.PHONY: all iso img clean run run-kvm run-iso run-iso-kvm run-img run-img-singlecore run-img-kvm run-img-kvm-singlecore

all:
	$(MAKE) PREFIX=$(PREFIX) -C root/src

iso: all
	grub-mkrescue -o qword.iso root

img: all
	cp root/src/qloader/qloader.bin ./qword.img
	dd bs=32768 count=32768 if=/dev/zero >> ./qword.img
	truncate --size=-4096 ./qword.img
	echfs-utils ./qword.img format 32768
	./copy-root-to-img.sh root qword.img

QEMU_FLAGS := $(QEMU_FLAGS) \
	-drive file=qword.img,index=0,media=disk,format=raw \
	-drive file=testiso.iso,index=1,media=disk,format=raw \
	-device ahci,id=ahci -drive if=none,id=disk,file=test.img,format=raw -device ide-drive,drive=disk,bus=ahci.0 \
	-net none \
	-serial stdio \
	-d cpu_reset

run: run-img

run-kvm: run-img-kvm

run-iso:
	qemu-system-x86_64 -drive file=qword.iso,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1 -net none -serial stdio

run-iso-kvm:
	qemu-system-x86_64 -drive file=qword.iso,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1 -enable-kvm -net none -serial stdio

run-img:
	qemu-system-x86_64 $(QEMU_FLAGS) -smp sockets=1,cores=4,threads=1

run-img-singlecore:
	qemu-system-x86_64 $(QEMU_FLAGS) -smp sockets=1,cores=1,threads=1

run-img-kvm:
	qemu-system-x86_64 $(QEMU_FLAGS) -smp sockets=1,cores=4,threads=1 -enable-kvm

run-img-kvm-singlecore:
	qemu-system-x86_64 $(QEMU_FLAGS) -smp sockets=1,cores=1,threads=1 -enable-kvm

clean:
	$(MAKE) clean -C root/src
	rm -f qword.iso qword.img
