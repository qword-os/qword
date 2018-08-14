#include <stdint.h>
#include <stddef.h>
#include <dev.h>
#include <klib.h>

#define MAX_DEVICES 128

typedef struct {
    int used;
    char name[128];
    int magic;
    uint64_t size;
    int (*read)(int, void *, uint64_t, size_t);
    int (*write)(int, const void *, uint64_t, size_t);
    int (*flush)(int);
} device_t;

static device_t devices[MAX_DEVICES];

int device_read(int device, void *buf, uint64_t loc, size_t count) {
    int magic = devices[device].magic;

    return devices[device].read(magic, buf, loc, count);
}

int device_write(int device, const void *buf, uint64_t loc, size_t count) {
    int magic = devices[device].magic;

    return devices[device].write(magic, buf, loc, count);
}

int device_flush(int device) {
    int magic = devices[device].magic;

    return devices[device].flush(magic);
}

/* Returns a device ID by name, (dev_t)(-1) if not found. */
dev_t device_find(const char *name) {
    dev_t device;

    for (device = 0; device < MAX_DEVICES; device++) {
        if (!devices[device].used)
            continue;
        if (!kstrcmp(devices[device].name, name))
            return device;
    }

    return (dev_t)(-1);
}

/* Registers a device. Returns the ID of the registered device. */
/* (dev_t)(-1) on error. */
dev_t device_add(const char *name, int magic, uint64_t size,
        int (*read)(int, void *, uint64_t, size_t),
        int (*write)(int, const void *, uint64_t, size_t),
        int (*flush)(int)) {

    dev_t device;

    for (device = 0; device < MAX_DEVICES; device++) {
        if (!devices[device].used) {
            devices[device].used = 1;
            kstrcpy(devices[device].name, name);
            devices[device].magic = magic;
            devices[device].size = size;
            devices[device].read = read;
            devices[device].write = write;
            devices[device].flush = flush;
            return device;
        }
    }

    return (dev_t)(-1);
}
