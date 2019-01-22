#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <fd/vfs/vfs.h>
#include <devices/dev.h>
#include <lib/lock.h>
#include <lib/errno.h>

#define DEVFS_HANDLES_STEP 1024

struct devfs_handle_t {
    int free;
    int refcount;
    char path[1024];
    int flags;
    int mode;
    long ptr;
    long begin;
    long end;
    int is_stream;
    int device;
    lock_t lock;
};

static lock_t devfs_lock = 1;

static struct devfs_handle_t *devfs_handles;
static int devfs_handles_i;

static int devfs_create_handle(struct devfs_handle_t handle) {
    int handle_n;

    for (int i = 0; i < devfs_handles_i; i++) {
        if (devfs_handles[i].free) {
            handle_n = i;
            goto load_handle;
        }
    }

    devfs_handles = krealloc(devfs_handles,
        (devfs_handles_i + DEVFS_HANDLES_STEP) * sizeof(struct devfs_handle_t));
    handle_n = devfs_handles_i;

    for ( ;
         devfs_handles_i < devfs_handles_i + DEVFS_HANDLES_STEP;
         devfs_handles_i++) {
        devfs_handles[devfs_handles_i].free = 1;
    }

load_handle:
    devfs_handles[handle_n] = handle;

    return handle_n;
}

static int devfs_write(int handle, const void *ptr, size_t len) {
    spinlock_acquire(&devfs_lock);
    if (   handle < 0
        || handle >= devfs_handles_i
        || devfs_handles[handle].free) {
        spinlock_release(&devfs_lock);
        errno = EBADF;
        return -1;
    }
    if (devfs_handles[handle].device == -1) {
        spinlock_release(&devfs_lock);
        errno = EISDIR;
        return -1;
    }
    spinlock_acquire(&devfs_handles[handle].lock);
    struct devfs_handle_t dev = devfs_handles[handle];
    spinlock_release(&devfs_lock);
    int ret = device_write(dev.device, ptr, dev.ptr, len);
    if (ret == -1) {
        spinlock_acquire(&devfs_lock);
        spinlock_release(&devfs_handles[handle].lock);
        spinlock_release(&devfs_lock);
        return -1;
    }
    dev.ptr += ret;
    spinlock_acquire(&devfs_lock);
    devfs_handles[handle] = dev;
    spinlock_release(&devfs_handles[handle].lock);
    spinlock_release(&devfs_lock);
    return ret;
}

static int devfs_read(int handle, void *ptr, size_t len) {
    spinlock_acquire(&devfs_lock);
    if (   handle < 0
        || handle >= devfs_handles_i
        || devfs_handles[handle].free) {
        spinlock_release(&devfs_lock);
        errno = EBADF;
        return -1;
    }
    if (devfs_handles[handle].device == -1) {
        spinlock_release(&devfs_lock);
        errno = EISDIR;
        return -1;
    }
    spinlock_acquire(&devfs_handles[handle].lock);
    struct devfs_handle_t dev = devfs_handles[handle];
    spinlock_release(&devfs_lock);
    int ret = device_read(dev.device, ptr, dev.ptr, len);
    if (ret == -1) {
        spinlock_acquire(&devfs_lock);
        spinlock_release(&devfs_handles[handle].lock);
        spinlock_release(&devfs_lock);
        return -1;
    }
    dev.ptr += ret;
    spinlock_acquire(&devfs_lock);
    devfs_handles[handle] = dev;
    spinlock_release(&devfs_handles[handle].lock);
    spinlock_release(&devfs_lock);
    return ret;
}

static int devfs_mount(void) {
    return 0;
}

static int devfs_open(char *path, int flags, int mode) {
    spinlock_acquire(&devfs_lock);

    int is_root = 0;

    if (flags & O_APPEND) {
        errno = EROFS;
        goto fail;
    }

    if (!kstrcmp(path, "/")) {
        is_root = 1;
        goto root;
    }

    if (*path == '/')
        path++;

    dev_t device = device_find(path);
    if (device == (dev_t)(-1)) {
        if (flags & O_CREAT)
            errno = EROFS;
        else
            errno = ENOENT;
        goto fail;
    }

root:;
    struct devfs_handle_t new_handle = {0};
    new_handle.free = 0;
    new_handle.refcount = 1;
    kstrcpy(new_handle.path, path);
    new_handle.flags = flags;
    new_handle.mode = mode;
    if (is_root)
        new_handle.end = 0;
    else
        new_handle.end = (long)device_size(device);
    if (!new_handle.end)
        new_handle.is_stream = 1;
    new_handle.ptr = 0;
    new_handle.begin = 0;
    if (is_root)
        new_handle.device = -1;
    else
        new_handle.device = device;

    new_handle.lock = 1;

    int ret = devfs_create_handle(new_handle);
    spinlock_release(&devfs_lock);
    return ret;

fail:
    spinlock_release(&devfs_lock);
    return -1;
}

static int devfs_close(int handle) {
    spinlock_acquire(&devfs_lock);

    if (   handle < 0
        || handle >= devfs_handles_i
        || devfs_handles[handle].free) {
        spinlock_release(&devfs_lock);
        errno = EBADF;
        return -1;
    }

    if (!(--devfs_handles[handle].refcount)) {
        devfs_handles[handle].free = 1;
    }

    spinlock_release(&devfs_lock);
    return 0;
}

