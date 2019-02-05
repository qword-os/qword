#!/usr/bin/env bash

set -e

PKG_NAME=mlibc
PKG_URL=https://github.com/managarm/mlibc.git
PKG_ARCHIVE_DIR=$PKG_NAME
PKG_PREFIX=/

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
git clone $PKG_URL

cd $PKG_ARCHIVE_DIR
mkdir build && cd build
sed "s|@@sysroot@@|$PKG_PREFIX|g" < ../../cross_file.txt > ./cross_file.txt
meson .. --prefix=$PKG_PREFIX --libdir=lib --buildtype=debugoptimized --cross-file cross_file.txt
ninja
DESTDIR=$QWORD_ROOT ninja install
