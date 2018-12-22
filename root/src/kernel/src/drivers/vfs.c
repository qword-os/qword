#include <stdint.h>
#include <stddef.h>
#include <fs.h>
#include <task.h>
#include <klib.h>
#include <smp.h>

struct fs_t *filesystems;
struct mnt_t *mountpoints;
struct vfs_handle_t *file_descriptors;

static size_t mountpoints_i = 0;
static size_t filesystems_i = 0;
static size_t fd_count = 0;

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

    *local_path = (char *)path + guess_size;
    if (**local_path == '/')
        (*local_path)++;

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

static int create_fd(int fs, int intern_fd) {
    size_t i = 0;

retry:
    for ( ; i < fd_count; i++) {
        if (!file_descriptors[i].used) {
            file_descriptors[i].fs = fs;
            file_descriptors[i].intern_fd = intern_fd;
            file_descriptors[i].used = 1;

            return (int)i;
        }
    }

    /* Make more space */
    fd_count += 256;
    file_descriptors = krealloc(file_descriptors, fd_count * sizeof(struct vfs_handle_t));

    goto retry;
}

/* Find free file handle and setup said handle */
int open(const char *path, int mode, int perms) {
    char *loc_path;

    int mountpoint = vfs_get_mountpoint(path, &loc_path);
    if (mountpoint == -1) return -1;

    int magic = mountpoints[mountpoint].magic;
    int fs = mountpoints[mountpoint].fs;

    int intern_fd = filesystems[fs].open(loc_path, mode, perms, magic);
    if (intern_fd == -1) return -1;

    return create_fd(fs, intern_fd);
}

int dup(int fd) {
    int fs = file_descriptors[fd].fs;
    int intern_fd = file_descriptors[fd].intern_fd;

    int new_intern_fd = filesystems[fs].dup(intern_fd);

    return create_fd(fs, new_intern_fd);
}

int read(int fd, void *buf, size_t len) {
    int fs = file_descriptors[fd].fs;
    int intern_fd = file_descriptors[fd].intern_fd;

    return filesystems[fs].read(intern_fd, buf, len);
}

int write(int fd, const void *buf, size_t len) {
    int fs = file_descriptors[fd].fs;
    int intern_fd = file_descriptors[fd].intern_fd;

    int res = filesystems[fs].write(intern_fd, buf, len);

    return res;
}

int close(int fd) {
    if (fd < 0) return -1;

    int fs = file_descriptors[fd].fs;
    int intern_fd = file_descriptors[fd].intern_fd;

    int res = filesystems[fs].close(intern_fd);
    if (res == -1) return -1;
    file_descriptors[fd].used = 0;

    return res;
}

int lseek(int fd, off_t offset, int type) {
    int fs = file_descriptors[fd].fs;
    int intern_fd = file_descriptors[fd].intern_fd;

    return filesystems[fs].lseek(intern_fd, offset, type);
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

int fstat(int fd, struct stat *buffer) {
    int fs = file_descriptors[fd].fs;
    int intern_fd = file_descriptors[fd].intern_fd;

    return filesystems[fs].fstat(intern_fd, buffer);
}

int vfs_install_fs(struct fs_t filesystem) {
    filesystems = krealloc(filesystems, (filesystems_i + 1) * sizeof(struct fs_t));

    filesystems[filesystems_i++] = filesystem;

    return 0;
}

void init_vfs(void) {
    file_descriptors = kalloc(256 * sizeof(struct vfs_handle_t));
    fd_count = 256;

    return;
}
