#include <stdint.h>
#include <fd/vfs/vfs.h>
#include <lib/time.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <lib/ht.h>
#include <lib/errno.h>

#define SECTORSHIFT 9

#define SECTOR_TO_OFFSET(x) ((x) << SECTORSHIFT)
#define CLUSTER_TO_OFFSET(x, c, s) (SECTOR_TO_OFFSET((c) + ((x) - 2) * (s)))

#define PATH_MAX   254
#define ATTRIB_DIR 0x10
#define SECTORSIZE 512

#define min(a, b) ((a) > (b) ? (b) : (a))

struct volumeid_t {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint32_t sectors_per_fat;
    uint32_t root_dir_cluster;
    uint16_t signature;
};

struct info_t {
    uint32_t fat_offset;
    uint32_t cluster_begin_offset;
};

struct mount_t {
    int    device;
    lock_t lock;
    struct volumeid_t volumeid;
    struct info_t     info;
};

struct fs_entry_t {
    uint8_t  name[11];
    uint8_t  attrib;
    uint32_t begin_cluster;
    uint32_t file_size;
};

struct handle_t {
    int free;
    int refcount;
    struct mount_t *mount;
    int flags;
    char path[PATH_MAX];
    struct fs_entry_t entry;
    size_t offset;
};

dynarray_new(struct mount_t, mounts);
dynarray_new(struct handle_t, handles);

static void read_offset(int handle, uint64_t location, void *dst, size_t len) {
    lseek(handle, location, SEEK_SET);
    read(handle, dst, len);
}

static int read_entry(struct mount_t *mnt, uint32_t cluster, size_t index, struct fs_entry_t *dest) {
    uint64_t offset = CLUSTER_TO_OFFSET(cluster, mnt->info.cluster_begin_offset,
        mnt->volumeid.sectors_per_cluster) + index * 32;

    uint8_t buf[32];
    memset(buf, 0, 32);
    read_offset(mnt->device, offset, buf, 32);

    if (!*buf) return 0;

    memcpy(dest->name, buf, 11);
    dest->attrib = buf[0xB];
    uint16_t cluster_low = buf[0x1A] | (buf[0x1B] << 8);
    uint16_t cluster_hi = buf[0x14] | (buf[0x15] << 8);
    dest->begin_cluster = cluster_low | (cluster_hi << 16);
    memcpy(&dest->file_size, buf + 0x1C, 4);

    return 1;
}

static inline uint32_t next_cluster(uint32_t cluster, uint32_t *fat, uint32_t fat_len) {
    if (cluster >= fat_len / 4) return 0;
    if (fat[cluster] >= 0xFFFFFFF8) return 0;
    return fat[cluster];
}

// TODO: actually parse lfn and only resort to this when there's no lfn
static int compare_fs_ent_and_path(struct fs_entry_t *ent, const char *path) {
    const char *ext = strchrnul(path, '.');

    char name[12];
    memset(name, 0, 12);
    memset(name, ' ', 11);

    if (*(ext - 1) == '.') {
        memcpy(name, path, min(ext - path - 1, 8));
        memcpy(name + 8, ext, min(strlen(ext), 3));
    } else {
        memcpy(name, path, min(strlen(path), 8));
    }

    for (size_t i = 0; i < 11; i++)
        name[i] = toupper(name[i]);

    return memcmp(ent->name, name, 11);
}

static struct fs_entry_t parse_path(struct mount_t *mnt, const char *path) {
    struct fs_entry_t root = {
        .name = {' '},
        .attrib = ATTRIB_DIR,
        .begin_cluster = mnt->volumeid.root_dir_cluster,
        .file_size = 0
    };

    if (!strcmp(path, "/"))
        return root;

    uint32_t cluster = mnt->volumeid.root_dir_cluster;

    path++;

    size_t fat_len = mnt->volumeid.sectors_per_fat * SECTORSIZE;
    uint32_t *fat = kalloc(fat_len);
    read_offset(mnt->device, SECTOR_TO_OFFSET(mnt->info.fat_offset), fat, fat_len);

