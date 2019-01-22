#include <stdint.h>
#include <stddef.h>
#include <lib/cio.h>
#include <lib/klib.h>
#include <misc/serial.h>
#include <misc/tty.h>
#include <misc/vga_textmode.h>
#include <misc/vbe_tty.h>
#include <misc/vbe.h>
#include <sys/e820.h>
#include <mm/mm.h>
#include <sys/idt.h>
#include <sys/pic.h>
#include <acpi/acpi.h>
#include <lib/cmdline.h>
#include <misc/pit.h>
#include <sys/smp.h>
#include <user/task.h>
#include <devices/storage/ata/ata.h>
#include <devices/dev.h>
#include <fd/vfs/vfs.h>
#include <user/elf.h>
#include <misc/pci.h>
#include <devices/storage/ahci/ahci.h>
#include <lib/time.h>
#include <misc/kbd.h>
#include <sys/irq.h>
#include <sys/panic.h>
#include <fs/fs.h>

void kmain_thread(void *arg) {
    (void)arg;

    init_net_i8254x();

    /* Launch the urm */
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(userspace_request_monitor, 0));

    /* Launch the device cache sync worker */
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(device_sync_worker, 0));

    /* Launch the fs cache sync worker */
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(vfs_sync_worker, 0));

    int tty = open("/dev/tty", O_RDWR);

    char *root = cmdline_get_value("root");
    char new_root[64];
    if (!root) {
        char *dev;
        kprint(KPRN_WARN, "kmain: Command line argument \"root\" not specified.");
        kprint(KPRN_INFO, "List of available devices:");
        for (size_t i = 0; (dev = device_list(i)); i++)
            kprint(KPRN_INFO, "/dev/%s", dev);
        readline(tty, "Select root device: ", new_root, 64);
        root = new_root;
    } else {
        kstrcpy(new_root, root);
        root = new_root;
    }
    kprint(KPRN_INFO, "kmain: root=%s", new_root);

    char *rootfs = cmdline_get_value("rootfs");
    char new_rootfs[64];
    if (!rootfs) {
        kprint(KPRN_WARN, "kmain: Command line argument \"rootfs\" not specified.");
        readline(tty, "Root filesystem to use: ", new_rootfs, 64);
        rootfs = new_rootfs;
    } else {
        kstrcpy(new_rootfs, rootfs);
        rootfs = new_rootfs;
    }
    kprint(KPRN_INFO, "kmain: rootfs=%s", new_rootfs);

    char *init = cmdline_get_value("init");
    char new_init[64];
    if (!init) {
        kprint(KPRN_WARN, "kmain: Command line argument \"init\" not specified.");
        readline(tty, "Location of init: ", new_init, 64);
        init = new_init;
    } else {
        kstrcpy(new_init, init);
        init = new_init;
    }
    kprint(KPRN_INFO, "kmain: init=%s", new_init);

    close(tty);

    /* Mount root partition */
    if (mount(root, "/", rootfs, 0, 0)) {
        panic("Unable to mount root", 0, 0);
    }

    /* Execute init process */
    kprint(KPRN_INFO, "kmain: Starting init");
    const char *args[] = { init, NULL };
    const char *environ[] = { NULL };
    if (kexec(init, args, environ, "/dev/tty", "/dev/tty", "/dev/tty") == -1) {
        panic("Unable to launch init", 0, 0);
    }

    kprint(KPRN_INFO, "kmain: End of kmain");

    /* kill kmain now */
    task_tkill(CURRENT_PROCESS, CURRENT_THREAD);

    for (;;) asm volatile ("hlt;");
}

/* Main kernel entry point, all the things should be initialised */
void kmain(void) {
    init_idt();

    init_vga_textmode();

    init_tty();

    kprint(KPRN_INFO, "Kernel booted");
    kprint(KPRN_INFO, "Build time: %s", BUILD_TIME);
    kprint(KPRN_INFO, "Command line: %s", cmdline);

    /* Memory-related stuff */
    init_e820();
    init_pmm();
    init_vmm();

    /* Early inits */
    init_vbe();
    init_vbe_tty();

    /* Time stuff */
    struct s_time_t s_time;
    bios_get_time(&s_time);
    kprint(KPRN_INFO, "Current date & time: %u/%u/%u %u:%u:%u",
           s_time.years, s_time.months, s_time.days,
           s_time.hours, s_time.minutes, s_time.seconds);
    unix_epoch = get_unix_epoch(s_time.seconds, s_time.minutes, s_time.hours,
                                s_time.days, s_time.months, s_time.years);
    kprint(KPRN_INFO, "Current unix epoch: %U", unix_epoch);

    /*** NO MORE REAL MODE CALLS AFTER THIS POINT ***/
    flush_irqs();
    init_acpi();
    init_pic();

    /* Enable interrupts on BSP */
    asm volatile ("sti");

    init_pit();
    init_smp();

    /* Initialise device drivers */
    init_pci();
    init_ahci();
    init_ata();
    init_kbd();

    /* Initialise filesystem drivers */
    init_fs();

    /* Mount /dev */
    mount("devfs", "/dev", "devfs", 0, 0);

    /* Initialise scheduler */
    init_sched();

    /* Unlock the scheduler for the first time */
    spinlock_release(&scheduler_lock);

    /* Start a main kernel thread which will take over when the scheduler is running */
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(kmain_thread, 0));

    /*** DO NOT ADD ANYTHING TO THIS FUNCTION AFTER THIS POINT, ADD TO kmain_thread
         INSTEAD! ***/

    /* Pre-scheduler init done. Wait for the main kernel thread to be scheduled. */
    for (;;) asm volatile ("hlt");
}
