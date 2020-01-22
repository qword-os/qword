# The qword kernel

[![goto counter](https://img.shields.io/github/search/qword-os/qword/goto.svg)](https://github.com/qword-os/qword/search?q=goto)

![Reference screenshot](/screenshot.png?raw=true "Reference screenshot")

As the name implies, this is just the kernel. To build the whole system, follow the
instructions at <https://github.com/qword-os/build>.

## Discord
Join our Discord! Invite: https://discord.gg/z6b3qZC

## Prebuilt image
Get a prebuilt image today at: https://ci.oogacraft.com/job/qword/lastSuccessfulBuild/artifact/qword.hdd.xz

Note 1: This is a hard drive image compressed with xz. Unpack it with
```bash
xzcat < qword.hdd.xz > qword.hdd
```

Note 2: This image can be ran on QEMU using the following recommended command
```bash
qemu-system-x86_64 -enable-kvm -cpu host -smp 4 -m 2G -hda qword.hdd
```

Note 3: The default user/password is 'root/root'.

## Build requirements

These are the tools needed for the build:
- `git` (only for the initial download).
- `bash`.
- `make`.
- `gcc`, 8 or higher.
- `nasm`.
- `objcopy`.
- `qemu` (to test it).

## Building

```bash
# Clone repo wherever you like and enter.
git clone https://github.com/qword-os/qword.git
cd qword
# Make the kernel, you can replace the 4 in -j4 with your number of cores + 1.
make clean && make -j4
# Install in PREFIX, may need root permissions depending on the place.
sudo make install PREFIX=..
# Uninstall if needed in the specified PREFIX.
sudo make uninstall PREFIX=..
```
