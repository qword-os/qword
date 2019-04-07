MAKE = make

PREFIX =

.PHONY: core core-clean

core:
	@# Build and install qword kernel
	./build_lai.sh
	$(MAKE) -C kernel
	$(MAKE) PREFIX=$(PREFIX) install -C kernel

	@# Build qloader bootloader
	$(MAKE) -C qloader

core-clean:
	@# Cleanup qword kernel
	$(MAKE) clean -C kernel

	@# Cleanup qloader bootloader
	$(MAKE) clean -C qloader
