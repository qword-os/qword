#!/usr/bin/env bash

set -e
set -x

CROSS_ROOT="$(pwd)/cross-root"
TARGET_ROOT="$(realpath ../..)/root"
TARGET=x86_64-qword

mkdir -p "$TARGET_ROOT"
export PATH="$CROSS_ROOT/bin:$PATH"

mkdir -p build-toolchain
cd build-toolchain

git clone https://github.com/managarm/mlibc.git || true
cd mlibc
git pull
rm -rf build
mkdir -p build
cd build
sed "s|@@sysroot@@|$TARGET_ROOT|g" < ../../../cross_file.txt > ./cross_file.txt
meson .. --prefix=/ --libdir=lib --includedir=usr/include --buildtype=debugoptimized --cross-file cross_file.txt
pushd ../subprojects
for i in $(ls -d */); do
	cd $i
	git pull
	cd ..
done
popd
ninja
DESTDIR="$TARGET_ROOT" ninja install

exit 0
