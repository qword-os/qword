#ifndef __FS_H__
#define __FS_H__

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int used;
    int perms;
    size_t sz;
    size_t offset;
    uint8_t *start;
} stat_t;

#endif