static int devfs_lseek(int handle, off_t offset, int type) {
    spinlock_acquire(&devfs_lock);

    if (   handle < 0
        || handle >= devfs_handles_i
        || devfs_handles[handle].free) {
        errno = EBADF;
        spinlock_release(&devfs_lock);
        return -1;
    }

    if (devfs_handles[handle].is_stream) {
        errno = ESPIPE;
        spinlock_release(&devfs_lock);
        return -1;
    }

    switch (type) {
        case SEEK_SET:
            if ((devfs_handles[handle].begin + offset) > devfs_handles[handle].end ||
                (devfs_handles[handle].begin + offset) < devfs_handles[handle].begin) goto def;
            devfs_handles[handle].ptr = devfs_handles[handle].begin + offset;
            break;
        case SEEK_END:
            if ((devfs_handles[handle].end + offset) > devfs_handles[handle].end ||
                (devfs_handles[handle].end + offset) < devfs_handles[handle].begin) goto def;
            devfs_handles[handle].ptr = devfs_handles[handle].end + offset;
            break;
        case SEEK_CUR:
            if ((devfs_handles[handle].ptr + offset) > devfs_handles[handle].end ||
                (devfs_handles[handle].ptr + offset) < devfs_handles[handle].begin) goto def;
            devfs_handles[handle].ptr += offset;
            break;
        default:
        def:
            spinlock_release(&devfs_lock);
            errno = EINVAL;
            return -1;
    }

    long ret = devfs_handles[handle].ptr;
    spinlock_release(&devfs_lock);
    return ret;
}

static int devfs_dup(int handle) {
    spinlock_acquire(&devfs_lock);

    if (   handle < 0
        || handle >= devfs_handles_i
        || devfs_handles[handle].free) {
        spinlock_release(&devfs_lock);
        errno = EBADF;
        return -1;
    }

    devfs_handles[handle].refcount++;

    spinlock_release(&devfs_lock);

    return 0;
}

int devfs_fstat(int handle, struct stat *st) {
    spinlock_acquire(&devfs_lock);

    if (   handle < 0
        || handle >= devfs_handles_i
        || devfs_handles[handle].free) {
        spinlock_release(&devfs_lock);
        errno = EBADF;
        return -1;
    }

    st->st_dev = 0;
    st->st_ino = devfs_handles[handle].device + 1;
    st->st_nlink = 1;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    if (!devfs_handles[handle].is_stream)
        st->st_size = devfs_handles[handle].end;
    else
        st->st_size = 0;
    st->st_blksize = 512;
    st->st_blocks = (st->st_size + 512 - 1) / 512;
    st->st_atim.tv_sec = 0;
    st->st_atim.tv_nsec = 0;
    st->st_mtim.tv_sec = 0;
    st->st_mtim.tv_nsec = 0;
    st->st_ctim.tv_sec = 0;
    st->st_ctim.tv_nsec = 0;

    st->st_mode = 0;
    if (devfs_handles[handle].device == -1) {
        st->st_mode |= S_IFDIR;
    } else {
        if (devfs_handles[handle].is_stream)
            st->st_mode |= S_IFCHR;
        else
            st->st_mode |= S_IFBLK;
    }

    spinlock_release(&devfs_lock);
    return 0;
}

static int devfs_readdir(int handle, struct dirent *dir) {
    spinlock_acquire(&devfs_lock);

    if (   handle < 0
        || handle >= devfs_handles_i
        || devfs_handles[handle].free) {
        spinlock_release(&devfs_lock);
        errno = EBADF;
        return -1;
    }

    for (;;) {
        // check if past directory table
        if (devfs_handles[handle].ptr >= MAX_DEVICES) goto end_of_dir;
        struct device *dev = &devices[devfs_handles[handle].ptr];
        devfs_handles[handle].ptr++;
        if (dev->used) {
            // valid entry
            dir->d_ino = devfs_handles[handle].ptr;
            kstrcpy(dir->d_name, dev->name);
            dir->d_reclen = sizeof(struct dirent);
            if (dev->size) {
                dir->d_type = DT_CHR;
            } else {
                dir->d_type = DT_BLK;
            }
            break;
        }
    }

    spinlock_release(&devfs_lock);
    return 0;

end_of_dir:
    spinlock_release(&devfs_lock);
    errno = 0;
    return -1;
}

int devfs_sync(void) {
    return 0;
}

int init_fs_devfs(void) {
    struct fs_t devfs = {0};
    devfs_handles = kalloc(DEVFS_HANDLES_STEP * sizeof(struct devfs_handle_t));
    devfs_handles_i = DEVFS_HANDLES_STEP;

    for (size_t i = 0; i < DEVFS_HANDLES_STEP; i++) {
        devfs_handles[i].free = 1;
    }

    kstrcpy(devfs.type, "devfs");
    devfs.read = devfs_read;
    devfs.write = devfs_write;
    devfs.mount = (void *)devfs_mount;
    devfs.open = (void *)devfs_open;
    devfs.close = devfs_close;
    devfs.lseek = devfs_lseek;
    devfs.fstat = devfs_fstat;
    devfs.dup = devfs_dup;
    devfs.readdir = devfs_readdir;
    devfs.sync = devfs_sync;

    vfs_install_fs(devfs);
}
