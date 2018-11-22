# qword - A modern and fast unix-like operating system, written in C and Assembly for x86_64.

[![goto counter](https://img.shields.io/github/search/qword-os/qword/goto.svg)](https://github.com/qword-os/qword/search?q=goto)

## Features
- Paging support with higher-half kernel.
- SMP compliant scheduler supporting thread scheduling.
- Program loading with minimal userspace.
- ATA PIO disk support.
- Fully functional VFS and devfs with support for the echidnaFS filesystem.

## Planned features
- Support for AHCI/SATA.
- Support for `pthreads` and other elements of POSIX, to allow porting common programs.


## Build requirements
In order to build qword, make sure to have the following installed:
- bash
- make
- meson
- ninja
- GCC and binutils
- nasm
- QEMU (to test it)

On Debian and Ubuntu, these packages can be installed with:
```bash
sudo apt-get install build-essential meson nasm qemu-system-x86
```

## Building
```bash
# Clone repo wherever you like
git clone https://github.com/qword-os/qword.git
cd qword
# Build and install echfs-utils (used to build the root fs image)
cd host/echfs-utils
make
# This will install echfs-utils in /usr/local
sudo make install
# Else specify a PREFIX variable if you want to install it elsewhere
make PREFIX=<myprefix> install
# Now build the toolchain (this step will take a while)
cd ../toolchain
./make_toolchain.sh
# Go back to the root of the tree
cd ../..
# Now to build qword itself
make clean && make img && sync
```

You've now built qword, a flat `qword.img` disk image has been generated.
To run the OS in QEMU, use `make run-img`.
To run it with KVM enabled, use `make run-img-kvm`.
