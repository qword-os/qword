#!/bin/bash

if [ -z "$PREFIX" ]; then
	PREFIX="$(pwd)/sysroot"
fi
if [ -z "$TARGET" ]; then
	TARGET=x86_64-qword
fi
if [ -z "$MAKEFLAGS" ]; then
	MAKEFLAGS="-j `grep -c ^processor /proc/cpuinfo`"
fi

export MAKEFLAGS

echo "Prefix: $PREFIX"
echo "Target: $TARGET"
echo "Make flags: $MAKEFLAGS"

set -e
set -x

mkdir -p "$PREFIX"
export PATH="$PREFIX/bin:$PATH"

mkdir -p build-toolchain
cd build-toolchain

git clone https://github.com/managarm/mlibc.git || true
cd mlibc
git pull
rm -rf build
mkdir -p build
cd build
sed "s|@@sysroot@@|$PREFIX|g" < ../../../cross_file.txt > ./cross_file.txt
meson .. --prefix=/usr --libdir=lib --buildtype=debugoptimized --cross-file cross_file.txt
pushd ../subprojects
for i in $(ls -d */); do
	cd $i
	git pull
	cd ..
done
popd
ninja
DESTDIR="$PREFIX" ninja install

# install libraries into root
mkdir -p ../../../../../root/lib
cp -vr "$PREFIX/usr/lib/"* ../../../../../root/lib/

exit 0
