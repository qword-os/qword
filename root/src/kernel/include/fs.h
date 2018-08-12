#ifndef __FS_H__
#define __FS_H__

#include <stdint.h>
#include <stddef.h>
#include <dev.h>
#include <task.h>

/* Flags */
#define O_RDONLY        0b0001
#define O_WRONLY        0b0010
#define O_RDWR          0b0100

/* Mode */
#define O_APPEND        0b001000
#define O_CREAT         0b010000
#define O_TRUNC         0b100000

/* FIXME perhaps define off_t somewhere else */
/* We only need it for stat for now, so this is fine */
typedef int64_t off_t;

typedef uint64_t ino_t;
typedef int mode_t;
typedef uint64_t nlink_t;
typedef uint64_t blksize_t;
typedef uint64_t blkcnt_t;

typedef struct {
    int fs_id;
    int intern_fd;
    int mountpoint;
} vfs_handle_t;

typedef struct {
    char mntpt[2048];
    char dev[2048];
    char fs[2048];
} mnt_t;

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

/* A filesystem, defined by the function pointers that allow us to access it */
typedef struct {
    char name[128];
    int (*mount)(const char *, const char *, const char *,
                    unsigned long, const void *);
    int (*umount)(const char *);
    int (*open)(char *, int, int);
    int (*close)(int);
    int (*fstat)(int, struct stat *);
    int (*read)(int, void *, size_t);
    int (*write)(int, void *, size_t);
} fs_t;

/* VFS calls */
int mount(const char *, const char *, const char *,
            unsigned long, const void *);
int umount(const char *);
int open(char *, int, int);
int close(int);
int fstat(int, struct stat *);
int read(int, void *, size_t);
int write(int, void *, size_t);

/* VFS specific functions */
int vfs_get_mountpoint(const char *, char **);
void vfs_get_absolute_path(char *, const char *, const char *);

#endif
