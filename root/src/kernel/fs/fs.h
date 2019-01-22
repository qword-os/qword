#ifndef __FS_H__
#define __FS_H__

int init_fs(void);

int init_fs_devfs(void);
int init_fs_echfs(void);
int init_fs_iso9660(void);

#endif
