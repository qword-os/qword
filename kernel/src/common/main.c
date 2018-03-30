#include <vga_textmode.h>

int kmain(int argc, char *argv[]) {
    init_vga_textmode();
    text_putstring("\e[5;5Hhello, C world");
    for (;;);
    return 0;
}
