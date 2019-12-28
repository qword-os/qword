#include <devices/dev.h>
#include <devices/storage/nvme/nvme.h>
#include <fs/devfs/devfs.h>
#include <proc/task.h>

void init_dev_streams(void);
void init_dev_tty(void);
void init_dev_ide(void);
void init_dev_sata(void);
void init_dev_vesafb(void);

void init_dev(void) {
    init_dev_streams();
    init_dev_tty();
    init_dev_ide();
    init_dev_nvme();
    init_dev_sata();
    init_dev_vesafb();

    /* Launch the device cache sync worker */
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(0, device_sync_worker, 0));
}
