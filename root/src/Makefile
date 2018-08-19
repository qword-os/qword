MAKE = make

PREFIX = 

.PHONY: all clean

all:
	@# Build and install qword kernel
	$(MAKE) -C kernel
	$(MAKE) PREFIX=$(PREFIX) install -C kernel

	@# Build qloader bootloader
	$(MAKE) -C qloader

	@# Future targets are going to be added here

clean:
	@# Cleanup qword kernel
	$(MAKE) clean -C kernel

	@# Cleanup qloader bootloader
	$(MAKE) clean -C qloader

	@# Future targets are going to be added here
