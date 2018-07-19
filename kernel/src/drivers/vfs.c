#include <fs.h>
#include <task.h>
#include <klib.h>

fs_t *filesystems;
mnt_t *mountpoints;

static int mountpoints_count = 0;
static int filesystems_count = 0;

/* Store a previously created file handle in the handle table of the given process */
int vfs_create_fd(pid_t pid, fd_t new_handle) {
    int i;

    /* Search for already-free handle */
    for (i = 0; i < process_table[pid]->fd_count; i++) {
        fd_t fd = process_table[pid]->file_handles[i];

        if ((int)&fd == -1) {
            process_table[pid]->file_handles[i] = new_handle;
        } else {
            continue;
        }
    }

    /* If we reach this point, there were no free entries. Make more space for file handles */
    process_table[pid]->file_handles = krealloc(process_table[pid]->file_handles, ((process_table[pid]->fd_count + 256) * sizeof(fd_t)));
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

int vfs_kopen(int fd, int perms, int mode) {

}
