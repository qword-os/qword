#include <fs/devfs/devfs.h>
#include <lib/alloc.h>
#include <lib/bit.h>
#include <lib/cmem.h>
#include <lib/dynarray.h>
#include <lib/klib.h>
#include <lib/part.h>
#include <lib/scsi.h>

#define MAX_CACHED_BLOCKS 8192
size_t CACHE_BLOCK_SIZE = 65536;
#define CACHE_PRESENT 1
#define CACHE_DIRTY   2

extern int debug_xhci;

lock_t scsi_lock = new_lock;

struct cached_block_t {
    uint32_t block;
    char *cache;
    int status;
};

struct scsi_dev_t {
    int intern_fd;
    int (*send_cmd)(int, char *, size_t, char *, size_t, int);
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
            device->cached_blocks[i].status)
            return i;
        if (free_pos == -1 && !device->cached_blocks[i].status)
            free_pos = i;
    }

    if (free_pos == -1)
        free_pos = 0;

    struct cached_block_t *cache = &device->cached_blocks[free_pos];
    cache->cache = kalloc(CACHE_BLOCK_SIZE);
    cache->block = lba;

    struct scsi_read_10_t read_cmd = {0};
    size_t length = CACHE_BLOCK_SIZE / device->block_size;
    uint64_t rlba = (CACHE_BLOCK_SIZE / device->block_size) * lba;
    read_cmd.op_code = 0x28;
    read_cmd.lba[0] = rlba >> 24;
    read_cmd.lba[1] = (rlba >> 16) & 0xFF;
    read_cmd.lba[2] = (rlba >> 8) & 0xFF;
    read_cmd.lba[3] = rlba & 0xFF;
    read_cmd.length[0] = length >> 8;
    read_cmd.length[1] = length & 0xFF;

    int ret = device->send_cmd(device->intern_fd, (char *)&read_cmd,
                               sizeof(struct scsi_read_10_t), cache->cache,
                               CACHE_BLOCK_SIZE, 0);
    if (ret)
        return -1;
    cache->status = CACHE_PRESENT;

    return free_pos;
}

static int scsi_read(int drive, void *buf, uint64_t loc, size_t count) {
    spinlock_acquire(&scsi_lock);
    struct scsi_dev_t *device =
        dynarray_getelem(struct scsi_dev_t, devices, drive);

    uint64_t progress = 0;
    while (progress < count) {
        int cache = cache_block(device, (loc + progress) / CACHE_BLOCK_SIZE);
        if (cache == -1)
            return -1;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % CACHE_BLOCK_SIZE;
        if (chunk > CACHE_BLOCK_SIZE - offset)
            chunk = CACHE_BLOCK_SIZE - offset;

        memcpy(buf + progress, &device->cached_blocks[cache].cache[offset],
               chunk);
        progress += chunk;
    }
    spinlock_release(&scsi_lock);
    return count;
}

static int scsi_write(int drive, void *buf, uint64_t loc, size_t count) {
    return -1;
}

int scsi_register(int intern_fd,
                  int (*send_cmd)(int, char *, size_t, char *, size_t, int),
                  size_t mtransf) {
    struct scsi_dev_t device = {0};
    device.intern_fd = intern_fd;
    device.send_cmd = send_cmd;
    // CACHE_BLOCK_SIZE = mtransf;

    kprint(KPRN_INFO, "SCSI INIT");
    /* read in block size */
    struct scsi_read_capacity_10_t cap_cmd = {0};
    cap_cmd.op_code = 0x25;
    uint64_t data = 0;
    int ret = send_cmd(intern_fd, (char *)&cap_cmd,
                       sizeof(struct scsi_read_capacity_10_t), (char *)&data,
                       sizeof(uint64_t), 0);
    kprint(KPRN_INFO, "SCSI get INFO");
    uint32_t lba_num = bswap(uint32_t, data & 0xFFFFFFFF);
    uint32_t block_size = bswap(uint32_t, data >> 32);
    kprint(KPRN_INFO, "lba num and block size: %x %x", lba_num, block_size);
    device.block_size = block_size;

    device.cached_blocks =
        kalloc(sizeof(struct cached_block_t) * MAX_CACHED_BLOCKS);
    ret = dynarray_add(struct scsi_dev_t, devices, &device);
    if (ret < 0) {
        kfree(device.cached_blocks);
        return -1;
    }

    struct device_t dev = {0};
    dev.calls = default_device_calls;
    char *dev_name = prefixed_itoa("usbd", 0, 10);
    strcpy(dev.name, dev_name);
    kprint(KPRN_INFO, "scsi: Initialised /dev/%s with maximum transfer size %X",
           dev_name, mtransf);
    kfree(dev_name);
    dev.intern_fd = ret;
    dev.size = lba_num * block_size;
    dev.calls.read = scsi_read;
    dev.calls.write = scsi_write;
    device_add(&dev);
    enum_partitions(dev_name, &dev);

    return 0;
}
