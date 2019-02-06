#include <fs/devfs/devfs.h>
#include <lib/klib.h>

/** /dev/null **/

static int null_write(int unused1, const void *unused2, uint64_t unused3, size_t count) {
    (void)unused1;
    (void)unused2;
    (void)unused3;

    return (int)count;
}

static int null_read(int unused1, void *unused2, uint64_t unused3, size_t unused4) {
    (void)unused1;
    (void)unused2;
    (void)unused3;
    (void)unused4;

    return 0;
}

/** /dev/zero **/

static int zero_write(int unused1, const void *unused2, uint64_t unused3, size_t count) {
    (void)unused1;
    (void)unused2;
    (void)unused3;

    return (int)count;
}

static int zero_read(int unused1, void *buf, uint64_t unused2, size_t count) {
    (void)unused1;
    (void)unused2;

    uint8_t *buf1 = buf;

    for (size_t i = 0; i < count; i++)
        buf1[i] = 0;

    return count;
}

/** initialise **/

void init_dev_streams(void) {
    struct device_t device = {0};

    device.calls = default_device_calls;

    kstrcpy(device.name, "null");
    device.calls.read = null_read;
    device.calls.write = null_write;
    device_add(&device);

    kstrcpy(device.name, "zero");
    device.calls.read = zero_read;
    device.calls.write = zero_write;
    device_add(&device);
}
