#include <stdint.h>
#include <stddef.h>
#include <cio.h>
#include <klib.h>
#include <serial.h>
#include <tty.h>
#include <vga_textmode.h>
#include <vbe_tty.h>
#include <vbe.h>
#include <e820.h>
#include <mm.h>
#include <idt.h>
#include <pic.h>
#include <acpi.h>
#include <cmdline.h>
#include <pit.h>
#include <smp.h>
#include <task.h>
#include <ata.h>
#include <dev.h>
#include <fs.h>
#include <elf.h>
#include <pci.h>
#include <ahci.h>
#include <time.h>
#include <kbd.h>
#include <irq.h>
#include <panic.h>

void kmain_thread(void *arg) {
    (void)arg;

    int tty = open("/dev/tty", 0, 0);

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

    struct s_time_t s_time;

    bios_get_time(&s_time);
    kprint(KPRN_INFO, "Current date & time: %u/%u/%u %u:%u:%u",
           s_time.years, s_time.months, s_time.days,
           s_time.hours, s_time.minutes, s_time.seconds);

    /* Memory-related stuff */
    init_e820();
    init_pmm();
    init_vmm();

    /* Early inits */
    init_vbe();
    init_vbe_tty();

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

    /* Initialise Virtual Filesystem */
    init_vfs();

    /* Initialise filesystem drivers */
    init_devfs();
    init_echfs();
    init_iso9660();

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
