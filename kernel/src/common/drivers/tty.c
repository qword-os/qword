#include <tty.h>
#include <klib.h>
#include <vga_textmode.h>
#include <cmdline.h>

static int use_vbe = 0;

void init_tty(void) {
    char *tty_cmdline;

    if ((tty_cmdline = cmdline_get_value("display"))) {
        if (!kstrcmp(tty_cmdline, "vga")) {
            init_vga_textmode();
            use_vbe = 0;
        } else if (!kstrcmp(tty_cmdline, "vbe")) {
            goto default_setting;
        } else {
            for (;;);
        }
    } else {
default_setting:
       //init_vbe_textmode();
       use_vbe = 1;
    }

    return;
}

void tty_putchar(char c) {
    if (use_vbe)
        ;//vbe_putchar(c);
    else
        text_putchar(c);

    return;
}
