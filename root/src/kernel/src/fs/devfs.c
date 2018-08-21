#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <fs.h>
#include <dev.h>

typedef struct {
    int free;
    char path[1024];
    int flags;
    int mode;
    long ptr;
    long begin;
    long end;
    int isblock;
    int device;
} devfs_handle_t;

static devfs_handle_t* devfs_handles = (devfs_handle_t*)0;
static int devfs_handles_ptr = 0;

static int devfs_create_handle(devfs_handle_t handle) {
    int handle_n;

    // check for a free handle first
    for (int i = 0; i < devfs_handles_ptr; i++) {
        if (devfs_handles[i].free) {
            handle_n = i;
            goto load_handle;
        }
    }

    devfs_handles = krealloc(devfs_handles, (devfs_handles_ptr + 1) * sizeof(devfs_handle_t));
    handle_n = devfs_handles_ptr++;
    
load_handle:
    devfs_handles[handle_n] = handle;
    
    return handle_n;

}

static int devfs_write(int handle, const void *ptr, size_t len) {
    int ret = device_write(devfs_handles[handle].device, ptr, devfs_handles[handle].ptr, len);
    if (ret == -1)
        return -1;
    devfs_handles[handle].ptr += ret;
    return ret;
}

static int devfs_read(int handle, void *ptr, size_t len) {
    int ret = device_read(devfs_handles[handle].device, ptr, devfs_handles[handle].ptr, len);
    if (ret == -1)
        return -1;
    devfs_handles[handle].ptr += ret;
    return ret;
}

static int devfs_mount(void) {
    return 0;
}

static int devfs_open(char *path, int flags, int mode) {
    dev_t device = device_find(path);

    if (device == (dev_t)(-1))
        return -1;

    if (flags & O_TRUNC)
        return -1;
    if (flags & O_APPEND)
        return -1;
    if (flags & O_CREAT)
        return -1;

    devfs_handle_t new_handle = {0};
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

    return devfs_create_handle(new_handle);
}

static int devfs_close(int handle) {
    if (handle < 0)
        return -1;

    if (handle >= devfs_handles_ptr)
        return -1;

    if (devfs_handles[handle].free)
        return -1;

    devfs_handles[handle].free = 1;

    return 0;
}

static int devfs_lseek(int handle, off_t offset, int type) {
    if (handle < 0)
        return -1;

    if (handle >= devfs_handles_ptr)
        return -1;

    if (devfs_handles[handle].free)
        return -1;

    if (devfs_handles[handle].isblock)
        return -1;

    switch (type) {
        case SEEK_SET:
            if ((devfs_handles[handle].begin + offset) > devfs_handles[handle].end ||
                (devfs_handles[handle].begin + offset) < devfs_handles[handle].begin) return -1;
            devfs_handles[handle].ptr = devfs_handles[handle].begin + offset;
            return devfs_handles[handle].ptr;
        case SEEK_END:
            if ((devfs_handles[handle].end + offset) > devfs_handles[handle].end ||
                (devfs_handles[handle].end + offset) < devfs_handles[handle].begin) return -1;
            devfs_handles[handle].ptr = devfs_handles[handle].end + offset;
            return devfs_handles[handle].ptr;
        case SEEK_CUR:
            if ((devfs_handles[handle].ptr + offset) > devfs_handles[handle].end ||
                (devfs_handles[handle].ptr + offset) < devfs_handles[handle].begin) return -1;
            devfs_handles[handle].ptr += offset;
            return devfs_handles[handle].ptr;
        default:
            return -1;
    }
}

void init_devfs(void) {
    fs_t devfs = {0};

    kstrcpy(devfs.type, "devfs");
    devfs.read = devfs_read;
    devfs.write = devfs_write;
    devfs.mount = (void *)devfs_mount;
    devfs.open = (void *)devfs_open;
    devfs.close = devfs_close;
    devfs.lseek = devfs_lseek;

    vfs_install_fs(devfs);
}
