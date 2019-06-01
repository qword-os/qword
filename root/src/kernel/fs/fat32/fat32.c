#include <stdint.h>
#include <fd/vfs/vfs.h>
#include <lib/time.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <lib/errno.h>

#define SECTOR_SHIFT 9

#define SECTOR_TO_OFF(x) ((x) << SECTOR_SHIFT)
#define CLUSTER_TO_OFF(x, c, s) (SECTOR_TO_OFF((c) + ((x) - 2) * (s)))

#define PATH_MAX 254

#define ATTRIB_DIR 0x10

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
    int device;
    struct fs_info info;
};

struct handle_t {
    int free;
    int refcount;
    int mount;
    int flags;
    char path[PATH_MAX];
    struct fs_ent ent;
    size_t offset;
};

static struct mount_t *mounts = NULL;
static int mount_i = 0;

static struct handle_t *handles = NULL;
static int handle_i = 0;

static lock_t fat32_lock = new_lock;

static int read_ent(struct mount_t *mnt, uint32_t cluster,
                    size_t index, struct fs_ent *dest) {
    uint64_t off = CLUSTER_TO_OFF(cluster, mnt->info.cluster_begin_off,
                                    mnt->info.sectors_per_cluster) + index * 32;

    uint8_t buf[32];
    memset(buf, 0, 32);
    read_off(mnt->device, off, buf, 32);

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

static int fat32_mount(const char *source) {
    spinlock_acquire(&fat32_lock);
    int device = open(source, O_RDONLY);

    if (device < 0) {
        kprint(KPRN_ERR, "fat32: failed to open mount source!");
        spinlock_release(&fat32_lock);
        return -1;
    }

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
    
    mounts = krealloc(mounts, sizeof(struct mount_t) * (mount_i + 1));
    
    struct mount_t *mnt = &mounts[mount_i];
   
    mnt->device = device;
    mnt->info = info;

    size_t mount_new = mount_i;
    mount_i++;

    spinlock_release(&fat32_lock);

    return mount_new;
}

#define min(a, b) ((a) > (b) ? (b) : (a))
#define islower(c) ((c) >= 'a' && (c) <= 'z')

int toupper(int c) {
    if (islower(c)) return c - ('a' - 'A');
    else return c;
}

// TODO: actually parse lfn and only resort to this when there's no lfn
static int compare_fs_ent_and_path(struct fs_ent *ent, const char *path) {
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

    for (size_t i = 0; i < 11; i++) name[i] = toupper(name[i]);

    return memcmp(ent->name, name, 11);
}

static struct fs_ent parse_path(int mount, const char *path) {
    struct mount_t *mnt = &mounts[mount];

    struct fs_ent root = {
        .name = {' '},
        .file_size = 0,
        .begin_cluster = mnt->info.root_dir_cluster,
        .attrib = ATTRIB_DIR,
    };
    if (path[0] == '/' && path[1] == '\0') return root;
    
    uint32_t cluster = mnt->info.root_dir_cluster;
   
    path++;

    size_t fat_len = mnt->info.sectors_per_fat * 512;
    uint32_t *fat = kalloc(fat_len);
    read_off(mnt->device, SECTOR_TO_OFF(mnt->info.fat_off), fat, fat_len);
 
    struct fs_ent ent = {0};
    for (size_t i = 0; read_ent(mnt, cluster, i, &ent); i++) {
        if (i == mnt->info.sectors_per_cluster * 16) {
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
    struct fs_ent err = {0};
    return err;
}

static int create_handle(struct handle_t handle) {
    for (int i = 0; i < handle_i; i++) {
        if (handles[i].free) {
            handles[i] = handle;
            return i;
        }
    }
    int handle_n = 0;
    handles = krealloc(handles, (handle_i + 1) * sizeof(struct handle_t));
    handle_n = handle_i++;
    handles[handle_n] = handle;
    return handle_n;
}

static int fat32_open(const char *path, int flags, int mount) {
    spinlock_acquire(&fat32_lock);
    
    struct handle_t handle = {0};
    strcpy(handle.path, path);
    handle.flags = flags;
    handle.mount = mount;
    handle.refcount = 1;
    handle.free = 0;
    handle.offset = 0;

    handle.ent = parse_path(mount, path);
    
    int hnd = create_handle(handle);

    spinlock_release(&fat32_lock);

    return hnd;
}

static int fat32_read(int handle, void *buf, size_t count) {
    spinlock_acquire(&fat32_lock);
    
    if (handle >= handle_i) {
        spinlock_release(&fat32_lock);
        return -1;
    }

    if (handles[handle].free) {
        spinlock_release(&fat32_lock);
        return -1;
    }

    struct mount_t *mnt = &mounts[handles[handle].mount];
    struct fs_ent *ent = &handles[handle].ent;

    if (handles[handle].offset >= ent->file_size) {
        spinlock_release(&fat32_lock);
        return 0;
    }
    size_t fat_len = mnt->info.sectors_per_fat * 512;
    uint32_t *fat = kalloc(fat_len);
    read_off(mnt->device, SECTOR_TO_OFF(mnt->info.fat_off), fat, fat_len);

    int read_size = min(count, ent->file_size);

    size_t bytes_per_cluster = mnt->info.sectors_per_cluster * 512;
    size_t cluster_bytes = ((read_size + bytes_per_cluster - 1) / bytes_per_cluster) * bytes_per_cluster;
    
    size_t cluster_offset = (handles[handle].offset + bytes_per_cluster - 1) / bytes_per_cluster;
    size_t cluster = ent->begin_cluster;
    while (cluster_offset--) cluster = next_cluster(cluster, fat, fat_len);
    size_t index = 0;

    while (index + handles[handle].offset < read_size) {
        size_t off = CLUSTER_TO_OFF(cluster, mnt->info.cluster_begin_off,
                                    mnt->info.sectors_per_cluster) + (handles[handle].offset % bytes_per_cluster);
    
        size_t read_amount = min(cluster_bytes - index, bytes_per_cluster);
        if (read_amount > read_size - index) read_amount = read_size - index;

        read_off(mnt->device, off, (buf + index), read_amount);
        
        index += bytes_per_cluster;

        if (index + handles[handle].offset < read_size) {
            size_t old_cluster = cluster;
            cluster = next_cluster(cluster, fat, fat_len);
        }
    }

    kfree(fat);
    handles[handle].offset += read_size;

    spinlock_release(&fat32_lock);

    return read_size;
}

static int fat32_write(int handle, const void *buf, size_t count) {
    return -1;      // R/O for now
}

static int fat32_close(int handle) {
    spinlock_acquire(&fat32_lock);
    if (handle >= handle_i) {
        spinlock_release(&fat32_lock);
        return -1;
    }

    if (handles[handle].free) {
        spinlock_release(&fat32_lock);
        return -1;
    }
    if (!(--handles[handle].refcount))
        handles[handle].free = 1;


    spinlock_release(&fat32_lock);
    return 0;
}

static int fat32_dup(int handle) {
    spinlock_acquire(&fat32_lock);
    if (handle < 0) {
        spinlock_release(&fat32_lock);
        return -1;
    }

    if (handle >= handle_i) {
        spinlock_release(&fat32_lock);
        return -1;
    }
    if (handles[handle].free) {
        spinlock_release(&fat32_lock);
        return -1;
    }
    handles[handle].refcount++;

    spinlock_release(&fat32_lock);

    return 0;
}

static int fat32_fstat(int handle, struct stat *st) {
    spinlock_acquire(&fat32_lock);
    if (handle < 0) {
        spinlock_release(&fat32_lock);
        return -1;
    }

    if (handle >= handle_i) {
        spinlock_release(&fat32_lock);
        return -1;
    }
    if (handles[handle].free) {
        spinlock_release(&fat32_lock);
        return -1;
    }

    int is_dir = handles[handle].ent.attrib & ATTRIB_DIR;

    st->st_dev = mounts[handles[handle].mount].device;
    st->st_ino = 1;
    st->st_nlink = 1;
    st->st_uid = 1;
    st->st_gid = 1;
    st->st_rdev = 0;
    st->st_size = handles[handle].ent.file_size;
    st->st_blksize = 512;
    st->st_blocks = (st->st_size + 512 - 1) / 512;
    st->st_atim.tv_sec = 0;
    st->st_atim.tv_nsec = 0;
    st->st_mtim.tv_sec = 0;
    st->st_mtim.tv_nsec = 0;
    st->st_ctim.tv_sec = 0;
    st->st_ctim.tv_nsec = 0;

    st->st_mode |= is_dir ? S_IFDIR : S_IFREG;

    spinlock_release(&fat32_lock);
    return 0;
}

static int fat32_readdir(int handle, struct dirent *d) {
    return 0;
}

static int fat32_sync() {
    return 0;
}

void init_fs_fat32(void) {
    struct fs_t fat32 = {0};

    fat32 = default_fs_handler;
    strcpy(fat32.name, "fat32");
    fat32.mount = (void *)fat32_mount;
    fat32.open = fat32_open;
    fat32.close = fat32_close;
    fat32.dup = fat32_dup;
    fat32.read = fat32_read;
    fat32.readdir = fat32_readdir;
    fat32.fstat = fat32_fstat;
    fat32.write = fat32_write;
    /*fat32.lseek = fat32_seek;*/
    fat32.sync = fat32_sync;

    vfs_install_fs(&fat32);
}
