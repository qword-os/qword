set -e

PKG_URL=https://github.com/windozz/lai.git

git clone $PKG_URL
cd lai
mkdir build && cd build
meson ..
ninja
pwd
cp liblai.a ../../kernel/
cd ../..
pwd
cp -r lai/ ./kernel/acpi/
rm -rf lai
