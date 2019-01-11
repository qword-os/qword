PATH := $(shell pwd)/host/toolchain/sysroot/bin:$(PATH)

MAKE = make

PREFIX = $(shell pwd)/root

.PHONY: all iso img clean run run-kvm run-iso run-iso-kvm run-img-ata run-img-ata-kvm run-img run-img-kvm

all:
	$(MAKE) core PREFIX=$(PREFIX) -C root/src

iso: all
	grub-mkrescue -o qword.iso root

img: all
	cp root/src/qloader/qloader.bin ./qword.img
	dd bs=32768 count=32768 if=/dev/zero >> ./qword.img
	truncate --size=-4096 ./qword.img
	echfs-utils ./qword.img format 32768
	./copy-root-to-img.sh root qword.img

clean:
	$(MAKE) core-clean -C root/src
	rm -f qword.iso qword.img

QEMU_FLAGS := $(QEMU_FLAGS) \
	-m 2G \
	-net none \
	-debugcon stdio \
	-d cpu_reset \
	-machine type=q35

run: run-img

run-kvm: run-img-kvm

run-iso:
	qemu-system-x86_64 $(QEMU_FLAGS) -drive file=qword.iso,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1

run-iso-kvm:
	qemu-system-x86_64 $(QEMU_FLAGS) -drive file=qword.iso,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1 -enable-kvm

run-img-ata:
	qemu-system-x86_64 $(QEMU_FLAGS) -drive file=qword.img,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1

run-img-ata-kvm:
	qemu-system-x86_64 $(QEMU_FLAGS) -drive file=qword.img,index=0,media=disk,format=raw -smp sockets=1,cores=4,threads=1 -enable-kvm

run-img:
	qemu-system-x86_64 $(QEMU_FLAGS) -device ahci,id=ahci -drive if=none,id=disk,file=qword.img,format=raw -device ide-drive,drive=disk,bus=ahci.0 -smp sockets=1,cores=4,threads=1

run-img-kvm:
	qemu-system-x86_64 $(QEMU_FLAGS) -device ahci,id=ahci -drive if=none,id=disk,file=qword.img,format=raw -device ide-drive,drive=disk,bus=ahci.0 -smp sockets=1,cores=4,threads=1 -enable-kvm
