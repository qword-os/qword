#include <stdint.h>
#include <vbe_tty.h>
#include <klib.h>
#include <tty.h>
#include <vbe.h>
#include <panic.h>

int vbe_tty_available = 0;

static int cursor_x = 0;
static int cursor_y = 0;
static int cursor_status = 1;
static uint32_t cursor_bg_col = 0x00ffffff;
static uint32_t cursor_fg_col = 0x00000000;
static uint32_t text_bg_col = 0x00000000;
static uint32_t text_fg_col = 0x00aaaaaa;
static char *grid;
static uint32_t *gridbg;
static uint32_t *gridfg;
static int escape = 0;
static int esc_value0 = 0;
static int esc_value1 = 0;
static int *esc_value = &esc_value0;
static int esc_default0 = 1;
static int esc_default1 = 1;
static int *esc_default = &esc_default0;
static int raw = 0;
static int noblock = 0;
static int noscroll = 0;

static int rows;
static int cols;

uint8_t vga_font[16 * 256];

static void plot_px(int x, int y, uint32_t hex) {
    size_t fb_i = x + vbe_width * y;

    vbe_framebuffer[fb_i] = hex;

    return;
}

static void plot_char(char c, int x, int y, uint32_t hex_fg, uint32_t hex_bg) {
    int orig_x = x;

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 8; j++) {
            if ((vga_font[c * 16 + i] >> (7 - j)) & 1)
                plot_px(x++, y, hex_fg);
            else
                plot_px(x++, y, hex_bg);
        }
        y++;
        x = orig_x;
    }

    return;
}

static void plot_char_grid(char c, int x, int y, uint32_t hex_fg, uint32_t hex_bg) {
    plot_char(c, x * 8, y * 16, hex_fg, hex_bg);
    grid[x + y * cols] = c;
    gridfg[x + y * cols] = hex_fg;
    gridbg[x + y * cols] = hex_bg;
    return;
}

static void clear_cursor(void) {
    plot_char(grid[cursor_x + cursor_y * cols],
        cursor_x * 8, cursor_y * 16,
        text_fg_col, text_bg_col);
    return;
}

static void draw_cursor(void) {
    if (cursor_status)
        plot_char(grid[cursor_x + cursor_y * cols],
            cursor_x * 8, cursor_y * 16,
            cursor_fg_col, cursor_bg_col);
    return;
}

void vbe_tty_refresh(void) {
    /* interpret the grid and print the chars */
    for (size_t i = 0; i < (size_t)(rows * cols); i++) {
        plot_char_grid(grid[i], i % cols, i / cols, gridfg[i], gridbg[i]);
    }
    draw_cursor();
    return;
}

static void scroll(void) {
    /* notify grid */
    for (size_t i = cols; i < (size_t)(rows * cols); i++) {
        grid[i - cols] = grid[i];
        gridbg[i - cols] = gridbg[i];
        gridfg[i - cols] = gridfg[i];
    }
    /* clear the last line of the screen */
    for (size_t i = rows * cols - cols; i < (size_t)(rows * cols); i++) {
        grid[i] = ' ';
        gridbg[i] = text_bg_col;
        gridfg[i] = text_fg_col;
    }

    vbe_tty_refresh();
    return;
}

void vbe_tty_clear(void) {
    for (size_t i = 0; i < (size_t)(rows * cols); i++) {
        grid[i] = ' ';
        gridbg[i] = text_bg_col;
        gridfg[i] = text_fg_col;
    }

    cursor_x = 0;
    cursor_y = 0;

    vbe_tty_refresh();
    return;
}

static void vbe_tty_clear_no_move(void) {
    for (size_t i = 0; i < (size_t)(rows * cols); i++) {
        grid[i] = ' ';
        gridbg[i] = text_bg_col;
        gridfg[i] = text_fg_col;
    }

    vbe_tty_refresh();
    return;
}

void vbe_tty_enable_cursor(void) {
    cursor_status = 1;
    draw_cursor();
    return;
}

void vbe_tty_disable_cursor(void) {
    cursor_status = 0;
    clear_cursor();
    return;
}

static uint32_t ansi_colours[] = {
    0x00000000,              /* black */
    0x00aa0000,              /* red */
    0x0000aa00,              /* green */
    0x00aa5500,              /* brown */
    0x000000aa,              /* blue */
    0x00aa00aa,              /* magenta */
    0x0000aaaa,              /* cyan */
    0x00aaaaaa,              /* grey */
};

static void sgr(void) {

    if (esc_value0 >= 30 && esc_value0 <= 37) {
        text_fg_col = ansi_colours[esc_value0 - 30];
        return;
    }

    if (esc_value0 >= 40 && esc_value0 <= 47) {
        text_bg_col = ansi_colours[esc_value0 - 40];
        return;
    }

    return;
}

