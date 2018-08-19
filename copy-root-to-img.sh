#!/bin/bash

set -e

shopt -s globstar

cd root

ROOT_FILES=$(echo **)

set -x

for i in $ROOT_FILES; do
    echfs-utils ../qword.img import "$i" "$i"
done
