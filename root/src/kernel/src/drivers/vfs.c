#include <stdint.h>
#include <stddef.h>
#include <fs.h>
#include <task.h>
#include <klib.h>
#include <smp.h>
#include <errno.h>

struct fs_t *filesystems;
struct mnt_t *mountpoints;
struct vfs_handle_t *file_descriptors;

static size_t mountpoints_i = 0;
static size_t filesystems_i = 0;
static size_t fd_count = 0;

static int create_fd(struct vfs_handle_t *fd) {
    size_t i = 0;

retry:
    for ( ; i < fd_count; i++) {
        if (!file_descriptors[i].used) {
            file_descriptors[i] = *fd;
            return i;
        }
    }

    /* Make more space */
    fd_count += 256;
    file_descriptors = krealloc(file_descriptors, fd_count * sizeof(struct vfs_handle_t));

    goto retry;
}

int pipe(int *pipefd) {
    struct vfs_handle_t fd_read = {0};
    struct vfs_handle_t fd_write = {0};
    struct pipe_t *new_pipe = pipe_open();

    fd_read.used = 1;
    fd_read.type = FD_PIPE_READ;
    fd_read.pipe = new_pipe;

    fd_write.used = 1;
    fd_write.type = FD_PIPE_WRITE;
    fd_write.pipe = new_pipe;

    new_pipe->refcount = 2;

    pipefd[0] = create_fd(&fd_read);
    pipefd[1] = create_fd(&fd_write);

    return 0;
}

int perfmon_create(void) {
    struct vfs_handle_t fd;
    struct perfmon_t *perfmon = kalloc(sizeof(struct perfmon_t));
    perfmon->refcount = 1;

    fd.used = 1;
    fd.type = FD_PERFMON;
    fd.perfmon = perfmon;

    return create_fd(&fd);
}

int perfmon_attach(int fd) {
    if (file_descriptors[fd].type != FD_PERFMON) {
        errno = EINVAL;
        return -1;
    }

    struct perfmon_t *perfmon = file_descriptors[fd].perfmon;
    perfmon_ref(file_descriptors[fd].perfmon);

    spinlock_acquire(&scheduler_lock);
    struct process_t *process = process_table[CURRENT_PROCESS];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->perfmon_lock);
    if (process->active_perfmon) {
        spinlock_release(&process->perfmon_lock);
        perfmon_unref(perfmon);
        errno = EINVAL;
        return -1;
    }

    process->active_perfmon = perfmon;
    spinlock_release(&process->perfmon_lock);
    return 0;
}

/* Return index into mountpoints array corresponding to the mountpoint
   inside which this file/path is located.
   char **local_path will return a pointer (in *local_path) to the
   part of the path inside the mountpoint. */
