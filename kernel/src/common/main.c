#include <vga_textmode.h>
#include <e820.h>

int kmain(int argc, char *argv[]) {
    init_vga_textmode();
    text_putstring("hello, C world\n");
    init_e820();
    for (;;);
    return 0;
}
