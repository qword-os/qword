#ifndef __DEV_H__
#define __DEV_H__

#include <stddef.h>
#include <stdint.h>

typedef uint64_t dev_t;

#define MAX_DEVICES 128

struct device {
    int used;
    char name[128];
    int magic;
    uint64_t size;
    int (*read)(int, void *, uint64_t, size_t);
    int (*write)(int, const void *, uint64_t, size_t);
    int (*flush)(int);
};

extern struct device devices[MAX_DEVICES];

dev_t device_find(const char *);
dev_t device_add(const char *, int, uint64_t,
        int (*read)(int, void *, uint64_t, size_t),
        int (*write)(int, const void *, uint64_t, size_t),
        int (*flush)(int));

uint64_t device_size(int);

int device_read(int, void *, uint64_t, size_t);
int device_write(int, const void *, uint64_t, size_t);
int device_flush(int);
char *device_list(size_t);

void device_sync_worker(void *);

#endif
