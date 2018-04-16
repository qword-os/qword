#include <tty.h>
#include <klib.h>
#include <vbe.h>
#include <vga_textmode.h>
#include <vbe_tty.h>
#include <cmdline.h>

static int use_vbe = 0;

void init_tty(void) {
    char *tty_cmdline;

    if ((tty_cmdline = cmdline_get_value("display"))) {
        if (!kstrcmp(tty_cmdline, "vga")) {
            use_vbe = 0;
        } else if (!kstrcmp(tty_cmdline, "vbe")) {
            use_vbe = 1;
        } else {
            for (;;);
        }
    } else {
        use_vbe = 1;
    }

    return;
}

void tty_putchar(char c) {
    if (use_vbe) {
        if (!vbe_available || !vbe_tty_available) {
            text_putchar(c);
        } else {
            vbe_tty_putchar(c);
        }
    } else {
        text_putchar(c);
    }

    return;
}
