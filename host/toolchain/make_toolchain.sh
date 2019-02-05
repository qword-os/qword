#!/usr/bin/env bash

set -e
set -x

PREFIX="$(pwd)/sysroot"
TARGET=x86_64-qword
GCCVERSION=8.2.0
BINUTILSVERSION=2.31.1

if [ -z "$MAKEFLAGS" ]; then
	MAKEFLAGS="$1"
fi
export MAKEFLAGS

rm -rf "$PREFIX"
mkdir -p "$PREFIX"
export PATH="$PREFIX/bin:$PATH"

if [ -x "$(command -v gmake)" ]; then
    mkdir -p "$PREFIX/bin"
    cat <<EOF >"$PREFIX/bin/make"
#!/usr/bin/env sh
gmake "\$@"
EOF
    chmod +x "$PREFIX/bin/make"
fi

mkdir -p build-toolchain
cd build-toolchain

git clone https://github.com/managarm/mlibc.git || true
pushd mlibc
git pull
rm -rf build
mkdir -p build
cd build
sed "s|@@sysroot@@|$PREFIX|g" < ../../../cross_file.txt > ./cross_file.txt
meson .. --prefix=/usr --libdir=lib --buildtype=debugoptimized --cross-file cross_file.txt -Dheaders_only=true
ninja
DESTDIR="$PREFIX" ninja install
popd

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
contrib/download_prerequisites
patch -p1 < ../../gcc-$GCCVERSION.patch
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
make all-target-libstdc++-v3
make install-target-libstdc++-v3

exit 0
