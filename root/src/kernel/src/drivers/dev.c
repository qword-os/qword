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
    int (*read)(int drive, void *buf, uint64_t loc, size_t count);
    int (*write)(int drive, void *buf, uint64_t loc, size_t count);
} device_t;

static device_t devices[MAX_DEVICES];

/* Registers a device. Returns the ID of the registered device. */
/* (dev_t)(-1) on error. */
dev_t device_add(const char *name, int magic, uint64_t size,
        int (*read)(int drive, void *buf, uint64_t loc, size_t count),
        int (*write)(int drive, void *buf, uint64_t loc, size_t count)) {

    dev_t device;

    for (device = 0; device < MAX_DEVICES; device++) {
        if (!devices[device].used) {
            devices[device].used = 1;
            kstrcpy(devices[device].name, name);
            devices[device].magic = magic;
            devices[device].size = size;
            devices[device].read = read;
            devices[device].write = write;
            return device;
        }
    }

    return (dev_t)(-1);
}
