#include <fs/fs.h>

int init_fs(void) {
    if (init_fs_devfs()) return -1;
    if (init_fs_echfs()) return -1;
    if (init_fs_iso9660()) return -1;

    return 0;
}
