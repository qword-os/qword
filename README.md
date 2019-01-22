# qword - A KISS Unix-like operating system, written in C and Assembly for x86_64.

[![goto counter](https://img.shields.io/github/search/qword-os/qword/goto.svg)](https://github.com/qword-os/qword/search?q=goto)

## Features
- SMP (multicore) scheduler supporting thread scheduling.
- Program loading with minimal userspace.
- Fully functional VFS with support for several filesystems.
- Support for AHCI/SATA.
- ATA disk support.

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
#make PREFIX=<myprefix> install
# Now build the toolchain (this step will take a while)
cd ../toolchain
./make_toolchain.sh
# Go back to the root of the tree
cd ../..
# Build the ports distribution
cd root/src
./makeworld.sh -j4
./makeworld.sh clean
# Now to build qword itself
cd ../..
make clean && make DEBUG=qemu img && sync
```

You've now built qword, a flat `qword.img` disk image has been generated.
To run the OS in QEMU, use `make run-img`.
To run it with KVM enabled, use `make run-img-kvm`.
