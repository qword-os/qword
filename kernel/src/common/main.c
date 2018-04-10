#include <klib.h>
#include <serial.h>
#include <vga_textmode.h>
#include <e820.h>
#include <mm.h>
#include <idt.h>
#include <pic8259.h>

/* Main kernel entry point, all the things should be initialised */
int kmain(int argc, char *argv[]) {
    init_idt();

    init_com1();
    init_vga_textmode();

    kprint(KPRN_INFO, "Kernel booted");
    kprint(KPRN_INFO, "Build time: %s", BUILD_TIME);

    /* Memory-related stuff */
    init_e820();
    init_pmm();
    init_vmm();

    kprint(KPRN_INFO, "Kernel bootup finished. Nothing to see here yet :^)");

    for (;;)
        asm volatile ("hlt;");

    return 0;
}
