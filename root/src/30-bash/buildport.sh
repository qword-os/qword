#!/usr/bin/env bash

set -e

PKG_NAME=bash
PKG_VERSION=4.4
PKG_URL=https://ftp.gnu.org/gnu/bash/bash-$PKG_VERSION.tar.gz
PKG_TARBALL=$PKG_NAME-$PKG_VERSION.tar.gz
PKG_ARCHIVE_DIR=$PKG_NAME-$PKG_VERSION
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
if [ ! -f $PKG_TARBALL ]; then
    wget $PKG_URL
fi

# bash-specific: get all the patches
BASH_PATCHES="
001 002 003 004 005 006 007 008 009 010
011 012 013 014 015 016 017 018 019 020
021 022 023
"

# XXX update this in the future
BASH_PATCH_URL_BASE="https://ftp.gnu.org/gnu/bash/bash-4.4-patches/bash44"
BASH_PATCH_BASENAME="bash44"

for i in $BASH_PATCHES; do
    if [ ! -f $BASH_PATCH_BASENAME-$i ]; then
        wget $BASH_PATCH_URL_BASE-$i
    fi
done

tar -xf $PKG_TARBALL
cd $PKG_ARCHIVE_DIR
patch -p1 < ../$PKG_NAME-$PKG_VERSION.patch

# bash-specific: apply all patches
for i in $BASH_PATCHES; do
    patch -p0 < ../$BASH_PATCH_BASENAME-$i
done

./configure --host=x86_64-qword --prefix=$PKG_PREFIX --without-bash-malloc
make "$@"
make DESTDIR=$QWORD_ROOT install
