SHELL = /bin/bash

PATH := $(shell pwd)/host/toolchain/cross-root/bin:$(PATH)
PREFIX = $(shell pwd)/root

.PHONY: all img hdd clean run run-img run-nokvm run-img-nvme-test

all:
	$(MAKE) core PREFIX=$(PREFIX) -C root/src
	cp -v /etc/localtime ./root/etc/

IMGSIZE := 4096

img: all
	cp root/src/qloader/qloader.bin ./qword.img
	fallocate -l $(IMGSIZE)M ./qword.img
	echfs-utils ./qword.img quick-format 32768
	./copy-root-to-img.sh root qword.img

LOOP_DEVICE = $(shell losetup --find)

hdd: all
	sudo -v
	rm -rf qword.part
	fallocate -l $(IMGSIZE)M qword.part
	echfs-utils ./qword.part quick-format 32768
	./copy-root-to-img.sh root qword.part
	rm -rf qword.hdd
	fallocate -l $(IMGSIZE)M qword.hdd
	fallocate -o $(IMGSIZE)M -l $$(( 67108864 + 1048576 )) qword.hdd
	./create_partition_scheme.sh
	sudo losetup -P $(LOOP_DEVICE) qword.hdd
	sudo mkfs.fat $(LOOP_DEVICE)p1
	sudo rm -rf mnt
	sudo mkdir mnt && sudo mount $(LOOP_DEVICE)p1 ./mnt
	sudo grub-install --target=i386-pc --boot-directory=`realpath ./mnt/boot` $(LOOP_DEVICE)
	sudo cp -r ./root/boot/* ./mnt/boot/
	sudo umount ./mnt
	sudo rm -rf mnt
	sudo bash -c "cat qword.part > $(LOOP_DEVICE)p2"
	sudo losetup -d $(LOOP_DEVICE)
	rm qword.part

clean:
	$(MAKE) core-clean -C root/src

QEMU_FLAGS := $(QEMU_FLAGS) \
	-m 2G \
	-net none \
	-debugcon stdio \
	-d cpu_reset

run:
	qemu-system-x86_64 $(QEMU_FLAGS) -device ahci,id=ahci -drive if=none,id=disk,file=qword.hdd,format=raw -device ide-drive,drive=disk,bus=ahci.0 -smp sockets=1,cores=4,threads=1 -enable-kvm

run-img:
	qemu-system-x86_64 $(QEMU_FLAGS) -device ahci,id=ahci -drive if=none,id=disk,file=qword.img,format=raw -device ide-drive,drive=disk,bus=ahci.0 -smp sockets=1,cores=4,threads=1 -enable-kvm

run-nokvm:
	qemu-system-x86_64 $(QEMU_FLAGS) -device ahci,id=ahci -drive if=none,id=disk,file=qword.hdd,format=raw -device ide-drive,drive=disk,bus=ahci.0 -smp sockets=1,cores=4,threads=1

run-img-nvme-test:
	qemu-system-x86_64 $(QEMU_FLAGS) -device ahci,id=ahci -drive if=none,id=disk,file=qword.img,format=raw \
	-device ide-drive,drive=disk,bus=ahci.0 -smp sockets=1,cores=4,threads=1 \
	-drive if=none,id=disk1,file=test.img,format=raw \
	-device nvme,drive=disk1,serial=deadbeef
