MAKE = make

iso: kernel/kernel.bin
	cp kernel/kernel.bin iso/boot/kernel.bin
	grub-mkrescue -o os.iso iso

kernel/kernel.bin:
	$(MAKE) -C kernel

clean:
	$(MAKE) clean -C kernel
