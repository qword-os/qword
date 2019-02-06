#ifndef __VFS_H__
#define __VFS_H__

#include <stddef.h>
#include <fd/fd.h>
#include <lib/klib.h>

/* A filesystem, defined by the function pointers that allow us to access it */
struct fs_t {
    char name[256];
    int (*mount)(const char *, unsigned long, const void *);
    int (*umount)(const char *);
    int (*open)(const char *, int, int);
    int (*close)(int);
    int (*fstat)(int, struct stat *);
    int (*read)(int, void *, size_t);
    int (*write)(int, const void *, size_t);
    int (*lseek)(int, off_t, int);
    int (*dup)(int);
    int (*readdir)(int, struct dirent *);
    int (*sync)(void);
    int (*tcgetattr)(int, struct termios *);
    int (*tcsetattr)(int, int, struct termios *);
};

/* VFS calls */
int mount(const char *, const char *, const char *, unsigned long, const void *);
int umount(const char *);
int open(const char *, int);

int vfs_sync(void);
void vfs_sync_worker(void *);
void vfs_get_absolute_path(char *, const char *, const char *);
int vfs_install_fs(struct fs_t *);

#endif
