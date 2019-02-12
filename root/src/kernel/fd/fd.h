#ifndef __FD_H__
#define __FD_H__

#include <stddef.h>
#include <lib/time.h>
#include <lib/dynarray.h>
#include <lib/types.h>
#include <devices/term/tty/tty.h>  // for termios
#include <lib/errno.h>

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

struct fd_handler_t {
    int (*close)(int);
    int (*fstat)(int, struct stat *);
    int (*read)(int, void *, size_t);
    int (*write)(int, const void *, size_t);
    int (*lseek)(int, off_t, int);
    int (*dup)(int);
    int (*readdir)(int, struct dirent *);
    int (*tcgetattr)(int, struct termios *);
    int (*tcsetattr)(int, int, struct termios *);
    int (*getfdflags)(int);
    int (*setfdflags)(int, int);
    int (*getflflags)(int);
    int (*setflflags)(int, int);
};

struct file_descriptor_t {
    int intern_fd;
    struct fd_handler_t fd_handler;
};

public_dynarray_prototype(struct file_descriptor_t, file_descriptors);

void init_fd(void);
int fd_create(struct file_descriptor_t *);
int close(int);
int fstat(int, struct stat *);
int read(int, void *, size_t);
int write(int, const void *, size_t);
int lseek(int, off_t, int);
int dup(int);
int readdir(int, struct dirent *);
int tcgetattr(int, struct termios *);
int tcsetattr(int, int, struct termios *);
int getfdflags(int);
int setfdflags(int, int);
int getflflags(int);
int setflflags(int, int);

__attribute__((unused)) static int bogus_fstat() {
    errno = EINVAL;
    return -1;
}

__attribute__((unused)) static int bogus_close() {
    errno = EIO;
    return -1;
}

__attribute__((unused)) static int bogus_readdir() {
    errno = ENOTDIR;
    return -1;
}

__attribute__((unused)) static int bogus_dup() {
    errno = EINVAL;
    return -1;
}

__attribute__((unused)) static int bogus_read() {
    errno = EINVAL;
    return -1;
}

__attribute__((unused)) static int bogus_write() {
    errno = EINVAL;
    return -1;
}

__attribute__((unused)) static int bogus_lseek() {
    errno = EINVAL;
    return -1;
}

__attribute__((unused)) static int bogus_flush() {
    return 1;
}

__attribute__((unused)) static int bogus_tcgetattr() {
    errno = ENOTTY;
    return -1;
}

__attribute__((unused)) static int bogus_tcsetattr() {
    errno = ENOTTY;
    return -1;
}

__attribute__((unused)) static int bogus_getfdflags() {
    return 0;
}

__attribute__((unused)) static int bogus_setfdflags() {
    return 0;
}

__attribute__((unused)) static int bogus_getflflags() {
    return 0;
}

__attribute__((unused)) static int bogus_setflflags() {
    return 0;
}

__attribute__((unused)) static struct fd_handler_t default_fd_handler = {
    (void *)bogus_close,
    (void *)bogus_fstat,
    (void *)bogus_read,
    (void *)bogus_write,
    (void *)bogus_lseek,
    (void *)bogus_dup,
    (void *)bogus_readdir,
    (void *)bogus_tcgetattr,
    (void *)bogus_tcsetattr,
    (void *)bogus_getfdflags,
    (void *)bogus_setfdflags,
    (void *)bogus_getflflags,
    (void *)bogus_setflflags
};

#endif
