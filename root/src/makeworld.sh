#!/usr/bin/env bash

set -e

BUILDPORTS=$(find ./ -type f -name 'def.pkg' | sed 's/\/def.pkg//g')

cd ../bin
./pkg install $BUILDPORTS
./pkg clean $BUILDPORTS