    struct fs_entry_t ent = {0};
    for (size_t i = 0; read_entry(mnt, cluster, i, &ent); i++) {
        if (i == mnt->volumeid.sectors_per_cluster * 16) {
            i = 0;
            cluster = next_cluster(cluster, fat, fat_len);
        }

        if ((ent.attrib & 0x0F) == 0x0F) continue; // skip lfn for now
        if (ent.name[0] == 0xE5) continue;
        if (ent.name[0] == 0x05) ent.name[0] = 0xE5;

        char sub_elem[PATH_MAX] = {0};
        int top_level = 0;

        const char *slash = strchrnul(path, '/');
        if (*slash == '\0') {
            memcpy(sub_elem, path, strlen(path));
            top_level = 1;
        } else {
            memcpy(sub_elem, path, slash - path);
            sub_elem[slash - path] = '\0';
        }

        if (!compare_fs_ent_and_path(&ent, sub_elem)) {

            if (top_level) {
                // found file we were looking for
                kfree(fat);
                return ent;
            } else {
                cluster = ent.begin_cluster;
                i = 0;
            }
        }
    }

    kfree(fat);
    struct fs_entry_t err = {0};
    return err;
}

static int fat32_mount(const char *source) {
    // Open the device.
    int device = open(source, O_RDWR);

    if (device == -1) {
        kprint(KPRN_ERR, "fat32: failed to open mount source!");
        return -1;
    }

    // Start the mounting process.
    kprint(KPRN_INFO, "fat32: mounting");

    // Fill the mount struct.
    struct mount_t mount;

    mount.device = device;
    mount.lock   = new_lock;

    // Read info from offsets for the volume ID.
    read_offset(device, 0x0b,  &mount.volumeid.bytes_per_sector, 2);
    read_offset(device, 0x0d,  &mount.volumeid.sectors_per_cluster, 1);
    read_offset(device, 0x0e,  &mount.volumeid.reserved_sectors, 2);
    read_offset(device, 0x10,  &mount.volumeid.num_fats, 1);
    read_offset(device, 0x24,  &mount.volumeid.sectors_per_fat, 4);
    read_offset(device, 0x2c,  &mount.volumeid.root_dir_cluster, 4);
    read_offset(device, 0x1fe, &mount.volumeid.signature, 2);

    // Some calculations for the extra info.
    mount.info.fat_offset = mount.volumeid.reserved_sectors;
    mount.info.cluster_begin_offset = mount.volumeid.reserved_sectors + mount.volumeid.num_fats * mount.volumeid.sectors_per_fat;

    // Check fields.
    if (mount.volumeid.signature != 0xaa55) {
        kprint(KPRN_ERR, "fat32: signature is incorrect");
        return -1;
    }

    // Finally add the mount.
    return dynarray_add(struct mount_t, mounts, &mount);
}

static int fat32_open(const char *path, int flags, int mount) {
    // Get the mount.
    struct mount_t *mnt = dynarray_getelem(struct mount_t, mounts, mount);
    if (!mnt)
        return -1;

    // Lock mount.
    spinlock_acquire(&mnt->lock);

    // Create the handle.
    struct handle_t handle = {0};
    strcpy(handle.path, path);
    handle.refcount = 1;
    handle.mount    = mnt;
    handle.flags    = flags;
    handle.offset   = 0;
    handle.entry    = parse_path(mnt, path);

    // Add handle to the list, unlock and return the mount to the list.
    int ret = dynarray_add(struct handle_t, handles, &handle);
    spinlock_release(&mnt->lock);
    dynarray_unref(mounts, mount);
    return ret;
}

static int fat32_close(int handle) {
    // Get the handle.
    struct handle_t *hdl = dynarray_getelem(struct handle_t, handles, handle);

    if (!hdl) {
        errno = EBADF;
        return -1;
    }

    // Get the mount and lock.
    struct mount_t *mnt = hdl->mount;
    spinlock_acquire(&mnt->lock);

    // Reduce the reference count, if 0, delete it.
    if (!(--hdl->refcount))
        dynarray_remove(handles, handle);

    dynarray_unref(handles, handle);
    spinlock_release(&mnt->lock);

    return 0;
}

