set -e

PKG_URL=https://github.com/windozz/lai.git

git clone $PKG_URL
cd lai
mkdir build && cd build
meson ..
ninja
cp liblai.a ../../kernel/
cd ..
cp -r include/lai ../kernel/acpi/
rm -rf build
cd ..
rm -rf lai
