#!/bin/bash

cat <<EOF

global debug_symbols_names

section .debug_symbols

debug_symbols_names:

EOF

i386-elf-nm -n kernel.sym | awk '/ T | t / {printf "db \"%s\", 0\n", $3;}'

cat <<EOF

global debug_symbols_addresses

section .debug_symbols

align 4
debug_symbols_addresses:

EOF

i386-elf-nm -n kernel.sym | awk '/ T | t / {printf "dd 0x%s\n", $1;}'
