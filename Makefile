MAKE = make
ARCH = x86_64

.PHONY: iso clean run

iso:
	$(MAKE) -C kernel
	cp kernel/kernel.bin iso/boot/kernel.bin
	grub-mkrescue -o os.iso iso
run:
	qemu-system-$(ARCH) -cdrom os.iso -smp sockets=1,cores=4,threads=1 -net none
run-kvm:
	qemu-system-$(ARCH) -cdrom os.iso -smp sockets=1,cores=4,threads=1 -enable-kvm -net none
clean:
	$(MAKE) clean -C kernel
	rm -f os.iso
