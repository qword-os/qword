#include <klib.h>
#include <serial.h>
#include <vga_textmode.h>
#include <e820.h>
#include <mm.h>

int kmain(int argc, char *argv[]) {
    init_com1();
    init_vga_textmode();
    kprint(KPRN_INFO, "Kernel booted");
    kprint(KPRN_INFO, "Build time: %s", BUILD_TIME);
    init_e820();
    init_pmm();
    full_identity_map();

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
    
    uint64_t *i = 0x0;

    for (;;);

    return 0;
}
