#include <stdint.h>
#include <stddef.h>
#include <devices/dev.h>
#include <lib/klib.h>

struct device devices[MAX_DEVICES];

void device_sync_worker(void *arg) {
    (void)arg;

    for (;;) {
        for (size_t i = 0; i < MAX_DEVICES; i++) {
            if (!devices[i].used)
                continue;
            switch (devices[i].flush(devices[i].magic)) {
                case 1:
                    /* flush is a no-op */
                    break;
                default:
                    yield(100);
                    break;
            }
        }
        yield(1000);
    }
}

uint64_t device_size(int dev) {
    return devices[dev].size;
}

int device_read(int dev, void *buf, uint64_t loc, size_t count) {
    int magic = devices[dev].magic;

    return devices[dev].read(magic, buf, loc, count);
}

int device_write(int dev, const void *buf, uint64_t loc, size_t count) {
    int magic = devices[dev].magic;

    return devices[dev].write(magic, buf, loc, count);
}

int device_flush(int dev) {
    int magic = devices[dev].magic;

    return devices[dev].flush(magic);
}

char *device_list(size_t index) {
    size_t i = 0;
    size_t j;

    for (j = 0; i < index; j++) {
        if (j == MAX_DEVICES)
            return (char *)0;
        if (devices[j].used)
            i++;
    }

    if (devices[j].used)
        return devices[j].name;
    else
        return (char *)0;
}

/* Returns a device ID by name, (dev_t)(-1) if not found. */
dev_t device_find(const char *name) {
    dev_t dev;

    for (dev = 0; dev < MAX_DEVICES; dev++) {
        if (!devices[dev].used)
            continue;
        if (!kstrcmp(devices[dev].name, name))
            return dev;
    }

    return (dev_t)(-1);
}

/* Registers a device. Returns the ID of the registered device. */
/* (dev_t)(-1) on error. */
dev_t device_add(const char *name, int magic, uint64_t size,
        int (*read)(int, void *, uint64_t, size_t),
        int (*write)(int, const void *, uint64_t, size_t),
        int (*flush)(int)) {

    dev_t dev;

    for (dev = 0; dev < MAX_DEVICES; dev++) {
        if (!devices[dev].used) {
            devices[dev].used = 1;
            kstrcpy(devices[dev].name, name);
            devices[dev].magic = magic;
            devices[dev].size = size;
            devices[dev].read = read;
            devices[dev].write = write;
            devices[dev].flush = flush;
            return dev;
        }
    }

    return (dev_t)(-1);
}
