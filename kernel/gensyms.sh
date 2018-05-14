#!/bin/bash

cat <<EOF

global debug_symbols_names

section .debug_symbols

debug_symbols_names:

EOF

x86_64-elf-nm -n kernel.sym | awk '/ T | t / {printf "db \"%s\", 0\n", $3;}'

cat <<EOF

global debug_symbols_addresses

section .debug_symbols

align 8
debug_symbols_addresses:

EOF

x86_64-elf-nm -n kernel.sym | awk '/ T | t / {printf "dq 0x%s\n", $1;}'
