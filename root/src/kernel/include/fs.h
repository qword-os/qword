#ifndef __FS_H__
#define __FS_H__

#include <stdint.h>
#include <stddef.h>

/* Possible types of file on a filesystem */
#define FTYPE_FILE 0
#define FTYPE_DIR 1
#define FTYPE_DEV 2

typedef size_t pid_t;

typedef struct {
    int used;
    int perms;
    size_t sz;
    size_t offset;
    uint8_t *start;
} stat_t;

typedef struct {
    int fs_id;
    int intern_fd;
    int mountpoint;
} file_handle_t;

typedef struct {
    char filename[2048];
    int filetype;
    uint64_t size;
} vfs_meta_t;

typedef struct {
    char mntpt[2048];
    char dev[2048];
    char fs[2048];
} mnt_t;

/* A filesystem, defined by the functions that allow us to access
 * it */
typedef struct {
    char name[128];
    int (*read)(int fd, void *buf, size_t count);
    int (*write)(int fd, const void *buf, size_t count);
    int (*remove)(char *path);
    int (*mkdir)(char *path, uint16_t perms);
    int (*create)(char *path, uint16_t perms);
    int (*get_metadata)(char *path, vfs_meta_t *meta, int ftype);
    int (*mount)(char *dev, char *target);
    int (*open)(char *path, int flags, int mode);
    int (*close)(int fd);
    int (*fork)(int fd);
    int (*seek)(int fd, int offset, int ftype);
} fs_t;

int vfs_translate_mnt(char *, char **);
int vfs_translate_fs(int);
int vfs_create_fd(pid_t pid, file_handle_t *fd);
int vfs_open(char *, int flags, int mode);
int vfs_read(int, void *, size_t);
int vfs_write(int, void *, size_t);

#endif
