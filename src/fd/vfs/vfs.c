#include <stdint.h>
#include <stddef.h>
#include <fd/vfs/vfs.h>
#include <lib/klib.h>
#include <lib/errno.h>
#include <lib/dynarray.h>
#include <lib/ht.h>
#include <lib/cstring.h>
#include <lib/cmem.h>

struct vfs_handle_t {
    struct fs_t *fs;
    int intern_fd;
};

struct mnt_t {
    char name[2048];
    struct fs_t *fs;
    int magic;
};

ht_new(struct fs_t, filesystems);
ht_new(struct mnt_t, mountpoints);
dynarray_new(struct vfs_handle_t, vfs_handles);

/* Return index into mountpoints array corresponding to the mountpoint
   inside which this file/path is located.
   char **local_path will return a pointer (in *local_path) to the
   part of the path inside the mountpoint. */
static struct mnt_t *vfs_get_mountpoint(const char *path, char **local_path) {
    size_t size;

    spinlock_acquire(&mountpoints_lock);

    struct mnt_t **mnts = ht_dump(struct mnt_t, mountpoints, &size);
    if (!mnts)
        return NULL;

    ssize_t guess = -1;
    size_t guess_size = 0;

    for (size_t i = 0; i < size; i++) {
        if (!mnts[i])
            continue;

        size_t len = strlen(mnts[i]->name);

        if (!strncmp(path, mnts[i]->name, len)) {
            if ( (((path[len] == '/') || (path[len] == '\0'))
                 || (!strcmp(mnts[i]->name, "/")))
               && (len > guess_size)) {
                guess = i;
                guess_size = len;
            }
        }
    }

    *local_path = (char *)path;

    if (guess_size > 1)
        *local_path += guess_size;

    if (!**local_path)
        *local_path = "/";

    struct mnt_t *ret = mnts[guess];

    kfree(mnts);

    spinlock_release(&mountpoints_lock);

    if (guess != -1)
        return ret;
    else
        return NULL;
}

/* Convert a relative path into an absolute path.
   This is a freestanding function and can be used for any purpose :) */
void vfs_get_absolute_path(char *path_ptr, const char *path, const char *pwd) {
    char *orig_ptr = path_ptr;

    if (!*path) {
        strcpy(path_ptr, pwd);
        return;
    }

    if (*path != '/') {
        strcpy(path_ptr, pwd);
        path_ptr += strlen(path_ptr);
    } else {
        *path_ptr = '/';
        path_ptr++;
        path++;
    }

    goto first_run;

    for (;;) {
        switch (*path) {
            case '/':
                path++;
first_run:
                if (*path == '/') continue;
                if ((!strncmp(path, ".\0", 2))
                ||  (!strncmp(path, "./\0", 3))) {
                    goto term;
                }
                if ((!strncmp(path, "..\0", 3))
                ||  (!strncmp(path, "../\0", 4))) {
                    while (*path_ptr != '/') path_ptr--;
                    if (path_ptr == orig_ptr) path_ptr++;
                    goto term;
                }
                if (!strncmp(path, "../", 3)) {
                    while (*path_ptr != '/') path_ptr--;
                    if (path_ptr == orig_ptr) path_ptr++;
                    path += 2;
                    *path_ptr = 0;
                    continue;
                }
                if (!strncmp(path, "./", 2)) {
                    path += 1;
                    continue;
                }
                if (((path_ptr - 1) != orig_ptr) && (*(path_ptr - 1) != '/')) {
                    *path_ptr = '/';
                    path_ptr++;
                }
                continue;
            case '\0':
term:
                if ((*(path_ptr - 1) == '/') && ((path_ptr - 1) != orig_ptr))
                    path_ptr--;
                *path_ptr = 0;
                return;
            default:
                *path_ptr = *path;
                path++;
                path_ptr++;
                continue;
        }
    }
}

int vfs_sync(void) {
    size_t size;

    spinlock_acquire(&filesystems_lock);

    struct fs_t **fs = ht_dump(struct fs_t, filesystems, &size);
    if (!fs)
        return 0;

    for (size_t i = 0; i < size; i++)
        fs[i]->sync();

    kfree(fs);

    spinlock_release(&filesystems_lock);

    return 0;
}

void vfs_sync_worker(void *arg) {
    (void)arg;

    for (;;) {
        relaxed_sleep(2000);
        vfs_sync();
    }
}

static int vfs_call_invalid(void) {
    kprint(KPRN_WARN, "vfs: Unimplemented filesystem call occurred, returning ENOSYS!");
    errno = ENOSYS;
    return -1;
}

int vfs_install_fs(struct fs_t *filesystem) {
    /* check if any entry is bogus */
    /* XXX make this prettier */
    size_t *p = ((void *)filesystem) + 256; // 256 == size of type
    for (size_t i = 0; i < (sizeof(struct fs_t) - 256) / sizeof(size_t); i++)
        if (!p[i])
            p[i] = (size_t)vfs_call_invalid;

    struct fs_t *fs_glob = kalloc(sizeof(struct fs_t));
    *fs_glob = *filesystem;
    return ht_add(struct fs_t, filesystems, fs_glob);
}

