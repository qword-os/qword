#include <vga_textmode.h>

int kmain(int argc, char *argv[]) {
    text_clear();
    text_putstring("\e[5;5Hhello, C world");
    for (;;);
    return 0;
}
