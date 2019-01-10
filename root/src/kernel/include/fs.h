#ifndef __FS_H__
#define __FS_H__

#include <stdint.h>
#include <stddef.h>
#include <dev.h>
#include <task.h>
#include <time.h>

/* from options/ansi/include/bits/ansi/seek.h in mlibc */
#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 3

/* from abi_bits */
// reserve 3 bits for the access mode
#define O_ACCMODE 0x0007
#define O_EXEC 1
#define O_RDONLY 2
#define O_RDWR 3
#define O_SEARCH 4
#define O_WRONLY 5
// all remaining flags get their own bit
#define O_APPEND 0x0008
#define O_CREAT 0x0010
#define O_DIRECTORY 0x0020
#define O_EXCL 0x0040
#define O_NOCTTY 0x0080
#define O_NOFOLLOW 0x0100
#define O_TRUNC 0x0200
#define O_NONBLOCK 0x0400
#define O_DSYNC 0x0800
#define O_RSYNC 0x1000
#define O_SYNC 0x2000
#define O_CLOEXEC 0x4000

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
    int fdflags;
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

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14

struct dirent {
	ino_t d_ino;
	off_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[1024];
};

/* A filesystem, defined by the function pointers that allow us to access it */
struct fs_t {
    char type[256];
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
};

/* VFS calls */
int mount(const char *, const char *, const char *, unsigned long, const void *);
int umount(const char *);
int open(const char *, int);
int close(int);
int fstat(int, struct stat *);
int read(int, void *, size_t);
int write(int, const void *, size_t);
int lseek(int, off_t, int);
int dup(int);
int readdir(int, struct dirent *);
int sync(void);

void fs_sync_worker(void *);

/* VFS specific functions */
int vfs_get_mountpoint(const char *, char **);
void vfs_get_absolute_path(char *, const char *, const char *);
int vfs_install_fs(struct fs_t);
void init_vfs(void);

void init_devfs(void);
void init_echfs(void);
void init_iso9660(void);

extern struct vfs_handle_t *file_descriptors;

#endif
