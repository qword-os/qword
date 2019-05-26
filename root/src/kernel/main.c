#include <stdint.h>
#include <stddef.h>
#include <lib/cio.h>
#include <lib/klib.h>
#include <misc/serial.h>
#include <devices/display/vbe/vbe.h>
#include <devices/term/tty/tty.h>
#include <sys/e820.h>
#include <mm/mm.h>
#include <sys/idt.h>
#include <sys/pic.h>
#include <acpi/acpi.h>
#include <lib/cmdline.h>
#include <misc/pit.h>
#include <sys/smp.h>
#include <proc/task.h>
#include <devices/dev.h>
#include <fd/vfs/vfs.h>
#include <proc/elf.h>
#include <misc/pci.h>
#include <lib/time.h>
#include <sys/irq.h>
#include <sys/panic.h>
#include <fs/fs.h>
#include <fd/fd.h>
#include <devices/dev.h>
#include <sys/vga_font.h>
#include <lib/rand.h>
#include <sys/urm.h>

void kmain_thread(void *arg) {
    (void)arg;

    /* Launch the urm */
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(0, userspace_request_monitor, 0));

    /* Initialise PCI */
    init_pci();

    /* Initialise file descriptor handlers */
    init_fd();

    /* Initialise filesystem drivers */
    init_fs();

    /* Mount /dev */
    mount("devfs", "/dev", "devfs", 0, 0);

    /* Initialise device drivers */
    init_dev();

    int tty = open("/dev/tty0", O_RDWR);

    char *root = cmdline_get_value("root");
    char new_root[64];
    if (!root) {
        kprint(KPRN_WARN, "kmain: Command line argument \"root\" not specified.");
        readline(tty, "Select root device: ", new_root, 64);
        root = new_root;
    } else {
        strcpy(new_root, root);
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
        strcpy(new_rootfs, rootfs);
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
        strcpy(new_init, init);
        init = new_init;
    }
    kprint(KPRN_INFO, "kmain: init=%s", new_init);

    close(tty);

    /* Mount root partition */
    if (mount(root, "/", rootfs, 0, 0)) {
        panic("Unable to mount root", 0, 0, NULL);
    }

    /* Execute init process */
    kprint(KPRN_INFO, "kmain: Starting init");
    const char *args[] = { init, NULL };
    const char *environ[] = { NULL };
    if (kexec(init, args, environ, "/dev/tty0", "/dev/tty0", "/dev/tty0") == -1) {
        panic("Unable to launch init", 0, 0, NULL);
    }
    if (kexec(init, args, environ, "/dev/tty1", "/dev/tty1", "/dev/tty1") == -1) {
        panic("Unable to launch init", 0, 0, NULL);
    }
    if (kexec(init, args, environ, "/dev/tty2", "/dev/tty2", "/dev/tty2") == -1) {
        panic("Unable to launch init", 0, 0, NULL);
    }
    if (kexec(init, args, environ, "/dev/tty3", "/dev/tty3", "/dev/tty3") == -1) {
        panic("Unable to launch init", 0, 0, NULL);
    }
    if (kexec(init, args, environ, "/dev/tty4", "/dev/tty4", "/dev/tty4") == -1) {
        panic("Unable to launch init", 0, 0, NULL);
    }
    if (kexec(init, args, environ, "/dev/tty5", "/dev/tty5", "/dev/tty5") == -1) {
        panic("Unable to launch init", 0, 0, NULL);
    }

    kprint(KPRN_INFO, "kmain: End of kmain");

    /* kill kmain now */
    task_tkill(CURRENT_PROCESS, CURRENT_THREAD);

    for (;;) asm volatile ("hlt;");
}

/* Main kernel entry point, only initialise essential services and scheduler */
void kmain(void) {
    kprint(KPRN_INFO, "Kernel booted");
    kprint(KPRN_INFO, "Build time: %s", BUILD_TIME);
    kprint(KPRN_INFO, "Command line: %s", cmdline);

    init_idt();

    /* Memory-related stuff */
    init_e820();
    init_pmm();
    init_rand();
    init_vmm();

    dump_vga_font(vga_font);
    init_vbe();
    init_tty();

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

    init_pit();

    /* Enable interrupts on BSP */
    asm volatile ("sti");

    // ACPI specification section 5.8.1 - we are using the APIC,
    // so we need to be using mode 1 with the _PIC method.

    // This function enables the use of lai functions inside qword
    #ifdef _ACPI_
      lai_enable_acpi(1);
    #endif

    init_smp();

    /* Initialise scheduler */
    init_sched();

    /* Unlock the scheduler for the first time */
    spinlock_release(&scheduler_lock);

    /* Start a main kernel thread which will take over when the scheduler is running */
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(0, kmain_thread, 0));

    /*** DO NOT ADD ANYTHING TO THIS FUNCTION AFTER THIS POINT, ADD TO kmain_thread
         INSTEAD! ***/

    /* Pre-scheduler init done. Wait for the main kernel thread to be scheduled. */
    for (;;) asm volatile ("hlt");
}
