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
    int (*tcflow)(int, int);
    int (*isatty)(int);
};

__attribute__((unused)) static int bogus_mount() {
    errno = EIO;
    return -1;
}

__attribute__((unused)) static int bogus_umount() {
    errno = EIO;
    return -1;
}

__attribute__((unused)) static int bogus_open() {
    errno = EIO;
    return -1;
}

__attribute__((unused)) static int bogus_sync() {
    errno = EIO;
    return -1;
}

__attribute__((unused)) static struct fs_t default_fs_handler = {
    "bogusfs",
    (void *)bogus_mount,
    (void *)bogus_umount,
    (void *)bogus_open,
    (void *)bogus_close,
    (void *)bogus_fstat,
    (void *)bogus_read,
    (void *)bogus_write,
    (void *)bogus_lseek,
    (void *)bogus_dup,
    (void *)bogus_readdir,
    (void *)bogus_sync,
    (void *)bogus_tcgetattr,
    (void *)bogus_tcsetattr,
    (void *)bogus_tcflow,
    (void *)bogus_isatty
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
