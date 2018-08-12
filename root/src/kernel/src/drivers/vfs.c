#include <stdint.h>
#include <stddef.h>
#include <fs.h>
#include <task.h>
#include <klib.h>
#include <smp.h>

fs_t *filesystems;
mnt_t *mountpoints;

static int mountpoints_i = 0;
/*static int filesystems_i = 0;*/

/* Return index into mountpoints array corresponding to the mountpoint
   inside which this file/path is located.
   char **local_path will return a pointer (in *local_path) to the
   part of the path inside the mountpoint. */
int vfs_get_mountpoint(const char *path, char **local_path) {
    size_t guess = -1;
    size_t guess_size = 0;

    for (int i = 0; i < mountpoints_i; i++) {
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

/* Anything that was below this point was completely broken so I removed it */
/* To redo */
