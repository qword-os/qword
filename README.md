# qword - A KISS Unix-like operating system, written in C and Assembly for x86_64.

[![goto counter](https://img.shields.io/github/search/qword-os/qword/goto.svg)](https://github.com/qword-os/qword/search?q=goto)

![Reference screenshot](/screenshot.png?raw=true "Reference screenshot")

## Talk to us!
We have a [Discord server](https://discord.gg/RnCtsqD) with all the developers for any question, support, contribution, or just chat!

## Features
- SMP (multicore) scheduler supporting thread scheduling.
- Program loading with minimal userspace.
- Fully functional VFS with support for several filesystems.
- Support for AHCI/SATA.
- ATA disk support.

## Build requirements
In order to build qword, make sure to have the following installed:
  `wget`, `git`, `bash`, `make` (`gmake` on *BSD), `meson`, `ninja`, `gcc/g++ (8 or higher)`, `nasm`, `xz`, `autoconf`, and QEMU (to test it).

## Building
```bash
# Clone repo wherever you like
git clone https://github.com/qword-os/qword.git
cd qword/host
# Let's first build and install the echfs-utils
git clone https://github.com/qword-os/echfs.git
cd echfs
make
# This will install echfs-utils in /usr/local
sudo make install
# Else specify a PREFIX variable if you want to install it elsewhere
#make PREFIX=<myprefix> install
# Now build the toolchain (this step will take a while)
cd ../toolchain
# You can replace the 4 in -j4 with your number of cores + 1
./make_toolchain.sh -j4
# Go back to the root of the tree
cd ../..
# Build the ports distribution
cd root/src
MAKEFLAGS=-j4 ./makeworld.sh
# Now to build qword itself
cd ../..
# You might need to use gmake instead of make here on FreeBSD
make clean && make img               # For a standard release build
make clean && make DBGOUT=qemu img   # For QEMU console debug output
make clean && make DBGOUT=tty img    # For kernel tty debug output
make clean && make DBGOUT=both img   # For both of the above
make clean && make DBGSYM=yes img    # For compilation with debug symbols and other debug facilities (can be used in combination with the other options)
```

You've now built qword, a flat `qword.img` disk image has been generated.
To run the OS in QEMU, use `make run-img`.
To run it with KVM enabled, use `make run-img-kvm`.
