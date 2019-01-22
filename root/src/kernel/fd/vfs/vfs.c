#include <stdint.h>
#include <stddef.h>
#include <fd/vfs/vfs.h>
#include <lib/klib.h>
#include <lib/errno.h>

struct vfs_handle_t {
    int fs;
    int intern_fd;
};

struct mnt_t {
    char mntpt[2048];
    int fs;
    int magic;
};

dynarray_new(struct fs_t, filesystems);
dynarray_new(struct mnt_t, mountpoints);
dynarray_new(struct vfs_handle_t, vfs_handles);

/* Return index into mountpoints array corresponding to the mountpoint
   inside which this file/path is located.
   char **local_path will return a pointer (in *local_path) to the
   part of the path inside the mountpoint. */
static struct mnt_t *vfs_get_mountpoint(const char *path, char **local_path) {
    spinlock_acquire(&mountpoints_lock);

    size_t guess = -1;
    size_t guess_size = 0;

    for (size_t i = 0; i < mountpoints_i; i++) {
        if (!mountpoints[i])
            continue;

        size_t len = kstrlen(mountpoints[i]->mntpt);

        if (!kstrncmp(path, mountpoints[i]->mntpt, len)) {
            if ( (((path[len] == '/') || (path[len] == '\0'))
                 || (!kstrcmp(mountpoints[i]->mntpt, "/")))
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

    spinlock_release(&mountpoints_lock);
    if (guess != -1)
        return mountpoints[guess];
    else
        return NULL;
}

/* Convert a relative path into an absolute path.
   This is a freestanding function and can be used for any purpose :) */
void vfs_get_absolute_path(char *path_ptr, const char *path, const char *pwd) {
    char *orig_ptr = path_ptr;

    if (!*path) {
        kstrcpy(path_ptr, pwd);
        return;
    }

    if (*path != '/') {
        kstrcpy(path_ptr, pwd);
        path_ptr += kstrlen(path_ptr);
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
                if ((!kstrncmp(path, ".\0", 2))
                ||  (!kstrncmp(path, "./\0", 3))) {
                    goto term;
                }
                if ((!kstrncmp(path, "..\0", 3))
                ||  (!kstrncmp(path, "../\0", 4))) {
                    while (*path_ptr != '/') path_ptr--;
                    if (path_ptr == orig_ptr) path_ptr++;
                    goto term;
                }
                if (!kstrncmp(path, "../", 3)) {
                    while (*path_ptr != '/') path_ptr--;
                    if (path_ptr == orig_ptr) path_ptr++;
                    path += 2;
                    *path_ptr = 0;
                    continue;
                }
                if (!kstrncmp(path, "./", 2)) {
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
    spinlock_acquire(&filesystems_lock);
    for (size_t i = 0; i < filesystems_i; i++) {
        if (!filesystems[i])
            continue;
        filesystems[i]->sync();
    }
    spinlock_release(&filesystems_lock);
    return 0;
}

void vfs_sync_worker(void *arg) {
    (void)arg;

    for (;;) {
        yield(2000);
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

    dynarray_add(struct fs_t, filesystems, filesystem);

    return 0;
}

static int vfs_dup(int fd) {
    int fs = vfs_handles[fd]->fs;
    int intern_fd = vfs_handles[fd]->intern_fd;

    if (filesystems[fs]->dup(intern_fd))
        return -1;

    return dynarray_add(struct vfs_handle_t, vfs_handles, vfs_handles[fd]);
}

static int vfs_readdir(int fd, struct dirent *buf) {
    int fs = vfs_handles[fd]->fs;
    int intern_fd = vfs_handles[fd]->intern_fd;

    return filesystems[fs]->readdir(intern_fd, buf);
}

static int vfs_read(int fd, void *buf, size_t len) {
    int fs = vfs_handles[fd]->fs;
    int intern_fd = vfs_handles[fd]->intern_fd;

    return filesystems[fs]->read(intern_fd, buf, len);
}

static int vfs_write(int fd, const void *buf, size_t len) {
    int fs = vfs_handles[fd]->fs;
    int intern_fd = vfs_handles[fd]->intern_fd;

    return filesystems[fs]->write(intern_fd, buf, len);
}

static int vfs_close(int fd) {
    int fs = vfs_handles[fd]->fs;
    int intern_fd = vfs_handles[fd]->intern_fd;

    return filesystems[fs]->close(intern_fd);
}

static int vfs_lseek(int fd, off_t offset, int type) {
    int fs = vfs_handles[fd]->fs;
    int intern_fd = vfs_handles[fd]->intern_fd;

    return filesystems[fs]->lseek(intern_fd, offset, type);
}

static int vfs_fstat(int fd, struct stat *st) {
    int fs = vfs_handles[fd]->fs;
    int intern_fd = vfs_handles[fd]->intern_fd;

    return filesystems[fs]->fstat(intern_fd, st);
}

static struct fd_handler_t vfs_functions = {
    vfs_close,
    vfs_fstat,
    vfs_read,
    vfs_write,
    vfs_lseek,
    vfs_dup,
    vfs_readdir
};

int open(const char *path, int mode) {
    struct vfs_handle_t vfs_handle = {0};

    char *loc_path;

    struct mnt_t *mountpoint = vfs_get_mountpoint(path, &loc_path);
    if (!mountpoint)
        return -1;

    int magic = mountpoint->magic;
    int fs = mountpoint->fs;

    int intern_fd = filesystems[fs]->open(loc_path, mode, magic);
    if (intern_fd == -1)
        return -1;

    vfs_handle.fs = fs;
    vfs_handle.intern_fd = intern_fd;

    int vfs_fd = dynarray_add(struct vfs_handle_t, vfs_handles, &vfs_handle);

    struct file_descriptor_t fd = {0};

    fd.intern_fd = vfs_fd;
    fd.fd_handler = vfs_functions;

    return fd_create(&fd);
}

int mount(const char *source, const char *target,
          const char *fs_type, unsigned long m_flags,
          const void *data) {
    size_t i;

    /* Search for fs with the correct type, since we know nothing
     * about the fs from the path given */
    /* TODO use a hash table */
    spinlock_acquire(&filesystems_lock);
    for (i = 0; i < filesystems_i; i++) {
        if (!filesystems[i])
            continue;
        if (!kstrcmp(filesystems[i]->type, fs_type))
            goto fnd;
    }
    spinlock_release(&filesystems_lock);
    return -1;

fnd:;
    struct fs_t *fs = filesystems[i];
    spinlock_release(&filesystems_lock);

    int res = fs->mount(source, m_flags, data);
    if (res == -1)
        return -1;

    struct mnt_t mount;

    kstrcpy(mount.mntpt, target);
    mount.fs = i;
    mount.magic = res;

    if (dynarray_add(struct mnt_t, mountpoints, &mount) == -1)
        return -1;

    kprint(KPRN_INFO, "vfs: Mounted `%s` on `%s`, type `%s`.", source, target, fs_type);

    return 0;
}
