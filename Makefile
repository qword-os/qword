MAKE = make

.PHONY: iso clean

iso:
	$(MAKE) -C kernel
	cp kernel/kernel.bin iso/boot/kernel.bin
	grub-mkrescue -o os.iso iso

clean:
	$(MAKE) clean -C kernel
