#!/usr/bin/env bash

set -e

PKG_NAME=gcc
PKG_VERSION=8.2.0
PKG_URL=https://ftp.gnu.org/gnu/gcc/gcc-$PKG_VERSION/gcc-$PKG_VERSION.tar.gz
PKG_TARBALL=$PKG_NAME-$PKG_VERSION.tar.gz
PKG_ARCHIVE_DIR=$PKG_NAME-$PKG_VERSION
PKG_PREFIX=/usr

if [ "$1" = "clean" ]; then
    rm -rf $PKG_ARCHIVE_DIR
    exit 0
fi

QWORD_ROOT=$(realpath ../..)

if [ ! "$OSTYPE" = "qword" ]; then
    QWORD_BASE=$(realpath ../../..)
    export PATH=$QWORD_BASE/host/toolchain/cross-root/bin:$PATH
fi

set -x

rm -rf $PKG_ARCHIVE_DIR
if [ ! -f $PKG_TARBALL ]; then
    wget $PKG_URL
fi

tar -xf $PKG_TARBALL
cd $PKG_ARCHIVE_DIR
contrib/download_prerequisites
patch -p1 < ../$PKG_NAME-$PKG_VERSION.patch

mkdir build && cd build
../configure --host=x86_64-qword --target=x86_64-qword --prefix=$PKG_PREFIX --with-sysroot=/ --with-build-sysroot=$QWORD_ROOT --enable-languages=c,c++ --disable-multilib --enable-initfini-array
make all-gcc "$@"
make DESTDIR=$QWORD_ROOT install-gcc
make all-target-libgcc "$@"
make DESTDIR=$QWORD_ROOT install-target-libgcc
make all-target-libstdc++-v3 "$@"
make DESTDIR=$QWORD_ROOT install-target-libstdc++-v3
