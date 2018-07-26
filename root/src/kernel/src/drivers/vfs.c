#include <fs.h>
#include <task.h>
#include <klib.h>
#include <smp.h>

fs_t *filesystems;
mnt_t *mountpoints;

static int mountpoints_count = 0;
static int filesystems_count = 0;

/* Store a previously created file handle in the handle table of the given process */
int vfs_create_fd(pid_t pid, file_handle_t *new_handle) {
    int i;

    /* Search for already-free handle */
    for (i = 0; i < process_table[pid]->fd_count; i++) {
        file_handle_t *fd = process_table[pid]->file_handles[i];

        if (fd == (void *)(size_t)(-1)) {
            process_table[pid]->file_handles[i] = new_handle;
        } else {
            continue;
        }
    }

    /* If we reach this point, there were no free entries. Make more space for file handles */
    process_table[pid]->file_handles = krealloc(process_table[pid]->file_handles, ((process_table[pid]->fd_count + 256) * sizeof(file_handle_t)));
    /* Return next index */
    int j = i + 1;
    process_table[pid]->file_handles[j] = new_handle;
    return j;
}

/* Return index into mountpoints array corresponding to this path */
int vfs_translate_mnt(char *path, char **local_path) {
    int guess = -1;
    int guess_size = 0;

    for (int i = 0; i < mountpoints_count; i++) {
        size_t len = kstrlen(mountpoints[i].mntpt);

        /* Check edge cases */
        if (!kstrncmp(path, mountpoints[i].mntpt, len)) {
            if ( (((path[len] == '/') || (path[len] == '\0'))
                 || (!kstrcmp(mountpoints[i].mntpt, "/")))
               && (len > guess_size)) {
                guess = i;
                guess_size = len;
            }
        }
    }

    *local_path = path + guess_size;
    if (**local_path == '/') *local_path++;
    return guess;
}

int vfs_translate_fs(int mountpoint) {
    for (int i = 0; i < filesystems_count; i++) {
        if (!kstrcmp(filesystems[i].name, mountpoints[i].fs)) {
            return i;
        }
    }

    return -1;
}

/* Convert a relative path into an absolute path */
void vfs_get_absolute_path(char *path_ptr, char *path) {
    char *orig_ptr = path_ptr;
    
    pid_t current_process = cpu_locals[current_cpu].current_process;

    if (!*path) {
        kstrcpy(path_ptr, process_table[current_process]->cwd);
        return;
    }

    if (*path != '/') {
        kstrcpy(path_ptr, process_table[current_process]->cwd);
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
                if ((*(path_ptr - 1) == '/') && ((path_ptr - 1) != orig_ptr)) path_ptr--;
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

int vfs_fstat(int fd, void *buf) {
    pid_t current_process = cpu_locals[current_cpu].current_process;
    
    int intern_fd = process_table[current_cpu]->file_handles[fd]->intern_fd;
    int fs_id = vfs_translate_fs(process_table[current_process]->file_handles[fd]->mountpoint);
    return (*filesystems[fs_id].fstat)(intern_fd, buf);
}

/* Open a file and return a file descriptor */
int vfs_open(char *path, int flags, int mode) {
    char *loc_path;
    char absolute_path[2048];
    
    file_handle_t handle = {0}; 

    /* TODO (possibly) account for ::// in absolute path determination */
    if (!kstrncmp(path, ":://", 4)) {
        loc_path = path + 4;
        kstrcpy(absolute_path, "/dev/");
        kstrcpy(absolute_path + 5, loc_path);
    } else {
        vfs_get_absolute_path(absolute_path, path);
    }

    int mountpoint = vfs_translate_mnt(absolute_path, &loc_path);
    if (mountpoint == -1) return -1;

    int fs_id = vfs_translate_fs(mountpoint);
    if (mountpoint == -1) return -1;
    
    spinlock_acquire(&scheduler_lock);
    int internal_handle = (*filesystems[fs_id].open)(loc_path, flags, mode);
    if (internal_handle == -1) return -1;

    handle.fs_id = fs_id;
    handle.intern_fd = internal_handle;
    handle.mountpoint = mountpoint;    

    pid_t current_process = cpu_locals[current_cpu].current_process;

    int ret = vfs_create_fd(current_process, &handle);
    spinlock_release(&scheduler_lock);

    return ret;
}

/* Read `len` bytes from the file given by `fd` into the given buffer */
int vfs_read(int fd, void *buf, size_t len) {
    pid_t current_process = cpu_locals[current_cpu].current_process;
    /* Determine the FS from the mountpoint */
    int fs_id = vfs_translate_fs(process_table[current_process]->file_handles[fd]->mountpoint);
    if (fs_id == -1) return -1;

    return (*filesystems[fs_id].read)(process_table[current_process]->file_handles[fd]->intern_fd, buf, len);
}
