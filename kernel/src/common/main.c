#include <klib.h>
#include <serial.h>
#include <vga_textmode.h>
#include <e820.h>
#include <mm.h>
#include <idt.h>
#include <pic8259.h>

/* Main kernel entry point, all the things should be initialised */
int kmain(int argc, char *argv[]) {
    init_com1();
    init_vga_textmode();

    kprint(KPRN_INFO, "Kernel booted");
    kprint(KPRN_INFO, "Build time: %s", BUILD_TIME);
 
    /* Memory-related stuff */ 
    init_e820();
    init_pmm();
    full_identity_map();

    init_idt();
    init_pic8259();

    kprint(KPRN_INFO, "Allocating physical memory...");

    for (int i = 0; i < 5; i++)
        #ifdef __I386__
            kprint(KPRN_INFO, "page start address: %x", pmm_alloc(1));
        #endif
        #ifdef __X86_64__
            kprint(KPRN_INFO, "page start address: %X", pmm_alloc(1));
        #endif
    for (int i = 0; i < 5; i++)
        #ifdef __I386__
            kprint(KPRN_INFO, "page start address: %x", kalloc(1));
        #endif
        #ifdef __X86_64__
            kprint(KPRN_INFO, "page start address: %X", kalloc(1));
        #endif

    asm volatile ("sti;");

    for (;;);

    return 0;
}
