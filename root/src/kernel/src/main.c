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

void kmain_thread(void) {
    /* Execute a test process */
    kexec("/bin/test", 0, 0);

    kprint(KPRN_INFO, "kmain: End of init.");

    for (;;) asm volatile ("hlt;");
}

/* Main kernel entry point, all the things should be initialised */
void kmain(void) {
    init_idt();

    init_com1();
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
    init_acpi();
    init_pic();

    /* Enable interrupts on BSP */
    asm volatile ("sti");

    init_pit();
    init_smp();

    /* Initialise device drivers */
    init_ata();
    init_pci();
    init_ahci();

    /* Initialise Virtual Filesystem */
    init_vfs();

    /* Initialise filesystem drivers */
    init_devfs();
    init_echfs();

    /* Mount /dev */
    mount("devfs", "/dev", "devfs", 0, 0);

    /* Mount /dev/hda on / */
    mount("/dev/hda", "/", "echfs", 0, 0);

    /* Initialise scheduler */
    init_sched();

    /* Start a main kernel thread which will take over when the scheduler is running */
    task_tcreate(0, kalloc(4096) + 4096, (void *)kmain_thread, 0);

    /* Unlock the scheduler for the first time */
    spinlock_release(&scheduler_lock);

    /* Pre-scheduler init done. Wait for the main kernel thread to be scheduled. */
    for (;;) asm volatile ("hlt");
}
