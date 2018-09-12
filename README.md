# qword - A modern and fast unix-like operating system, written in C and Assembly for x86_64.

## Features
- Paging support with higher-half kernel.
- SMP compliant scheduler supporting thread scheduling.
- Program loading with minimal userspace.
- ATA PIO disk support.
- Fully functional VFS and devfs with support for the echidnaOS filesystem, `echfs`.

## Planned features
- Support for AHCI/SATA.
- Support for `pthreads` and other elements of POSIX, to allow porting common programs.


## Building
```bash
# clone repo wherever you like
git clone https://github.com/qword-os/qword.git
cd qword
# build echfs-utils (used to build the root fs image).
cd host/echfs-utils
sudo make
# copy echfs-utils to /usr/local/bin. If you want to place it 
# somewhere else in your path, just change the PREFIX variable in the
# Makefile. The binary will then be copied to $PREFIX/bin.
sudo make install
# Now to build qword itself. Ensure you have nasm installed.
cd ...
make img
```
- You've now built qword. To run the OS, use `make run-img` to start a qemu instance. Again, you should have qemu installed for this.
