#ifndef __DEVFS_H__
#define __DEVFS_H__

#include <stddef.h>
#include <stdint.h>
#include <lib/dynarray.h>
#include <lib/types.h>

#define MAX_DEVICES 128

struct device_calls_t {
    int (*read)(int, void *, uint64_t, size_t);
    int (*write)(int, const void *, uint64_t, size_t);
    int (*flush)(int);
};

struct device_t {
    char name[128];
    int intern_fd;
    size_t size;
    struct device_calls_t calls;
};

dev_t device_add(struct device_t *);

void device_sync_worker(void *);

#endif
