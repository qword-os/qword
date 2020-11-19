# The qword kernel

[![goto counter](https://img.shields.io/github/search/qword-os/qword/goto.svg)](https://github.com/qword-os/qword/search?q=goto)

![Reference screenshot](/screenshot.png?raw=true "Reference screenshot")

# THIS PROJECT IS DEFUNCT.
Sorry about that.

Please do not send PRs or open issues against this defunct project which will
no longer be updated.

You're free to check out the code, fork it, and all that, as long as the license
(LICENSE.md) is respected.

# Moving on
As the name implies, this is just the kernel. To build the whole system, follow the
instructions at <https://github.com/qword-os/build>.

## Build requirements

These are the tools needed for the build:
- `git` (only for the initial download).
- `bash`.
- `make`.
- `gcc`, 8 or higher.
- `nasm`.
- `objcopy`.
- `qemu` (to test it).
- `clang` (to test it).
- `llvm` (to test it).

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
