#ifndef __DEV_H__
#define __DEV_H__

#include <stddef.h>
#include <stdint.h>

typedef size_t dev_t;

dev_t device_find(const char *);
dev_t device_add(const char *, int, uint64_t,
        int (*read)(int, void *, uint64_t, size_t),
        int (*write)(int, const void *, uint64_t, size_t),
        int (*flush)(int));

uint64_t device_size(int);

int device_read(int, void *, uint64_t, size_t);
int device_write(int, const void *, uint64_t, size_t);
int device_flush(int);

#endif
