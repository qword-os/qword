#!/bin/bash

if [ -z "$PREFIX" ]; then
	PREFIX=$(pwd)/sysroot
fi
if [ -z "$TARGET" ]; then
	TARGET=x86_64-qword
fi
if [ -z "$GCCVERSION" ]; then
	GCCVERSION=8.2.0
fi
if [ -z "$BINUTILSVERSION" ]; then
	BINUTILSVERSION=2.31.1
fi
if [ -z "$MAKEFLAGS" ]; then
	MAKEFLAGS="$1"
fi
if [ -x "$(command -v gmake)" ]; then
    mkdir -p "$PREFIX/bin"
    cat <<EOF >"$PREFIX/bin/make"
#!/bin/sh
gmake "$@"
EOF
    chmod +x "$PREFIX/bin/make"
    MAKE="gmake"
else
    MAKE="make"
fi

export MAKEFLAGS

echo "Prefix: $PREFIX"
echo "Target: $TARGET"
echo "GCC version: $GCCVERSION"
echo "Binutils version: $BINUTILSVERSION"
echo "Make flags: $MAKEFLAGS"

set -e
set -x

rm -rf "$PREFIX"
mkdir -p "$PREFIX"
export PATH="$PREFIX/bin:$PATH"

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
$MAKE
$MAKE install

mkdir -p "$PREFIX/usr/include"
cd ../gcc-$GCCVERSION
contrib/download_prerequisites
patch -p1 < ../../gcc-$GCCVERSION.patch
cd ..
mkdir build-gcc
cd build-gcc
../gcc-$GCCVERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$PREFIX" --enable-languages=c,c++ --disable-multilib --enable-initfini-array
$MAKE all-gcc
$MAKE install-gcc
cd ../..

./make_mlibc.sh

cd build-toolchain
cd build-gcc
$MAKE all-target-libgcc
$MAKE install-target-libgcc
$MAKE all-target-libstdc++-v3
$MAKE install-target-libstdc++-v3

exit 0
