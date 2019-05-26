#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <lib/time.h>
#include <fd/vfs/vfs.h>
#include <fs/devfs/devfs.h>
#include <lib/lock.h>
#include <lib/errno.h>
#include <sys/panic.h>

dynarray_new(struct device_t, devices);

struct devfs_handle_t {
    struct device_t *device;
    int root;
    int dev_fd;
    long ptr;
    long size;
    int refcount;
    lock_t lock;
};

dynarray_new(struct devfs_handle_t, devfs_handles);

dev_t device_add(struct device_t *device) {
    return dynarray_add(struct device_t, devices, device);
}

static int devfs_open(const char *path, int flags, int unused) {
    (void)unused;
    struct devfs_handle_t new_handle = {0};

    new_handle.refcount = 1;
    new_handle.lock = new_lock;

    if (flags & O_APPEND) {
        errno = EROFS;
        return -1;
    }

    if (!strcmp(path, "/")) {
        new_handle.root = 1;
        return dynarray_add(struct devfs_handle_t, devfs_handles, &new_handle);
    }

    if (*path == '/')
        path++;

    struct device_t *device = dynarray_search(struct device_t, devices, !strcmp(elem->name, path));
    if (!device) {
        if (flags & O_CREAT)
            errno = EROFS;
        else
            errno = ENOENT;
        return -1;
    }

    new_handle.dev_fd = device->intern_fd;
    new_handle.size = device->size;
    new_handle.device = device;

    return dynarray_add(struct devfs_handle_t, devfs_handles, &new_handle);
}

void device_sync_worker(void *arg) {
    (void)arg;

    for (;;) {
        for (size_t i = 0; i < locked_read(size_t, &devices_i); i++) {
            struct device_t *device = dynarray_getelem(struct device_t, devices, i);
            if (!device)
                continue;
            if (device->calls.flush) {
                switch (device->calls.flush(device->intern_fd)) {
                    case 1:
                        /* flush is a no-op */
                        break;
                    default:
                        relaxed_sleep(100);
                        break;
                }
            }
            dynarray_unref(devices, i);
            relaxed_sleep(1000);
        }
    }
}

static int devfs_tcgetattr(int fd, struct termios *buf) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    int ret = devfs_handle->device->calls.tcgetattr(
                devfs_handle->dev_fd,
                buf);

    dynarray_unref(devfs_handles, fd);
    return ret;
}

static int devfs_tcsetattr(int fd, int optional_actions, struct termios *buf) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    int ret = devfs_handle->device->calls.tcsetattr(
                devfs_handle->dev_fd,
                optional_actions,
                buf);

    dynarray_unref(devfs_handles, fd);
    return ret;
}

static int devfs_tcflow(int fd, int action) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    int ret = devfs_handle->device->calls.tcflow(
                devfs_handle->dev_fd,
                action);

    dynarray_unref(devfs_handles, fd);
    return ret;
}

static int devfs_isatty(int fd) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    int ret = devfs_handle->device->calls.isatty(devfs_handle->dev_fd);

    dynarray_unref(devfs_handles, fd);
    return ret;
}

static int devfs_read(int fd, void *ptr, size_t len) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    if (!devfs_handle) {
        errno = EBADF;
        return -1;
    }

    if (devfs_handle->root) {
        dynarray_unref(devfs_handles, fd);
        errno = EISDIR;
        return -1;
    }

    spinlock_acquire(&devfs_handle->lock);

    if (devfs_handle->size)
        if (devfs_handle->ptr + len >= devfs_handle->size)
            len -= devfs_handle->size - devfs_handle->ptr;

    int ret = devfs_handle->device->calls.read(
                devfs_handle->dev_fd,
                ptr,
                devfs_handle->ptr,
                len);

    if (ret != -1 && devfs_handle->size)
        devfs_handle->ptr += ret;

    spinlock_release(&devfs_handle->lock);
    dynarray_unref(devfs_handles, fd);

    return ret;
}

static int devfs_write(int fd, const void *ptr, size_t len) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    if (!devfs_handle) {
        errno = EBADF;
        return -1;
    }

    if (devfs_handle->root) {
        dynarray_unref(devfs_handles, fd);
        errno = EISDIR;
        return -1;
    }

    spinlock_acquire(&devfs_handle->lock);

    if (devfs_handle->size) {
        if (devfs_handle->ptr == devfs_handle->size) {
            spinlock_release(&devfs_handle->lock);
            errno = ENOSPC;
            return -1;
        }
        if (devfs_handle->ptr + len >= devfs_handle->size)
            len -= devfs_handle->size - devfs_handle->ptr;
    }

    int ret = devfs_handle->device->calls.write(
                devfs_handle->dev_fd,
                ptr,
                devfs_handle->ptr,
                len);

    if (ret != -1 && devfs_handle->size)
        devfs_handle->ptr += ret;

    spinlock_release(&devfs_handle->lock);
    dynarray_unref(devfs_handles, fd);

    return ret;
}

