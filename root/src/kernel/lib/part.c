#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <lib/alloc.h>
#include <fd/fd.h>
#include <fd/vfs/vfs.h>
#include <fs/devfs/devfs.h>

struct partinfo_t {
    int fd;
    uint64_t first_sect;
    uint64_t sect_count;
    struct device_calls_t dev_calls;
};

dynarray_new(struct partinfo_t, partinfos);

static int part_read(int pi, void *buf, uint64_t loc, size_t count) {
    struct partinfo_t *partinfo = dynarray_getelem(struct partinfo_t, partinfos, pi);

    if (!partinfo)
        return 0;

    if (loc >= partinfo->sect_count * 512)
        return 0;

    int ret = partinfo->dev_calls.read(
                partinfo->fd,
                buf,
                loc + partinfo->first_sect * 512,
                count);

    dynarray_unref(partinfos, pi);

    return ret;
}

static int part_write(int pi, const void *buf, uint64_t loc, size_t count) {
    struct partinfo_t *partinfo = dynarray_getelem(struct partinfo_t, partinfos, pi);

    if (!partinfo)
        return 0;

    if (loc >= partinfo->sect_count * 512)
        return 0;

    int ret = partinfo->dev_calls.write(
                partinfo->fd,
                buf,
                loc + partinfo->first_sect * 512,
                count);

    dynarray_unref(partinfos, pi);

    return ret;
}

static int create_partition_device(const char *device, int part_no, uint64_t first_sect, uint64_t sect_count, struct device_t *dev_struct) {
    char *dev_name = kalloc(strlen(device) + 16);
    strcpy(dev_name, device);
    strcpy(dev_name + strlen(device), "p");
    char *part_no_ascii = prefixed_itoa("", part_no, 10);
    strcpy(dev_name + strlen(device) + 1, part_no_ascii);
    kfree(part_no_ascii);

    kprint(KPRN_DBG, "dev_name: %s", dev_name);

    struct partinfo_t partinfo = {0};
    partinfo.fd = dev_struct->intern_fd;
    partinfo.first_sect = first_sect;
    partinfo.sect_count = sect_count;
    partinfo.dev_calls = dev_struct->calls;
    int fd = dynarray_add(struct partinfo_t, partinfos, &partinfo);

    struct device_t new_device = {0};
    new_device.calls = default_device_calls;
    strcpy(new_device.name, dev_name);
    new_device.intern_fd = fd;
    new_device.size = sect_count * 512;
    new_device.calls.read = part_read;
    new_device.calls.write = part_write;
    new_device.calls.flush = 0;
    device_add(&new_device);

    return 0;
}


static int enum_gpt(int fd, const char *device, struct device_t *dev_struct) {

    return -1;
}

struct mbr_entry {
    uint8_t status;
    uint8_t chs_first_sect[3];
    uint8_t type;
    uint8_t chs_last_sect[3];
    uint32_t first_sect;
    uint32_t sect_count;
};

static int enum_mbr(int fd, const char *device, struct device_t *dev_struct) {
    kprint(KPRN_DBG, "trying to parse MBR on device /dev/%s", device);

    // check if actually mbr
    uint16_t hint;
    lseek(fd, 444, SEEK_SET);
    read(fd, &hint, 2);
    if (hint && hint != 0x5a5a)
        return -1;
    kprint(KPRN_DBG, "hint passed");

    kprint(KPRN_DBG, "reading entries");
    struct mbr_entry entries[4];
    read(fd, entries, sizeof(struct mbr_entry) * 4);

    for (int i = 0; i < 4; i++) {
        if (!entries[i].type) {
            kprint(KPRN_DBG, "partition %d is empty", i);
            continue;
        }
        kprint(KPRN_DBG, "partition %d first_sect: %d", i, entries[i].first_sect);
        kprint(KPRN_DBG, "partition %d sect_count: %d", i, entries[i].sect_count);
        create_partition_device(device, i, entries[i].first_sect, entries[i].sect_count, dev_struct);
    }

    close(fd);

    return 0;
}

int enum_partitions(const char *device, struct device_t *dev_struct) {
    char *dev_path = kalloc(5 + strlen(device) + 1);

    if (!dev_path)
        return -1;

    strcpy(dev_path, "/dev/");
    strcpy(dev_path + 5, device);
    int fd = open(dev_path, O_RDONLY);
    kfree(dev_path);

    if (fd < 0)
        return -1;

    // try various partition schemes

    if (!enum_gpt(fd, device, dev_struct))
        return 0;

    if (!enum_mbr(fd, device, dev_struct))
        return 0;

    return -1;
}
