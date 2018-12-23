#ifndef __FS_H__
#define __FS_H__

#include <stdint.h>
#include <stddef.h>
#include <dev.h>
#include <task.h>
#include <time.h>

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

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
typedef int32_t mode_t;
typedef int32_t nlink_t;
typedef int64_t blksize_t;
typedef int64_t blkcnt_t;

struct vfs_handle_t {
    int used;
    int fs;
    int intern_fd;
};

struct mnt_t {
    char mntpt[2048];
    int fs;
    int magic;
};

#define S_IFMT 0x0F00
#define S_IFBLK 0x0000
#define S_IFCHR 0x0100
#define S_IFIFO 0x0200
#define S_IFREG 0x0300
#define S_IFDIR 0x0400
#define S_IFLNK 0x0500
#define S_IFSOCK 0x0600

#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

/* A filesystem, defined by the function pointers that allow us to access it */
struct fs_t {
    char type[256];
    int (*mount)(const char *, unsigned long, const void *);
    int (*umount)(const char *);
    int (*open)(const char *, int, int, int);
    int (*close)(int);
    int (*fstat)(int, struct stat *);
    int (*read)(int, void *, size_t);
    int (*write)(int, const void *, size_t);
    int (*lseek)(int, off_t, int);
    int (*dup)(int);
};

/* VFS calls */
int mount(const char *, const char *, const char *, unsigned long, const void *);
int umount(const char *);
int open(const char *, int, int);
int close(int);
int fstat(int, struct stat *);
int read(int, void *, size_t);
int write(int, const void *, size_t);
int lseek(int, off_t, int);
int dup(int);

/* VFS specific functions */
int vfs_get_mountpoint(const char *, char **);
void vfs_get_absolute_path(char *, const char *, const char *);
int vfs_install_fs(struct fs_t);
void init_vfs(void);

void init_devfs(void);
void init_echfs(void);
void init_iso9660(void);

#endif
