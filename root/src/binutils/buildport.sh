#!/bin/bash

set -e

PKG_NAME=binutils
PKG_VERSION=2.31.1
PKG_URL=https://ftp.gnu.org/gnu/binutils/binutils-$PKG_VERSION.tar.gz
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
    export PATH=$QWORD_BASE/host/toolchain/sysroot/bin:$PATH
fi

set -x

rm -rf $PKG_ARCHIVE_DIR
if [ ! -f $PKG_TARBALL ]; then
    wget $PKG_URL
fi

tar -xf $PKG_TARBALL
cd $PKG_ARCHIVE_DIR
patch -p1 < ../$PKG_NAME-$PKG_VERSION.patch

./configure --host=x86_64-qword --target=x86_64-qword --prefix=$PKG_PREFIX --with-sysroot=$PKG_PREFIX --disable-werror
make "$@"
make DESTDIR=$QWORD_ROOT install
