#include <devices/dev.h>

/** /dev/null **/

static int null_write(int unused1, const void *unused2, uint64_t unused3, size_t count) {
    (void)unused1;
    (void)unused2;
    (void)unused3;

    return (int)count;
}

static int null_read(int unused1, const void *unused2, uint64_t unused3, size_t unused4) {
    (void)unused1;
    (void)unused2;
    (void)unused3;
    (void)unused4;

    return 0;
}

static int null_flush(int unused) {
    (void)unused;

    return 1;
}

/** /dev/zero **/

static int zero_write(int unused1, const void *unused2, uint64_t unused3, size_t count) {
    (void)unused1;
    (void)unused2;
    (void)unused3;

    return (int)count;
}

static int zero_read(int unused1, const void *buf, uint64_t unused2, size_t count) {
    (void)unused1;
    (void)unused2;

    uint8_t *buf1 = buf;

    for (size_t i = 0; i < count; i++)
        buf1[i] = 0;

    return count;
}

static int zero_flush(int unused) {
    (void)unused;

    return 1;
}

void init_streams(void) {
    struct device_t device = {0};
    kstrcpy(device.name, "null");
    device.intern_fd = 0;
    device.size = 0;
    device.calls.read = null_read;
    device.calls.write = null_write;
    device.calls.flush = null_flush;
    device_add(&device);
    kstrcpy(device.name, "zero");
    device.intern_fd = 0;
    device.size = 0;
    device.calls.read = zero_read;
    device.calls.write = zero_write;
    device.calls.flush = zero_flush;
    device_add(&device);
}
