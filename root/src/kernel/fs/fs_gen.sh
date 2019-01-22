#!/bin/bash

cat >fs.h <<EOF
#ifndef __FS_H__
#define __FS_H__

int init_fs(void);

EOF

cat >fs.c <<EOF
#include <fs/fs.h>

int init_fs(void) {
EOF

for i in $(ls -d */); do
    echo "int init_fs_${i%%/}(void);" >> fs.h
    echo "    if (init_fs_${i%%/}()) return -1;" >> fs.c
done

cat >>fs.h <<EOF

#endif
EOF

cat >>fs.c <<EOF

    return 0;
}
EOF
