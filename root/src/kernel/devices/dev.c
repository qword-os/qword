#include <devices/dev.h>
#include <fs/devfs/devfs.h>
#include <user/task.h>

void init_dev_streams(void);
void init_dev_tty(void);
void init_dev_ata(void);
void init_dev_ahci(void);

void init_dev(void) {
    init_dev_streams();
    init_dev_tty();
    init_dev_ata();
    init_dev_ahci();

    /* Launch the device cache sync worker */
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(device_sync_worker, 0));
}
