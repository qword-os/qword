#!/usr/bin/env bash

set -e

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <source directory> <image>"
    exit 1
fi

if [ ! -d "$1" ]; then
    echo "Directory '$1' not found"
    exit 1
fi

if [ ! -f "$2" ]; then
    echo "File '$2' not found"
    exit 1
fi

shopt -s globstar

IMAGE_REALPATH=$(realpath "$2")

cd "$1"

shopt -s dotglob
ROOT_FILES=$(echo **)

echo "Transferring directory '$1' to image '$2'..."

for i in $ROOT_FILES; do
    echfs-utils "$IMAGE_REALPATH" import "$i" "$i" &> /dev/null
done

echo "Done."

exit 0
