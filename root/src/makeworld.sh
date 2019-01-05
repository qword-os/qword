#!/bin/bash

set -e

BUILDPORTS=$(find ./ -type f -name 'buildport.sh' -printf '%h\n')
for i in $BUILDPORTS ; do pushd $i && ./buildport.sh "$@" && popd ; done