static int devfs_fstat(int fd, struct stat *st) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    if (!devfs_handle) {
        errno = EBADF;
        return -1;
    }

    // devfs root fstat
    st->st_dev = 1;
    if (devfs_handle->root)
        st->st_ino = 1;
    else
        st->st_ino = (ino_t)((size_t)devfs_handle->device & 0xfffffff);
    st->st_nlink = 1;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = devfs_handle->size;
    st->st_blksize = 512;
    st->st_blocks = (devfs_handle->size + 512 - 1) / 512;
    st->st_atim.tv_sec = unix_epoch;
    st->st_atim.tv_nsec = 0;
    st->st_mtim.tv_sec = unix_epoch;
    st->st_mtim.tv_nsec = 0;
    st->st_ctim.tv_sec = unix_epoch;
    st->st_ctim.tv_nsec = 0;
    st->st_mode = 0;
    if (devfs_handle->root)
        st->st_mode |= S_IFDIR;
    else
        if (!devfs_handle->size)
            st->st_mode |= S_IFCHR;
        else
            st->st_mode |= S_IFBLK;

    dynarray_unref(devfs_handles, fd);

    return 0;
}

static int devfs_mount(const char *a, unsigned long b, const void *c) {
    (void)a;
    (void)b;
    (void)c;
    return 0;
}

static int devfs_umount(const char *a) {
    (void)a;
    return 0;
}

static int devfs_close(int fd) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    if (!devfs_handle) {
        errno = EBADF;
        return -1;
    }

    spinlock_acquire(&devfs_handle->lock);

    if (!(--devfs_handle->refcount)) {
        dynarray_remove(devfs_handles, fd);
        return 0;
    }

    spinlock_release(&devfs_handle->lock);
    dynarray_unref(devfs_handles, fd);

    return 0;
}

static int devfs_dup(int fd) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    if (!devfs_handle) {
        errno = EBADF;
        return -1;
    }

    spinlock_acquire(&devfs_handle->lock);
    devfs_handle->refcount++;
    spinlock_release(&devfs_handle->lock);

    dynarray_unref(devfs_handles, fd);

    return 0;
}

static int devfs_lseek(int fd, off_t offset, int type) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    if (!devfs_handle) {
        errno = EBADF;
        return -1;
    }

    if (devfs_handle->root) {
        dynarray_unref(devfs_handles, fd);
        errno = EISDIR;
        return -1;
    }

    if (!devfs_handle->size) {
        dynarray_unref(devfs_handles, fd);
        errno = ESPIPE;
        return -1;
    }

    spinlock_acquire(&devfs_handle->lock);

    switch (type) {
        case SEEK_SET:
            if (offset >= devfs_handle->size ||
                offset < 0) goto def;
            devfs_handle->ptr = offset;
            break;
        case SEEK_END:
            if (devfs_handle->size + offset >= devfs_handle->size ||
                devfs_handle->size + offset < 0) goto def;
            devfs_handle->ptr = devfs_handle->size + offset;
            break;
        case SEEK_CUR:
            if (devfs_handle->ptr + offset >= devfs_handle->size ||
                devfs_handle->ptr + offset < 0) goto def;
            devfs_handle->ptr += offset;
            break;
        default:
        def:
            spinlock_release(&devfs_handle->lock);
            dynarray_unref(devfs_handles, fd);
            errno = EINVAL;
            return -1;
    }

    int ret = (int)devfs_handle->ptr;
    spinlock_release(&devfs_handle->lock);
    dynarray_unref(devfs_handles, fd);
    return ret;
}

static int devfs_readdir(int fd, struct dirent *dir) {
    struct devfs_handle_t *devfs_handle =
        dynarray_getelem(struct devfs_handle_t, devfs_handles, fd);

    if (!devfs_handle) {
        errno = EBADF;
        return -1;
    }

    if (!devfs_handle->root) {
        dynarray_unref(devfs_handles, fd);
        errno = ENOTDIR;
        return -1;
    }

    spinlock_acquire(&devfs_handle->lock);

    for (;;) {
        // check if past directory table
        if (devfs_handle->ptr >= locked_read(size_t, &devices_i)) {
            errno = 0;
            spinlock_release(&devfs_handle->lock);
            dynarray_unref(devfs_handles, fd);
            return -1;
        }
        struct device_t *dev = dynarray_getelem(struct device_t, devices, devfs_handle->ptr);
        devfs_handle->ptr++;
        if (dev) {
            // valid entry
            dir->d_ino = (ino_t)((size_t)dev & 0xfffffff);
            strcpy(dir->d_name, dev->name);
            dir->d_reclen = sizeof(struct dirent);
            if (!dev->size) {
                dir->d_type = DT_CHR;
            } else {
                dir->d_type = DT_BLK;
            }
            break;
        }
    }

    spinlock_release(&devfs_handle->lock);
    dynarray_unref(devfs_handles, fd);
    return 0;
}

static int devfs_sync(void) {
    return 0;
}

void init_fs_devfs(void) {
    struct fs_t devfs = {0};

    devfs = default_fs_handler;
    strcpy(devfs.name, "devfs");
    devfs.read = devfs_read;
    devfs.write = devfs_write;
    devfs.mount = devfs_mount;
    devfs.umount = devfs_umount;
    devfs.open = devfs_open;
    devfs.close = devfs_close;
    devfs.lseek = devfs_lseek;
    devfs.fstat = devfs_fstat;
    devfs.dup = devfs_dup;
    devfs.readdir = devfs_readdir;
    devfs.sync = devfs_sync;
    devfs.tcgetattr = devfs_tcgetattr;
    devfs.tcsetattr = devfs_tcsetattr;
    devfs.tcflow = devfs_tcflow;
    devfs.isatty = devfs_isatty;

    vfs_install_fs(&devfs);
}