static int fat32_read(int handle, void *buf, size_t count) {
    // Get the handle.
    struct handle_t *hdl = dynarray_getelem(struct handle_t, handles, handle);

    if (!hdl) {
        errno = EBADF;
        return -1;
    }

    struct mount_t *mnt = hdl->mount;
    spinlock_acquire(&mnt->lock);

    struct fs_entry_t ent = hdl->entry;

    if (hdl->offset >= ent.file_size) {
        dynarray_unref(handles, handle);
        spinlock_release(&mnt->lock);
        return 0;
    }

    size_t fat_len = mnt->volumeid.sectors_per_fat * SECTORSIZE;
    uint32_t *fat = kalloc(fat_len);
    read_offset(mnt->device, SECTOR_TO_OFFSET(mnt->info.fat_offset), fat, fat_len);

    int read_size = min(count, ent.file_size);

    size_t bytes_per_cluster = mnt->volumeid.sectors_per_cluster * SECTORSIZE;
    size_t cluster_bytes = ((read_size + bytes_per_cluster - 1) / bytes_per_cluster) * bytes_per_cluster;

    size_t cluster_offset = (hdl->offset + bytes_per_cluster - 1) / bytes_per_cluster;
    size_t cluster = ent.begin_cluster;
    while (cluster_offset--) cluster = next_cluster(cluster, fat, fat_len);
    size_t index = 0;

    while (index + hdl->offset < read_size) {
        size_t off = CLUSTER_TO_OFFSET(cluster, mnt->info.cluster_begin_offset,
                                    mnt->volumeid.sectors_per_cluster) + (hdl->offset % bytes_per_cluster);

        size_t read_amount = min(cluster_bytes - index, bytes_per_cluster);

        if (read_amount > read_size - index) read_amount = read_size - index;

        read_offset(mnt->device, off, (buf + index), read_amount);

        index += bytes_per_cluster;

        if (index + hdl->offset < read_size) {
            size_t old_cluster = cluster;
            cluster = next_cluster(cluster, fat, fat_len);
        }
    }

    kfree(fat);
    hdl->offset += read_size;

    dynarray_unref(handles, handle);
    spinlock_release(&mnt->lock);

    return read_size;
}

static int fat32_dup(int handle) {
    // Get the handle.
    struct handle_t *hdl = dynarray_getelem(struct handle_t, handles, handle);

    if (!hdl) {
        errno = EBADF;
        return -1;
    }

    // Get the mount and lock.
    struct mount_t *mnt = hdl->mount;
    spinlock_acquire(&mnt->lock);

    hdl->refcount++;

    dynarray_unref(handles, handle);
    spinlock_release(&mnt->lock);

    return 0;
}

static int fat32_fstat(int handle, struct stat *st) {
    // Get the handle.
    struct handle_t *hdl = dynarray_getelem(struct handle_t, handles, handle);

    if (!hdl) {
        errno = EBADF;
        return -1;
    }

    // Get the mount and lock.
    struct mount_t *mnt = hdl->mount;
    spinlock_acquire(&mnt->lock);

    hdl->refcount++;

    int is_dir = hdl->entry.attrib & ATTRIB_DIR;

    st->st_dev = mnt->device;
    st->st_ino = 1;
    st->st_nlink = 1;
    st->st_uid = 1;
    st->st_gid = 1;
    st->st_rdev = 0;
    st->st_size = hdl->entry.file_size;
    st->st_blksize = 512;
    st->st_blocks = (st->st_size + 512 - 1) / 512;
    st->st_atim.tv_sec = 0;
    st->st_atim.tv_nsec = 0;
    st->st_mtim.tv_sec = 0;
    st->st_mtim.tv_nsec = 0;
    st->st_ctim.tv_sec = 0;
    st->st_ctim.tv_nsec = 0;

    st->st_mode |= is_dir ? S_IFDIR : S_IFREG;

    dynarray_unref(handles, handle);
    spinlock_release(&mnt->lock);
    return 0;
}

void init_fs_fat32(void) {
    struct fs_t fat32 = {0};

    fat32 = default_fs_handler;
    strcpy(fat32.name, "fat32");
    fat32.mount = (void *)fat32_mount;
    fat32.open = fat32_open;
    fat32.close = fat32_close;
    fat32.read = fat32_read;
    fat32.dup = fat32_dup;
    fat32.fstat = fat32_fstat;

    vfs_install_fs(&fat32);
}
