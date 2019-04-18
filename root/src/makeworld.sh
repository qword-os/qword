#!/usr/bin/env bash

set -e

BUILDPORTS=$(find ./ -type f -name 'def.pkg' | sed 's/\/def.pkg//g')

cd ../bin
for i in $BUILDPORTS; do
    ./pkg install $i
    ./pkg clean $i
done
