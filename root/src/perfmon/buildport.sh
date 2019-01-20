#!/bin/bash

set -e

QWORD_ROOT=$(realpath ../..)

if [ ! "$OSTYPE" = "qword" ]; then
    QWORD_BASE=$(realpath ../../..)
    export PATH=$QWORD_BASE/host/toolchain/sysroot/bin:$PATH
fi

set -x

make "$@"
make DESTDIR=$QWORD_ROOT install
