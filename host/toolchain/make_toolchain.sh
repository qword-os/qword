#!/usr/bin/env bash

set -e
set -x

CROSS_ROOT="$(pwd)/cross-root"
TARGET_ROOT="$(realpath ../..)/root"
TARGET=x86_64-qword
GCCVERSION=9.1.0
BINUTILSVERSION=2.32

if [ -z "$MAKEFLAGS" ]; then
	MAKEFLAGS="$1"
fi
export MAKEFLAGS

rm -rf "$CROSS_ROOT"
mkdir -p "$CROSS_ROOT"
export PATH="$CROSS_ROOT/bin:$PATH"

if [ -x "$(command -v gmake)" ]; then
    mkdir -p "$CROSS_ROOT/bin"
    cat <<EOF >"$CROSS_ROOT/bin/make"
#!/usr/bin/env sh
gmake "\$@"
EOF
    chmod +x "$CROSS_ROOT/bin/make"
fi

mkdir -p build-toolchain
cd build-toolchain

git clone https://github.com/managarm/mlibc.git || true
pushd mlibc
git pull
rm -rf build
mkdir -p build
cd build
sed "s|@@sysroot@@|$TARGET_ROOT|g" < ../../../cross_file.txt > ./cross_file.txt
meson .. --prefix=/ --libdir=lib --includedir=usr/include --buildtype=debugoptimized --cross-file cross_file.txt -Dheaders_only=true
ninja
DESTDIR="$TARGET_ROOT" ninja install
popd

rm -rf gcc-$GCCVERSION binutils-$BINUTILSVERSION build-gcc build-binutils
if [ ! -f binutils-$BINUTILSVERSION.tar.gz ]; then
	wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILSVERSION.tar.gz
fi
if [ ! -f gcc-$GCCVERSION.tar.gz ]; then
	wget https://ftp.gnu.org/gnu/gcc/gcc-$GCCVERSION/gcc-$GCCVERSION.tar.gz
fi
tar -xf gcc-$GCCVERSION.tar.gz
tar -xf binutils-$BINUTILSVERSION.tar.gz

cd binutils-$BINUTILSVERSION
patch -p1 < ../../binutils.patch
cd ..
mkdir build-binutils
cd build-binutils
../binutils-$BINUTILSVERSION/configure --target=$TARGET --prefix="$CROSS_ROOT" --with-sysroot="$TARGET_ROOT" --disable-werror
make
make install

cd ../gcc-$GCCVERSION
contrib/download_prerequisites
patch -p1 < ../../gcc.patch
cd libstdc++-v3 && autoconf && cd ..
cd ..
mkdir build-gcc
cd build-gcc
../gcc-$GCCVERSION/configure --target=$TARGET --prefix="$CROSS_ROOT" --with-sysroot="$TARGET_ROOT" --enable-languages=c,c++ --disable-gcov --disable-multilib --enable-initfini-array
make all-gcc
make install-gcc
cd ../..

./make_mlibc.sh

cd build-toolchain
cd build-gcc
make all-target-libgcc
make install-target-libgcc
make all-target-libstdc++-v3
make install-target-libstdc++-v3

exit 0
