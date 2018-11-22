#!/bin/bash

if [ -z "$PREFIX" ]; then
	PREFIX=$(pwd)/sysroot
fi
if [ -z "$TARGET" ]; then
	TARGET=x86_64-qword
fi
if [ -z "$GCCVERSION" ]; then
	GCCVERSION=8.2.0
fi
if [ -z "$BINUTILSVERSION" ]; then
	BINUTILSVERSION=2.31.1
fi
if [ -z "$MAKEFLAGS" ]; then
	MAKEFLAGS="-j `grep -c ^processor /proc/cpuinfo`"
fi

export MAKEFLAGS

echo "Prefix: $PREFIX"
echo "Target: $TARGET"
echo "GCC version: $GCCVERSION"
echo "Binutils version: $BINUTILSVERSION"
echo "Make flags: $MAKEFLAGS"

set -e
set -x

mkdir -p "$PREFIX"
export PATH="$PREFIX/bin:$PATH"

mkdir -p build-toolchain
cd build-toolchain

rm -rf gcc-$GCCVERSION binutils-$BINUTILSVERSION build-gcc build-binutils
if [ ! -f binutils-$BINUTILSVERSION.tar.gz ]; then
	wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILSVERSION.tar.gz
fi
if [ ! -f gcc-$GCCVERSION.tar.gz ]; then
	wget https://ftp.gnu.org/gnu/gcc/gcc-$GCCVERSION/gcc-$GCCVERSION.tar.gz
fi
tar -vxf gcc-$GCCVERSION.tar.gz
tar -vxf binutils-$BINUTILSVERSION.tar.gz

cd binutils-$BINUTILSVERSION
patch -p1 < ../../binutils-$BINUTILSVERSION.patch
cd ..
mkdir build-binutils
cd build-binutils
../binutils-$BINUTILSVERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$PREFIX" --disable-werror
make
make install

mkdir -p "$PREFIX/usr/include"
cd ../gcc-$GCCVERSION
patch -p1 < ../../gcc-$GCCVERSION.patch
contrib/download_prerequisites
cd ..
mkdir build-gcc
cd build-gcc
../gcc-$GCCVERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$PREFIX" --enable-languages=c,c++ --disable-multilib --enable-initfini-array
make all-gcc
make install-gcc
cd ../..

./make_mlibc.sh

cd build-toolchain
cd build-gcc
make all-target-libgcc
make install-target-libgcc

exit 0
