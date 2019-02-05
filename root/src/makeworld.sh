#!/usr/bin/env bash

set -e

BUILDPORTS=$(find ./ -type f -name 'buildport.sh' | sed 's/\/buildport.sh//g' | sort)
for i in $BUILDPORTS ; do pushd $i && ./buildport.sh "$@" && popd ; done