static int vfs_dup(int fd) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->dup(intern_fd);

    if (ret == -1) {
        dynarray_unref(vfs_handles, fd);
        return -1;
    }

    ret = dynarray_add(struct vfs_handle_t, vfs_handles, fd_ptr);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_readdir(int fd, struct dirent *buf) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->readdir(intern_fd, buf);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_read(int fd, void *buf, size_t len) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->read(intern_fd, buf, len);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_write(int fd, const void *buf, size_t len) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->write(intern_fd, buf, len);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_close(int fd) {
    struct vfs_handle_t fd_copy = *dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    dynarray_unref(vfs_handles, fd);
    dynarray_remove(vfs_handles, fd);
    int intern_fd = fd_copy.intern_fd;
    if (fd_copy.fs->close(intern_fd))
        return -1;
    return 0;
}

static int vfs_lseek(int fd, off_t offset, int type) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->lseek(intern_fd, offset, type);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_fstat(int fd, struct stat *st) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->fstat(intern_fd, st);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_tcgetattr(int fd, struct termios *buf) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->tcgetattr(intern_fd, buf);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_tcsetattr(int fd, int optional_actions, struct termios *buf) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->tcsetattr(intern_fd, optional_actions, buf);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_tcflow(int fd, int action) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->tcflow(intern_fd, action);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_isatty(int fd) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->isatty(intern_fd);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_unlink(int fd) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->unlink(intern_fd);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

static int vfs_getpath(int fd, char *buf) {
    struct vfs_handle_t *fd_ptr = dynarray_getelem(struct vfs_handle_t, vfs_handles, fd);
    int intern_fd = fd_ptr->intern_fd;
    int ret = fd_ptr->fs->getpath(intern_fd, buf);
    dynarray_unref(vfs_handles, fd);
    return ret;
}

int mkdir(const char *path) {
    char *loc_path;

    struct mnt_t *mountpoint = vfs_get_mountpoint(path, &loc_path);
    if (!mountpoint)
        return -1;

    int magic = mountpoint->magic;
    struct fs_t *fs = mountpoint->fs;

    return fs->mkdir(loc_path, magic);
}

int open(const char *path, int mode) {
    struct vfs_handle_t vfs_handle = {0};

    char *loc_path;

    struct mnt_t *mountpoint = vfs_get_mountpoint(path, &loc_path);
    if (!mountpoint)
        return -1;

    int magic = mountpoint->magic;
    struct fs_t *fs = mountpoint->fs;

    int intern_fd = fs->open(loc_path, mode, magic);
    if (intern_fd == -1)
        return -1;

    vfs_handle.fs = fs;
    vfs_handle.intern_fd = intern_fd;

    int vfs_fd = dynarray_add(struct vfs_handle_t, vfs_handles, &vfs_handle);

    struct file_descriptor_t fd = {0};

    fd.intern_fd = vfs_fd;

    struct fd_handler_t vfs_functions = default_fd_handler;
    vfs_functions.close = vfs_close;
    vfs_functions.fstat = vfs_fstat;
    vfs_functions.read = vfs_read;
    vfs_functions.write = vfs_write;
    vfs_functions.lseek = vfs_lseek;
    vfs_functions.dup = vfs_dup;
    vfs_functions.readdir = vfs_readdir;
    vfs_functions.tcgetattr = vfs_tcgetattr;
    vfs_functions.tcsetattr = vfs_tcsetattr;
    vfs_functions.tcflow = vfs_tcflow;
    vfs_functions.isatty = vfs_isatty;
    vfs_functions.unlink = vfs_unlink;

    fd.fd_handler = vfs_functions;

    // TODO actually implement poll shit
    fd.status = POLLOUT | POLLIN;

    return fd_create(&fd);
}

void init_fd_vfs(void) {
    ht_init(filesystems);
    ht_init(mountpoints);
}

int mount(const char *source, const char *target,
          const char *fs_type, unsigned long m_flags,
          const void *data) {
    struct fs_t *fs = ht_get(struct fs_t, filesystems, fs_type);
    if (!fs)
        return -1;

    int res = fs->mount(source, m_flags, data);
    if (res == -1)
        return -1;

    struct mnt_t *mount = kalloc(sizeof(struct mnt_t));

    strcpy(mount->name, target);
    mount->fs = fs;
    mount->magic = res;

    if (ht_add(struct mnt_t, mountpoints, mount) == -1)
        return -1;

    kprint(KPRN_INFO, "vfs: Mounted `%s` on `%s`, type `%s`.", source, target, fs_type);

    return 0;
}

int umount(const char *target) {
    struct mnt_t *mount = ht_get(struct mnt_t, mountpoints, target);

    int ret = mount->fs->umount(mount->magic);

    if (ret)
        return ret;

    ht_remove(struct mnt_t, mountpoints, target);
    kfree(mount);

    return 0;
}
