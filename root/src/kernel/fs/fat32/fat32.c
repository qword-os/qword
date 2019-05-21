#include <stdint.h>
#include <fd/vfs/vfs.h>
#include <lib/time.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <lib/errno.h>

#define SECTOR_SHIFT 9

#define SECTOR_TO_OFF(x) ((x) << SECTOR_SHIFT)
#define CLUSTER_TO_OFF(x, c, s) (SECTOR_TO_OFF((c) + ((x) - 2) * (s)))

static void read_off(int handle, uint64_t location, void *dst, size_t len) {
    lseek(handle, location, SEEK_SET);
    read(handle, dst, len);
}

struct fs_info {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint32_t sectors_per_fat;
    uint32_t root_dir_cluster;

    uint32_t fat_off;
    uint32_t cluster_begin_off;
};

struct fs_ent {
    uint8_t name[11];
    uint8_t attrib;
    uint32_t begin_cluster;
    uint32_t file_size;
};

struct mount_t {
};

static int mount_i = 0;

static int read_ent(int device, struct fs_info info, uint64_t cluster, size_t index, struct fs_ent *dest) {
    uint64_t off = CLUSTER_TO_OFF(cluster, info.cluster_begin_off, info.sectors_per_cluster) + index * 32;

    uint8_t buf[32];
    memset(buf, 0, 32);
    read_off(device, off, buf, 32);

    if (!*buf) return 0;

    memcpy(dest->name, buf, 11);
    dest->attrib = buf[0xB];
    uint16_t cluster_low = buf[0x1A] | (buf[0x1B] << 8);
    uint16_t cluster_hi = buf[0x14] | (buf[0x15] << 8);
    dest->begin_cluster = cluster_low | (cluster_hi << 16);
    memcpy(&dest->file_size, buf + 0x1C, 4);

    return 1;
}

static int fat32_mount(const char *source) {
    int device = open(source, O_RDONLY);

    kprint(KPRN_INFO, "fat32: mounting");

    struct fs_info info;
    read_off(device, 0x0B, &info.bytes_per_sector, 2);
    read_off(device, 0x0D, &info.sectors_per_cluster, 1);
    read_off(device, 0x0E, &info.reserved_sectors, 2);
    read_off(device, 0x10, &info.num_fats, 1);
    read_off(device, 0x24, &info.sectors_per_fat, 4);
    read_off(device, 0x2C, &info.root_dir_cluster, 4);

    info.fat_off = info.reserved_sectors;
    info.cluster_begin_off = (info.reserved_sectors + info.num_fats * info.sectors_per_fat);

    kprint(KPRN_DBG, "fat32: b/s: %u", info.bytes_per_sector);
    kprint(KPRN_DBG, "fat32: s/c: %u", info.sectors_per_cluster);
    kprint(KPRN_DBG, "fat32: reserved: %u", info.reserved_sectors);
    kprint(KPRN_DBG, "fat32: fats: %u", info.num_fats);
    kprint(KPRN_DBG, "fat32: s/f: %u", info.sectors_per_fat);
    kprint(KPRN_DBG, "fat32: root: %u", info.root_dir_cluster);
    kprint(KPRN_DBG, "fat32: offset to fat(sectors): %u", info.fat_off);
    kprint(KPRN_DBG, "fat32: offset to first cluster(sectors): %u", info.cluster_begin_off);

    struct fs_ent ent;

    for (size_t i = 0; read_ent(device, info, info.root_dir_cluster, i, &ent); i++) {
        if (ent.attrib == 0x0F) continue; // skip lfn
        if (ent.name[0] == 0xE5) continue;
        if (ent.name[0] == 0x05) ent.name[0] = 0xE5;

        char c[12];
        memset(c, 0, 12);
        memcpy(c, ent.name, 11);
        kprint(KPRN_DBG, "fat32: '%s' starts on %u cluster, is %u bytes long, attrib %x", c, ent.begin_cluster, ent.file_size, ent.attrib);
    
        uint64_t off = CLUSTER_TO_OFF(ent.begin_cluster, info.cluster_begin_off, info.sectors_per_cluster);

        uint8_t buf[info.sectors_per_cluster * info.bytes_per_sector];
        read_off(device, off, buf, info.sectors_per_cluster * info.bytes_per_sector);
        buf[511] = 0;

        kprint(KPRN_DBG, "fat32: '%s' contents: '%s'", c, buf);
    }

    return mount_i++;
}

void init_fs_fat32(void) {
    struct fs_t fat32 = {0};

    fat32 = default_fs_handler;
    strcpy(fat32.name, "fat32");
    fat32.mount = (void *)fat32_mount;
    /*fat32.open = fat32_open;
    fat32.read = fat32_read;
    fat32.lseek = fat32_seek;
    fat32.fstat = fat32_fstat;
    fat32.close = fat32_close;
    fat32.dup = fat32_dup;
    fat32.write = fat32_write;
    fat32.readdir = fat32_readdir;
    fat32.sync = fat32_sync;*/

    vfs_install_fs(&fat32);
}
