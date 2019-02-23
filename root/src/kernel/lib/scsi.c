#include <lib/scsi.h>
#include <fs/devfs/devfs.h>
#include <lib/dynarray.h>
#include <lib/bit.h>
#include <lib/klib.h>
#include <lib/alloc.h>

#define MAX_CACHED_BLOCKS 8192
#define CACHE_PRESENT 1
#define CACHE_DIRTY 2

struct cached_block_t {
    uint32_t block;
    char *cache;
    int status;
};

struct scsi_dev_t {
    int intern_fd;
    int (*send_cmd) (int, char *, size_t, char *, size_t, int);
    size_t block_size;
    struct cached_block_t *cached_blocks;
};

dynarray_new(struct scsi_dev_t, devices);

static int cache_block(struct scsi_dev_t *device, uint32_t lba) {
    if (!device) {
        errno = EINVAL;
        return -1;
    }

    int free_pos = -1;
    for (int i = 0; i < MAX_CACHED_BLOCKS; i++) {
        if (device->cached_blocks[i].block == lba &&
                device->cached_blocks[i].status) return i;
        if (free_pos == -1 && !device->cached_blocks[i].status) free_pos = i;
    }

    if (free_pos == -1)
        free_pos = 0;

    struct cached_block_t *cache = &device->cached_blocks[free_pos];
    cache->cache = kalloc(device->block_size);
    cache->block = lba;

    struct scsi_read_10_t read_cmd = {0};
    read_cmd.op_code = 0x28;
    read_cmd.lba = bswap(uint32_t, lba);
    read_cmd.length = 1;

    int ret = device->send_cmd(device->intern_fd, (char *)&read_cmd, sizeof(
                struct scsi_read_10_t), cache->cache, device->block_size, 0);
    if (ret) return -1;
    cache->status = CACHE_PRESENT;

    return free_pos;
}

static int scsi_read(int drive, void *buf, uint64_t loc, size_t count) {
    struct scsi_dev_t *device = dynarray_getelem(struct scsi_dev_t,
            devices, drive);

    uint64_t progress = 0;
    while (progress < count) {
        int cache = cache_block(device, (loc + progress) / device->block_size);
        if (cache == -1) return -1;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % device->block_size;
        if (chunk > device->block_size - offset)
            chunk = device->block_size - offset;

        kmemcpy(buf + progress, device->cached_blocks[cache].cache + offset, chunk);
        progress += chunk;
    }

    return count;
}

int scsi_register(int intern_fd,
        int (*send_cmd) (int, char *, size_t, char *, size_t, int)) {
    struct scsi_dev_t device = {0};
    device.intern_fd = intern_fd;
    device.send_cmd = send_cmd;

    /* read in block size */
    struct scsi_read_capacity_10_t cap_cmd = {0};
    cap_cmd.op_code = 0x25;
    uint64_t data = 0;
    int ret = send_cmd(intern_fd, (char *)&cap_cmd, sizeof(struct
                scsi_read_capacity_10_t), (char *)&data, sizeof(uint64_t), 0);
    uint32_t lba_num = bswap(uint32_t, data & 0xFFFFFFFF);
    uint32_t block_size = bswap(uint32_t, data >> 32);
    device.block_size = block_size;

    device.cached_blocks = kalloc(sizeof(struct cached_block_t) *
            MAX_CACHED_BLOCKS);
    ret = dynarray_add(struct scsi_dev_t, devices, &device);
    if (ret < 0) {
        kfree(device.cached_blocks);
        return -1;
    }

    struct device_t dev = {0};
    dev.calls = default_device_calls;
    dev.intern_fd = ret;
    kstrcpy(dev.name, "usbd0");
    dev.calls.read = scsi_read;
    dev.size = lba_num * block_size;
    device_add(&dev);
    kprint(KPRN_INFO, "scsi: Initialised %s", dev.name);

    return 0;
}
