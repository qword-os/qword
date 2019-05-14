#!/usr/bin/env bash

fdisk qword.hdd <<EOF
n



+64M
n




w
EOF
