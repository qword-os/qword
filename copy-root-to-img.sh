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

ROOT_FILES_COUNT=0
for i in $ROOT_FILES; do
    echo $(( ROOT_FILES_COUNT++ )) > /dev/null
done

echo "Transferring directory '$1' to image '$2'..."

FILES_COUNTER=1
for i in $ROOT_FILES; do
    printf "\r\e[KFile $FILES_COUNTER/$ROOT_FILES_COUNT ($i)"
    echo $(( FILES_COUNTER++ )) > /dev/null
    echfs-utils "$IMAGE_REALPATH" import "$i" "$i" &> /dev/null
done

printf "\nDone.\n"

exit 0
