#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <fs.h>
#include <dev.h>
#include <lock.h>

struct devfs_handle_t {
    int free;
    char path[1024];
    int flags;
    int mode;
    long ptr;
    long begin;
    long end;
    int isblock;
    int device;
    lock_t lock;
};

struct devfs_handle_t *devfs_handles;
static int devfs_handles_i = 0;

static int devfs_create_handle(struct devfs_handle_t handle) {
    int handle_n;

    for (int i = 0; i < devfs_handles_i; i++) {
        if (devfs_handles[i].free) {
            handle_n = i;
            goto load_handle;
        }
    }

    devfs_handles = krealloc(devfs_handles, (devfs_handles_i + 1) * sizeof(struct devfs_handle_t));
    handle_n = devfs_handles_i;
    devfs_handles_i++;

load_handle:
    devfs_handles[handle_n] = handle;

    return handle_n;
}

static int devfs_write(int handle, const void *ptr, size_t len) {
    spinlock_acquire(&devfs_handles[handle].lock);
    int ret = device_write(devfs_handles[handle].device, ptr, devfs_handles[handle].ptr, len);
    if (ret == -1) {
        spinlock_release(&devfs_handles[handle].lock);
        return -1;
    }
    devfs_handles[handle].ptr += ret;
    spinlock_release(&devfs_handles[handle].lock);
    return ret;
}

static int devfs_read(int handle, void *ptr, size_t len) {
    spinlock_acquire(&devfs_handles[handle].lock);
    int ret = device_read(devfs_handles[handle].device, ptr, devfs_handles[handle].ptr, len);
    if (ret == -1) {
        spinlock_release(&devfs_handles[handle].lock);
        return -1;
    }
    devfs_handles[handle].ptr += ret;
    spinlock_release(&devfs_handles[handle].lock);
    return ret;
}

static int devfs_mount(void) {
    return 0;
}

static lock_t devfs_open_close_lock = 1;

static int devfs_open(char *path, int flags, int mode) {
    spinlock_acquire(&devfs_open_close_lock);

    dev_t device = device_find(path);
    if (device == (dev_t)(-1))
        goto fail;

    if (flags & O_TRUNC)
        goto fail;
    if (flags & O_APPEND)
        goto fail;
    if (flags & O_CREAT)
        goto fail;

    struct devfs_handle_t new_handle = {0};
    new_handle.free = 0;
    kstrcpy(new_handle.path, path);
    new_handle.flags = flags;
    new_handle.mode = mode;
    new_handle.end = (long)device_size(device);
    if (!new_handle.end)
        new_handle.isblock = 1;
    new_handle.ptr = 0;
    new_handle.begin = 0;
    new_handle.device = device;

    new_handle.lock = 1;

    int ret = devfs_create_handle(new_handle);
    spinlock_release(&devfs_open_close_lock);
    return ret;

fail:
    spinlock_release(&devfs_open_close_lock);
    return -1;
}

static int devfs_close(int handle) {
    spinlock_acquire(&devfs_open_close_lock);

    if (handle < 0)
        goto fail;

    if (handle >= devfs_handles_i)
        goto fail;

    if (devfs_handles[handle].free)
        goto fail;

    devfs_handles[handle].free = 1;

    spinlock_release(&devfs_open_close_lock);
    return 0;

fail:
    spinlock_release(&devfs_open_close_lock);
    return -1;
}

static int devfs_lseek(int handle, off_t offset, int type) {
    spinlock_acquire(&devfs_handles[handle].lock);

    if (handle < 0)
        goto fail;

    if (handle >= devfs_handles_i)
        goto fail;

    if (devfs_handles[handle].free)
        goto fail;

    if (devfs_handles[handle].isblock)
        goto fail;

    switch (type) {
        case SEEK_SET:
            if ((devfs_handles[handle].begin + offset) > devfs_handles[handle].end ||
                (devfs_handles[handle].begin + offset) < devfs_handles[handle].begin) return -1;
            devfs_handles[handle].ptr = devfs_handles[handle].begin + offset;
            goto success;
        case SEEK_END:
            if ((devfs_handles[handle].end + offset) > devfs_handles[handle].end ||
                (devfs_handles[handle].end + offset) < devfs_handles[handle].begin) return -1;
            devfs_handles[handle].ptr = devfs_handles[handle].end + offset;
            goto success;
        case SEEK_CUR:
            if ((devfs_handles[handle].ptr + offset) > devfs_handles[handle].end ||
                (devfs_handles[handle].ptr + offset) < devfs_handles[handle].begin) return -1;
            devfs_handles[handle].ptr += offset;
            goto success;
        default:
            goto fail;
    }

success:
    spinlock_release(&devfs_handles[handle].lock);
    return devfs_handles[handle].ptr;

fail:
    spinlock_release(&devfs_handles[handle].lock);
    return -1;
}

void init_devfs(void) {
    struct fs_t devfs = {0};
    devfs_handles = kalloc(256 * sizeof(struct devfs_handle_t));
    devfs_handles_i = 256;

    for (size_t i = 0; i < 256; i++) {
        devfs_handles[i].free = 1;
    }

    kstrcpy(devfs.type, "devfs");
    devfs.read = devfs_read;
    devfs.write = devfs_write;
    devfs.mount = (void *)devfs_mount;
    devfs.open = (void *)devfs_open;
    devfs.close = devfs_close;
    devfs.lseek = devfs_lseek;

    vfs_install_fs(devfs);
}
