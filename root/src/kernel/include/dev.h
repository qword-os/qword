#ifndef __DEV_H__
#define __DEV_H__

#include <stddef.h>
#include <stdint.h>

typedef size_t dev_t;

dev_t device_find(const char *name);
dev_t device_add(const char *name, int magic, uint64_t size,
        int (*read)(int drive, void *buf, uint64_t loc, size_t count),
        int (*write)(int drive, void *buf, uint64_t loc, size_t count));

int device_read(int device, void *buf, uint64_t loc, size_t count);
int device_write(int device, void *buf, uint64_t loc, size_t count);

#endif
