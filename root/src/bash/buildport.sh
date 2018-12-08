#!/bin/bash

set -e
set -x

QWORD_BASE=$(realpath ../../..)
QWORD_ROOT=$(realpath ../..)
export PATH=$QWORD_BASE/host/toolchain/sysroot/bin:$PATH

BASH_VERSION=4.4.18

rm -rf bash-$BASH_VERSION
if [ ! -f bash-$BASH_VERSION.tar.gz ]; then
	wget https://ftp.gnu.org/gnu/bash/bash-$BASH_VERSION.tar.gz
fi

tar -xf bash-$BASH_VERSION.tar.gz
cd bash-$BASH_VERSION
patch -p1 < ../bash-$BASH_VERSION.patch

./configure --host=x86_64-qword --prefix=/ --without-bash-malloc
make
make DESTDIR=$QWORD_ROOT install
