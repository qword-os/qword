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
    pmm_init();
    
    kprint(KPRN_INFO, "Allocating physical memory...");

    void *page = kalloc(2);

    kprint(KPRN_INFO, "page start address: %X", (uint64_t *)page);
    
    for(;;);

    return 0;
}
