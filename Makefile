MAKE = make
ARCH = x86_64

.PHONY: iso clean run

iso:
	$(MAKE) -C kernel
	cp kernel/kernel.bin iso/boot/kernel.bin
	grub-mkrescue -o os.iso iso
run:
	qemu-system-$(ARCH) -cdrom os.iso
clean:
	$(MAKE) clean -C kernel
	rm -f os.iso
