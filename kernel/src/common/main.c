#include <klib.h>
#include <vga_textmode.h>
#include <e820.h>

int kmain(int argc, char *argv[]) {
    init_vga_textmode();
    kprint(KPRN_INFO, "Kernel booted");
    kprint(KPRN_INFO, "Build time: %s", BUILD_TIME);
    init_e820();
    for (;;);
    return 0;
}
