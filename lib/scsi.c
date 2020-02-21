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
#define CACHE_READY 1
#define CACHE_DIRTY   2

extern int debug_xhci;

struct cached_block_t {
    uint32_t block;
    char *cache;
    int status;
};

lock_t scsi_lock;
struct scsi_dev_t {
    int intern_fd;
    int (*send_cmd)(int, char *, size_t, char *, size_t, int);
    size_t block_size;
    struct cached_block_t *cache;
    lock_t lock;
    size_t overwritten_slot;
};

dynarray_new(struct scsi_dev_t, devices);

static int scsi_internal_read(struct scsi_dev_t *device, void* buf, uint32_t lba, size_t size) {
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
    return device->send_cmd(device->intern_fd, (char *)&read_cmd,
                               sizeof(struct scsi_read_10_t), buf,
                               CACHE_BLOCK_SIZE, 0);
}

static int scsi_internal_write(struct scsi_dev_t *device, void* buf, uint32_t lba, size_t size) {
    struct scsi_read_10_t read_cmd = {0};
    size_t length = CACHE_BLOCK_SIZE / device->block_size;
    uint64_t rlba = (CACHE_BLOCK_SIZE / device->block_size) * lba;
    read_cmd.op_code = 0x2A;
    read_cmd.lba[0] = rlba >> 24;
    read_cmd.lba[1] = (rlba >> 16) & 0xFF;
    read_cmd.lba[2] = (rlba >> 8) & 0xFF;
    read_cmd.lba[3] = rlba & 0xFF;
    read_cmd.length[0] = length >> 8;
    read_cmd.length[1] = length & 0xFF;
    return device->send_cmd(device->intern_fd, (char *)&read_cmd,
                            sizeof(struct scsi_read_10_t), buf,
                            CACHE_BLOCK_SIZE, 1);
}

static int find_block(struct scsi_dev_t *device, uint64_t block) {
    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++)
        if ((device->cache[i].block == block)
            && (device->cache[i].status))
            return i;

    return -1;
}

static int cache_block(struct scsi_dev_t *device, uint64_t block) {
    int targ;
    int ret;

    //Find empty block
    for (targ = 0; targ < MAX_CACHED_BLOCKS; targ++)
        if (!device->cache[targ].status) goto fnd;

    //Slot not found, overwrite another
    if (device->overwritten_slot == MAX_CACHED_BLOCKS)
        device->overwritten_slot = 0;

    targ = device->overwritten_slot++;

    //Flush device cache
    if (device->cache[targ].status == CACHE_DIRTY) {
        ret = scsi_internal_write(device, device->cache[targ].cache, device->cache[targ].block, CACHE_BLOCK_SIZE);

        if (ret == -1) return -1;
    }

    goto notfnd;

    fnd:
    device->cache[targ].cache = kalloc(CACHE_BLOCK_SIZE);

    notfnd:
    ret = scsi_internal_read(device, device->cache[targ].cache, block, CACHE_BLOCK_SIZE);

    if (ret == -1) return -1;

    device->cache[targ].block = block;
    device->cache[targ].status = CACHE_READY;

    return targ;
}

static int scsi_read(int drive, void *buf, uint64_t loc, size_t count) {
    uint64_t progress = 0;
    struct scsi_dev_t *device =
            dynarray_getelem(struct scsi_dev_t, devices, drive);
    spinlock_acquire(&scsi_lock);

    while (progress < count) {
        //cache the block
        uint64_t sect = (loc + progress) / CACHE_BLOCK_SIZE;
        int slot = find_block(device, sect);
        if (slot == -1) {
            slot = cache_block(device, sect);
            if (slot == -1) {
                spinlock_release(&scsi_lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % CACHE_BLOCK_SIZE;
        if (chunk > CACHE_BLOCK_SIZE - offset)
            chunk = CACHE_BLOCK_SIZE - offset;

        memcpy(buf + progress, (&device->cache[slot].cache[offset]), chunk);
        progress += chunk;
    }
    spinlock_release(&scsi_lock);

    return (int)count;
}

static int scsi_write(int drive, const void *buf, uint64_t loc, size_t count) {
    uint64_t progress = 0;
    struct scsi_dev_t *device =
            dynarray_getelem(struct scsi_dev_t, devices, drive);
    spinlock_acquire(&scsi_lock);

    while (progress < count) {
        //cache the block
        uint64_t sect = (loc + progress) / (CACHE_BLOCK_SIZE);
        int slot = find_block(device, sect);
        if (slot == -1) {
            slot = cache_block(device, sect);
            if (slot == -1) {
                spinlock_release(&scsi_lock);
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % CACHE_BLOCK_SIZE;
        if (chunk > CACHE_BLOCK_SIZE - offset)
            chunk = CACHE_BLOCK_SIZE - offset;

        memcpy(&(device->cache[slot].cache[offset]), buf + progress, chunk);
        device->cache[slot].status = CACHE_DIRTY;
        progress += chunk;
    }
    spinlock_release(&scsi_lock);

    return (int)count;
}

static int scsi_flush_cache(int drive) {
    struct scsi_dev_t *device = dynarray_getelem(struct scsi_dev_t, devices, drive);
    spinlock_acquire(&scsi_lock);

    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++) {
        if (device->cache[i].status == CACHE_DIRTY) {
            int ret = scsi_internal_write(device, device->cache[i].cache, device->cache[i].block, CACHE_BLOCK_SIZE);
            if (ret == -1) {
                spinlock_release(&scsi_lock);
                return -1;
            }

            device->cache[i].status = CACHE_READY;
        }
    }
    spinlock_release(&scsi_lock);

    return 0;
}

int scsi_register(int intern_fd,
                  int (*send_cmd)(int, char *, size_t, char *, size_t, int),
                  size_t mtransf) {
    struct scsi_dev_t device = {0};
    device.intern_fd = intern_fd;
    device.send_cmd = send_cmd;
    device.lock = new_lock;
    scsi_lock = new_lock;
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

    device.cache =
        kalloc(sizeof(struct cached_block_t) * MAX_CACHED_BLOCKS);
    ret = dynarray_add(struct scsi_dev_t, devices, &device);
    if (ret < 0) {
        kfree(device.cache);
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
    dev.calls.flush = scsi_flush_cache;
    device_add(&dev);
    enum_partitions(dev_name, &dev);

    return 0;
}
