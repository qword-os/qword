#include <klib.h>
#include <serial.h>
#include <tty.h>
#include <vga_textmode.h>
#include <vbe_tty.h>
#include <vbe.h>
#include <e820.h>
#include <mm.h>
#include <idt.h>
#include <gdt.h>
#include <pic.h>
#include <acpi.h>
#include <cmdline.h>
#include <pit.h>
#include <smp.h>

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

    init_vbe();
    init_vbe_tty();

    /* No more BIOS calls below this point */

    init_gdt();

    init_acpi();

    init_pic();

    asm volatile ("sti");

    init_pit();

    init_smp();
 
    for (;;)
        asm volatile ("hlt;");

    return 0;
}