int vfs_get_mountpoint(const char *path, char **local_path) {
    size_t guess = -1;
    size_t guess_size = 0;

    for (size_t i = 0; i < mountpoints_i; i++) {
        size_t len = kstrlen(mountpoints[i].mntpt);

        if (!kstrncmp(path, mountpoints[i].mntpt, len)) {
            if ( (((path[len] == '/') || (path[len] == '\0'))
                 || (!kstrcmp(mountpoints[i].mntpt, "/")))
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

    return (int)guess;
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

/* Find free file handle and setup said handle */
int open(const char *path, int mode) {
    struct vfs_handle_t fd = {0};

    char *loc_path;

    int mountpoint = vfs_get_mountpoint(path, &loc_path);
    if (mountpoint == -1) return -1;

    int magic = mountpoints[mountpoint].magic;
    int fs = mountpoints[mountpoint].fs;

    int intern_fd = filesystems[fs].open(loc_path, mode, magic);
    if (intern_fd == -1) return -1;

    fd.used = 1;
    fd.type = FD_FILE;
    fd.fs = fs;
    fd.intern_fd = intern_fd;

    return create_fd(&fd);
}

int dup(int fd) {
    switch (file_descriptors[fd].type) {
        case FD_FILE: {
            int fs = file_descriptors[fd].fs;
            int intern_fd = file_descriptors[fd].intern_fd;

            if (filesystems[fs].dup(intern_fd))
                return -1;

            return create_fd(&file_descriptors[fd]);
        }
        case FD_PIPE_WRITE:
        case FD_PIPE_READ: {
            struct pipe_t *pipe = file_descriptors[fd].pipe;
            spinlock_acquire(&pipe->lock);
            pipe->refcount++;
            spinlock_release(&pipe->lock);
            return create_fd(&file_descriptors[fd]);
        }
        case FD_PERFMON: {
            struct pipe_t *perfmon = file_descriptors[fd].perfmon;
            perfmon_ref(perfmon);
            return create_fd(&file_descriptors[fd]);
        }
        default:
            errno = EINVAL;
            return -1;
    }
}

int readdir(int fd, struct dirent *buf) {
    switch (file_descriptors[fd].type) {
        case FD_FILE: {
            int fs = file_descriptors[fd].fs;
            int intern_fd = file_descriptors[fd].intern_fd;

            return filesystems[fs].readdir(intern_fd, buf);
        }
        case FD_PIPE_WRITE:
        case FD_PIPE_READ:
        default:
            errno = EINVAL;
            return -1;
    }
}

int read(int fd, void *buf, size_t len) {
    switch (file_descriptors[fd].type) {
        case FD_FILE: {
            int fs = file_descriptors[fd].fs;
            int intern_fd = file_descriptors[fd].intern_fd;

            return filesystems[fs].read(intern_fd, buf, len);
        }
        case FD_PIPE_READ:
            if (file_descriptors[fd].fdflags & O_NONBLOCK)
                return pipe_read(file_descriptors[fd].pipe, buf, len, 0);
            else
                return pipe_read(file_descriptors[fd].pipe, buf, len, 1);
        case FD_PIPE_WRITE:
            errno = EINVAL;
            return -1;
        case FD_PERFMON:
            return perfmon_read(file_descriptors[fd].perfmon, buf, len);
        default:
            errno = EINVAL;
            return -1;
    }
}

int write(int fd, const void *buf, size_t len) {
    switch (file_descriptors[fd].type) {
        case FD_FILE: {
            int fs = file_descriptors[fd].fs;
            int intern_fd = file_descriptors[fd].intern_fd;

            return filesystems[fs].write(intern_fd, buf, len);
        }
        case FD_PIPE_WRITE:
            if (file_descriptors[fd].fdflags & O_NONBLOCK)
                return pipe_write(file_descriptors[fd].pipe, buf, len, 0);
            else
                return pipe_write(file_descriptors[fd].pipe, buf, len, 1);
        case FD_PIPE_READ:
        default:
            errno = EINVAL;
            return -1;
    }
}

int close(int fd) {
    int ret;

    switch (file_descriptors[fd].type) {
        case FD_FILE: {
            int fs = file_descriptors[fd].fs;
            int intern_fd = file_descriptors[fd].intern_fd;

            ret = filesystems[fs].close(intern_fd);
            if (ret)
                return ret;
            break;
        }
        case FD_PIPE_WRITE:
        case FD_PIPE_READ:
            pipe_close(file_descriptors[fd].pipe);
            ret = 0;
            break;
        case FD_PERFMON:
            perfmon_unref(file_descriptors[fd].perfmon);
            ret = 0;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    file_descriptors[fd].used = 0;

    return ret;
}

int lseek(int fd, off_t offset, int type) {
    switch (file_descriptors[fd].type) {
        case FD_FILE: {
            int fs = file_descriptors[fd].fs;
            int intern_fd = file_descriptors[fd].intern_fd;

            return filesystems[fs].lseek(intern_fd, offset, type);
        }
        case FD_PIPE_WRITE:
        case FD_PIPE_READ:
            errno = ESPIPE;
            return -1;
        default:
            errno = EINVAL;
            return -1;
    }
}

int sync(void) {
    for (size_t i = 0; i < filesystems_i; i++)
        filesystems[i].sync();
    return 0;
}

int mount(const char *source, const char *target,
          const char *fs_type, unsigned long m_flags,
          const void *data) {
    size_t i;
    /* Search for fs with the correct type, since we know nothing
     * about the fs from the path given */
    for (i = 0; i < filesystems_i; i++) {
        if (!kstrcmp(filesystems[i].type, fs_type)) break;
    }

    int res = filesystems[i].mount(source, m_flags, data);
    if (res == -1) return -1;

    mountpoints = krealloc(mountpoints, (mountpoints_i + 1) * sizeof(struct mnt_t));

    kstrcpy(mountpoints[mountpoints_i].mntpt, target);
    mountpoints[mountpoints_i].fs = i;
    mountpoints[mountpoints_i].magic = res;

    mountpoints_i++;

    kprint(KPRN_INFO, "vfs: Mounted `%s` on `%s`.", source, target);

    return 0;
}

int fstat(int fd, struct stat *st) {
    switch (file_descriptors[fd].type) {
        case FD_FILE: {
            int fs = file_descriptors[fd].fs;
            int intern_fd = file_descriptors[fd].intern_fd;

            return filesystems[fs].fstat(intern_fd, st);
        }
        case FD_PIPE_WRITE:
        case FD_PIPE_READ:
            st->st_dev = 0;
            st->st_ino = 0;
            st->st_nlink = 0;
            st->st_uid = 0;
            st->st_gid = 0;
            st->st_rdev = 0;
            st->st_size = 0;
            st->st_blksize = 0;
            st->st_blocks = 0;
            st->st_atim.tv_sec = 0;
            st->st_atim.tv_nsec = 0;
            st->st_mtim.tv_sec = 0;
            st->st_mtim.tv_nsec = 0;
            st->st_ctim.tv_sec = 0;
            st->st_ctim.tv_nsec = 0;
            st->st_mode = 0;
            st->st_mode |= S_IFIFO;
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

void fs_sync_worker(void *arg) {
    (void)arg;

    for (;;) {
        yield(2000);
        sync();
    }
}

static int fs_call_invalid(void) {
    kprint(KPRN_WARN, "vfs: Unimplemented filesystem call occurred, returning ENOSYS!");
    errno = ENOSYS;
    return -1;
}

int vfs_install_fs(struct fs_t filesystem) {
    /* check if any entry is bogus */
    /* XXX make this prettier */
    size_t *p = ((void *)&filesystem) + 256; // 256 == size of type
    for (size_t i = 0; i < (sizeof(struct fs_t) - 256) / sizeof(size_t); i++)
        if (!p[i])
            p[i] = (size_t)fs_call_invalid;

    filesystems = krealloc(filesystems, (filesystems_i + 1) * sizeof(struct fs_t));

    filesystems[filesystems_i++] = filesystem;

    return 0;
}

void init_vfs(void) {
    file_descriptors = kalloc(256 * sizeof(struct vfs_handle_t));
    fd_count = 256;

    return;
}
