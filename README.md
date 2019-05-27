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
- MBR partitions support.

## Build requirements
In order to build qword, make sure to have the following installed:
  `wget`, `git`, `bash`, `make`, `patch`, `libtool`, `fdisk`, `grub`, `meson`, `ninja`, `gcc/g++ (8 or higher)`, `nasm`, `xz`, `autoconf`, and QEMU (to test it).

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
make clean && make hdd               # For a standard release build
make clean && make DBGOUT=qemu hdd   # For QEMU console debug output
make clean && make DBGOUT=tty hdd    # For kernel tty debug output
make clean && make DBGOUT=both hdd   # For both of the above
make clean && make DBGSYM=yes hdd    # For compilation with debug symbols and other debug facilities (can be used in combination with the other options)
# And now if you wanna test it in qemu simply run
make run
# If that doesn't work because you don't have hardware virtualisation/KVM, run
make run-nokvm
```
