#!/usr/bin/env bash

set -e

BUILDPORTS=$(echo */def.pkg | sed 's/\/def.pkg//g')

cd ../bin
./pkg install $BUILDPORTS
./pkg clean $BUILDPORTS