static void escape_parse(char c) {
    
    if (c >= '0' && c <= '9') {
        *esc_value *= 10;
        *esc_value += c - '0';
        *esc_default = 0;
        return;
    }

    switch (c) {
        case '[':
            return;
        case ';':
            esc_value = &esc_value1;
            esc_default = &esc_default1;
            return;
        case 'A':
            if (esc_default0)
                esc_value0 = 1;
            if (esc_value0 > cursor_y)
                esc_value0 = cursor_y;
            vbe_tty_set_cursor_pos(cursor_x, cursor_y - esc_value0);
            break;
        case 'B':
            if (esc_default0)
                esc_value0 = 1;
            if ((cursor_y + esc_value0) > (rows - 1))
                esc_value0 = (rows - 1) - cursor_y;
            vbe_tty_set_cursor_pos(cursor_x, cursor_y + esc_value0);
            break;
        case 'C':
            if (esc_default0)
                esc_value0 = 1;
            if ((cursor_x + esc_value0) > (cols - 1))
                esc_value0 = (cols - 1) - cursor_x;
            vbe_tty_set_cursor_pos(cursor_x + esc_value0, cursor_y);
            break;
        case 'D':
            if (esc_default0)
                esc_value0 = 1;
            if (esc_value0 > cursor_x)
                esc_value0 = cursor_x;
            vbe_tty_set_cursor_pos(cursor_x - esc_value0, cursor_y);
            break;
        case 'H':
            esc_value0 -= 1;
            esc_value1 -= 1;
            if (esc_default0)
                esc_value0 = 0;
            if (esc_default1)
                esc_value1 = 0;
            if (esc_value1 >= cols)
                esc_value1 = cols - 1;
            if (esc_value0 >= rows)
                esc_value0 = rows - 1;
            vbe_tty_set_cursor_pos(esc_value1, esc_value0);
            break;
        case 'm':
            sgr();
            break;
        case 'J':
            switch (esc_value0) {
                case 2:
                    vbe_tty_clear_no_move();
                    break;
                default:
                    break;
            }
            break;
        /* non-standard sequences */
        case 'r': /* enter/exit raw mode */
            raw = !raw;
            break;
        case 'b': /* enter/exit non-blocking mode */
            noblock = !noblock;
            break;
        case 's': /* enter/exit non-scrolling mode */
            noscroll = !noscroll;
            break;
        /* end non-standard sequences */
        default:
            escape = 0;
            vbe_tty_putchar('?');
            break;
    }

    esc_value = &esc_value0;
    esc_value0 = 0;
    esc_value1 = 0;
    esc_default = &esc_default0;
    esc_default0 = 1;
    esc_default1 = 1;
    escape = 0;

    return;
}

void vbe_tty_putchar(char c) {
    if (!vbe_tty_available || !vbe_available)
        return;

    if (escape) {
        escape_parse(c);
        return;
    }
    switch (c) {
        case 0x00:
            break;
        case 0x1B:
            escape = 1;
            break;
        case 0x0A:
            if (cursor_y == (rows - 1)) {
                vbe_tty_set_cursor_pos(0, (rows - 1));
                scroll();
            } else {
                vbe_tty_set_cursor_pos(0, (cursor_y + 1));
            }
            break;
        case 0x08:
            if (cursor_x || cursor_y) {
                clear_cursor();
                if (cursor_x) {
                    cursor_x--;
                } else {
                    cursor_y--;
                    cursor_x = cols - 1;
                }
                draw_cursor();
            }
            break;
        default:
            plot_char_grid(c, cursor_x++, cursor_y, text_fg_col, text_bg_col);
            if (cursor_x == cols) {
                cursor_x = 0;
                cursor_y++;
            }
            if (cursor_y == rows) {
                cursor_y--;
                if (!noscroll)
                    scroll();
            }
            draw_cursor();
    }
    return;
}

void vbe_tty_set_cursor_pos(int x, int y) {
    clear_cursor();
    cursor_x = x;
    cursor_y = y;
    draw_cursor();
    return;
}

void init_vbe_tty(void) {
    if (!vbe_available)
        return;

    kprint(KPRN_INFO, "vbe_tty: Initialising...");

    cols = vbe_width / 8;
    rows = vbe_height / 16;

    dump_vga_font(vga_font);

    grid = kalloc(rows * cols);
    gridbg = kalloc(rows * cols * sizeof(uint32_t));
    gridfg = kalloc(rows * cols * sizeof(uint32_t));
    if (!grid || !gridbg || !gridfg)
        panic("Out of memory in init_vbe_tty()", 0, 0);

    for (size_t i = 0; i < (size_t)(rows * cols); i++) {
        grid[i] = ' ';
        gridbg[i] = text_bg_col;
        gridfg[i] = text_fg_col;
    }

    vbe_tty_available = 1;

    vbe_tty_refresh();
    kprint(KPRN_INFO, "vbe_tty: Ready!");
    return;
}
