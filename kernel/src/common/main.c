#include <klib.h>
#include <serial.h>
#include <tty.h>
#include <vbe.h>
#include <e820.h>
#include <mm.h>
#include <idt.h>
#include <pic.h>
#include <acpi.h>
#include <cmdline.h>

/* Main kernel entry point, all the things should be initialised */
int kmain(void) {
    init_idt();

    init_com1();
    init_tty();

    kprint(KPRN_INFO, "Kernel booted");
    kprint(KPRN_INFO, "Build time: %s", BUILD_TIME);
    kprint(KPRN_INFO, "Command line: %s", cmdline);

    init_vbe();

    /* Memory-related stuff */
    init_e820();
    init_pmm();
    init_vmm();

    init_acpi();

    init_pic();
 
    for (;;)
        asm volatile ("hlt;");

    return 0;
}
