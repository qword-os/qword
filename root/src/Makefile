MAKE = make

PREFIX = 

.PHONY: all clean

all:
	@# Build and install qword kernel
	$(MAKE) -C kernel
	$(MAKE) PREFIX=$(PREFIX) install -C kernel

	@# Build qloader bootloader
	$(MAKE) -C qloader

	@# Build test userland program
	$(MAKE) -C test
	$(MAKE) PREFIX=$(PREFIX) install -C test

	@# Future targets are going to be added here

clean:
	@# Cleanup qword kernel
	$(MAKE) clean -C kernel

	@# Cleanup qloader bootloader
	$(MAKE) clean -C qloader

	@# Cleanup test program
	$(MAKE) clean -C test

	@# Future targets are going to be added here
