#!/usr/bin/env bash

set -e

PKG_URL=https://github.com/qword-os/lai.git

cd ./kernel/acpi/
git clone $PKG_URL || ( cd lai && git pull )
