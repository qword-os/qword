set -e

PKG_URL=https://github.com/windozz/lai.git

git clone $PKG_URL
cp -r lai/ ./kernel/acpi/
rm -rf lai
