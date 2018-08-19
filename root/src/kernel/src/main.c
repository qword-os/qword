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

/* Main kernel entry point, all the things should be initialised */
int kmain(void) {
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

    /* Driver inits */
    init_vbe();
    init_vbe_tty();
    init_acpi();
    init_pic();
    
    /* TODO move this someplace else */
    asm volatile ("sti");

    init_pit();
    init_smp();

    /* /dev drivers init */
    init_ata();

    /* Initialise vfs */
    init_vfs();

    /* Initialise filesystem drivers */
    init_devfs();

    /* Mount /dev */
    mount("devfs", "/dev", "devfs", 0, 0);

    /* Initialise scheduler */
    init_sched();

    /* Try reading 128 bytes from /dev/hda */
    int hda = open("/dev/hda", O_RDWR, 0);
    kprint(KPRN_DBG, "\"/dev/hda\"'s handle is %u", hda);
    uint8_t data[128];
    read(hda, data, 128);

    for (size_t i = 0; i < 128; i+=16) {
        kprint(KPRN_DBG, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
            data[i], data[i+1], data[i+2], data[i+3], data[i+4], data[i+5], data[i+6], data[i+7],
            data[i+8], data[i+9], data[i+10], data[i+11], data[i+12], data[i+13], data[i+14], data[i+15]);
    }

    read(hda, data, 128);

    for (size_t i = 0; i < 128; i+=16) {
        kprint(KPRN_DBG, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
            data[i], data[i+1], data[i+2], data[i+3], data[i+4], data[i+5], data[i+6], data[i+7],
            data[i+8], data[i+9], data[i+10], data[i+11], data[i+12], data[i+13], data[i+14], data[i+15]);
    }
/*
    const char *hello = "hello world";
    device_write(hda, hello, 0x1d0, 11);
    device_flush(hda);
*/
    for (;;)
        asm volatile ("hlt;");

    return 0;
}
