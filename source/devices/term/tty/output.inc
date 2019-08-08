#include <stdint.h>
#include <lib/klib.h>
#include <devices/term/tty/tty.h>
#include <sys/panic.h>
#include <lib/lock.h>
#include <lib/alloc.h>
#include <lib/bit.h>

// Tries to implement this standard for terminfo
// http://man7.org/linux/man-pages/man4/console_codes.4.html

static int rows;
static int cols;

static uint32_t *fb;
static int fb_height;
static int fb_width;
static int fb_pitch;

static uint8_t *font;
static int font_height;
static int font_width;

static void put_char(int, char);

static void plot_px(int x, int y, uint32_t hex) {
    size_t fb_i = x + (fb_pitch / sizeof(uint32_t)) * y;

    fb[fb_i] = hex;

    return;
}

static void plot_char(char c, int x, int y, uint32_t hex_fg, uint32_t hex_bg) {
    int orig_x = x;
    uint8_t *glyph = &font[c * font_height];

    for (int i = 0; i < font_height; i++) {
        for (int j = font_width - 1; j >= 0; j--)
            plot_px(x++, y, bit_test(glyph[i], j) ? hex_fg : hex_bg);
        y++;
        x = orig_x;
    }

    return;
}

static void plot_char_grid(int tty, char c, int x, int y, uint32_t hex_fg, uint32_t hex_bg) {
    if (ttys[tty].grid[x + y * cols] != c
     || ttys[tty].gridfg[x + y * cols] != hex_fg
     || ttys[tty].gridbg[x + y * cols] != hex_bg) {
        if (tty == current_tty)
            plot_char(c, x * font_width, y * font_height, hex_fg, hex_bg);
        ttys[tty].grid[x + y * cols] = c;
        ttys[tty].gridfg[x + y * cols] = hex_fg;
        ttys[tty].gridbg[x + y * cols] = hex_bg;
    }
    return;
}

static void clear_cursor(int tty) {
    if (ttys[tty].cursor_status) {
        if (tty == current_tty) {
            plot_char(ttys[tty].grid[ttys[tty].cursor_x + ttys[tty].cursor_y * cols],
                ttys[tty].cursor_x * font_width, ttys[tty].cursor_y * font_height,
                ttys[tty].gridfg[ttys[tty].cursor_x + ttys[tty].cursor_y * cols],
                ttys[tty].gridbg[ttys[tty].cursor_x + ttys[tty].cursor_y * cols]);
        }
    }
    return;
}

static void draw_cursor(int tty) {
    if (ttys[tty].cursor_status) {
        if (tty == current_tty) {
            plot_char(ttys[tty].grid[ttys[tty].cursor_x + ttys[tty].cursor_y * cols],
                ttys[tty].cursor_x * font_width, ttys[tty].cursor_y * font_height,
                ttys[tty].cursor_fg_col, ttys[tty].cursor_bg_col);
        }
    }
    return;
}

static void refresh(int tty) {
    if (tty == current_tty) {
        for (int i = 0; i < rows * cols; i++)
            plot_char(ttys[tty].grid[i],
                (i % cols) * font_width,
                (i / cols) * font_height,
                ttys[tty].gridfg[i],
                ttys[tty].gridbg[i]);
        draw_cursor(tty);
    }

    return;
}

static void scroll(int tty) {
    clear_cursor(tty);

    for (int i = cols; i < rows * cols; i++) {
        plot_char_grid(tty,
            ttys[tty].grid[i],
            (i - cols) % cols,
            (i - cols) / cols,
            ttys[tty].gridfg[i],
            ttys[tty].gridbg[i]);
    }
    /* clear the last line of the screen */
    for (int i = rows * cols - cols; i < rows * cols; i++) {
        plot_char_grid(tty,
            ' ',
            i % cols,
            i / cols,
            ttys[tty].text_fg_col,
            ttys[tty].text_bg_col);
    }

    draw_cursor(tty);

    return;
}

static void clear(int tty) {
    clear_cursor(tty);

    for (int i = 0; i < rows * cols; i++) {
        plot_char_grid(tty,
            ' ',
            i % cols,
            i / cols,
            ttys[tty].text_fg_col,
            ttys[tty].text_bg_col);
    }

    ttys[tty].cursor_x = 0;
    ttys[tty].cursor_y = 0;

    draw_cursor(tty);
    return;
}

static void enable_cursor(int tty) {
    ttys[tty].cursor_status = 1;
    draw_cursor(tty);
    return;
}

static void disable_cursor(int tty) {
    clear_cursor(tty);
    ttys[tty].cursor_status = 0;
    return;
}

static void set_cursor_pos(int tty, int x, int y) {
    clear_cursor(tty);
    ttys[tty].cursor_x = x;
    ttys[tty].cursor_y = y;
    draw_cursor(tty);
    return;
}

static void sgr(int tty) {
    if (!ttys[tty].esc_values_i)
        goto def;

    for (int i = 0; i < ttys[tty].esc_values_i; i++) {

        if (!ttys[tty].esc_values[i]) {
def:
            ttys[tty].text_fg_col = ttys[tty].default_fg_col;
            ttys[tty].text_bg_col = ttys[tty].default_bg_col;
            continue;
        }

        if (ttys[tty].esc_values[i] >= 30 && ttys[tty].esc_values[i] <= 37) {
            ttys[tty].text_fg_col = ansi_colours[ttys[tty].esc_values[i] - 30];
            continue;
        }

        if (ttys[tty].esc_values[i] >= 40 && ttys[tty].esc_values[i] <= 47) {
            ttys[tty].text_bg_col = ansi_colours[ttys[tty].esc_values[i] - 40];
            continue;
        }

    }

    return;
}

static void control_sequence_parse(int tty, char c) {
    if (c >= '0' && c <= '9') {
        ttys[tty].rrr = 1;
        ttys[tty].esc_values[ttys[tty].esc_values_i] *= 10;
        ttys[tty].esc_values[ttys[tty].esc_values_i] += c - '0';
        return;
    } else {
        if (ttys[tty].rrr) {
            ttys[tty].esc_values_i++;
            ttys[tty].rrr = 0;
            if (c == ';')
                return;
        } else if (c == ';') {
            ttys[tty].esc_values[ttys[tty].esc_values_i] = 1;
            ttys[tty].esc_values_i++;
            return;
        }
    }

    // default rest to 1
    for (int i = ttys[tty].esc_values_i; i < MAX_ESC_VALUES; i++)
        ttys[tty].esc_values[i] = 1;

    switch (c) {
        case '@':
            // TODO
            break;
        case 'A':
            if (ttys[tty].esc_values[0] > ttys[tty].cursor_y)
                ttys[tty].esc_values[0] = ttys[tty].cursor_y;
            set_cursor_pos(tty, ttys[tty].cursor_x, ttys[tty].cursor_y - ttys[tty].esc_values[0]);
            break;
        case 'B':
            if ((ttys[tty].cursor_y + ttys[tty].esc_values[0]) > (rows - 1))
                ttys[tty].esc_values[0] = (rows - 1) - ttys[tty].cursor_y;
            set_cursor_pos(tty, ttys[tty].cursor_x, ttys[tty].cursor_y + ttys[tty].esc_values[0]);
            break;
        case 'C':
            if ((ttys[tty].cursor_x + ttys[tty].esc_values[0]) > (cols - 1))
                ttys[tty].esc_values[0] = (cols - 1) - ttys[tty].cursor_x;
            set_cursor_pos(tty, ttys[tty].cursor_x + ttys[tty].esc_values[0], ttys[tty].cursor_y);
            break;
        case 'D':
            if (ttys[tty].esc_values[0] > ttys[tty].cursor_x)
                ttys[tty].esc_values[0] = ttys[tty].cursor_x;
            set_cursor_pos(tty, ttys[tty].cursor_x - ttys[tty].esc_values[0], ttys[tty].cursor_y);
            break;
        case 'E':
            if (ttys[tty].cursor_y + ttys[tty].esc_values[0] >= rows)
                set_cursor_pos(tty, 0, rows - 1);
            else
                set_cursor_pos(tty, 0, ttys[tty].cursor_y + ttys[tty].esc_values[0]);
            break;
        case 'F':
            if (ttys[tty].cursor_y - ttys[tty].esc_values[0] < 0)
                set_cursor_pos(tty, 0, 0);
            else
                set_cursor_pos(tty, 0, ttys[tty].cursor_y - ttys[tty].esc_values[0]);
            break;
        case 'd':
            if (ttys[tty].esc_values[0] >= rows)
                break;
            clear_cursor(tty);
            ttys[tty].cursor_y = ttys[tty].esc_values[0];
            draw_cursor(tty);
            break;
        case 'G':
        case '`':
            if (ttys[tty].esc_values[0] >= cols)
                break;
            clear_cursor(tty);
            ttys[tty].cursor_x = ttys[tty].esc_values[0];
            draw_cursor(tty);
            break;
        case 'H':
        case 'f':
            ttys[tty].esc_values[0] -= 1;
            ttys[tty].esc_values[1] -= 1;
            if (ttys[tty].esc_values[1] >= cols)
                ttys[tty].esc_values[1] = cols - 1;
            if (ttys[tty].esc_values[0] >= rows)
                ttys[tty].esc_values[0] = rows - 1;
            set_cursor_pos(tty, ttys[tty].esc_values[1], ttys[tty].esc_values[0]);
            break;
        case 'J':
            switch (ttys[tty].esc_values[0]) {
                case 1: {
                    int cursor_abs = ttys[tty].cursor_y * cols + ttys[tty].cursor_x;
                    clear_cursor(tty);

                    for (int i = 0; i < cursor_abs; i++) {
                        plot_char_grid(tty,
                            ' ',
                            i % cols,
                            i / cols,
                            ttys[tty].text_fg_col,
                            ttys[tty].text_bg_col);
                    }

                    draw_cursor(tty);
                    }
                    break;
                case 2:
                    clear(tty);
                    break;
                default:
                    break;
            }
            break;
        case 'm':
            sgr(tty);
            break;
        case 'r':
            ttys[tty].scrolling_region_top = ttys[tty].esc_values[0];
            ttys[tty].scrolling_region_bottom = ttys[tty].esc_values[1];
            break;
        case 's':
            ttys[tty].saved_cursor_x = ttys[tty].cursor_x;
            ttys[tty].saved_cursor_y = ttys[tty].cursor_y;
            break;
        case 'u':
            clear_cursor(tty);
            ttys[tty].cursor_x = ttys[tty].saved_cursor_x;
            ttys[tty].cursor_y = ttys[tty].saved_cursor_y;
            draw_cursor(tty);
            break;
        case 'h':
        case 'l':
            if (ttys[tty].dec_private_mode) {
                ttys[tty].dec_private_mode = 0;
                switch (ttys[tty].esc_values[1]) {
                    case 1:
                        ttys[tty].decckm = (c == 'h');
                        break;
                    default:
                        kprint(KPRN_WARN, "tty: unknown DEC sequence '%d'", (int)ttys[tty].esc_values[1]);
                        break;
                }
            }
            break;
        case '?':
            ttys[tty].dec_private_mode = 1;
            return;
        default:
            kprint(KPRN_WARN, "tty: unknown CSI sequence '%c'", c);
            break;
    }

    ttys[tty].control_sequence = 0;
    ttys[tty].escape = 0;

    return;
}

static void escape_parse(int tty, char c) {
    if (ttys[tty].control_sequence) {
        control_sequence_parse(tty, c);
        return;
    }
    switch (c) {
        case '[':
            for (int i = 0; i < MAX_ESC_VALUES; i++)
                ttys[tty].esc_values[i] = 0;
            ttys[tty].esc_values_i = 0;
            ttys[tty].rrr = 0;
            ttys[tty].control_sequence = 1;
            break;
        default:
            ttys[tty].escape = 0;
            break;
    }

    return;
}

int tty_write(int tty, const void *void_buf, uint64_t unused, size_t count) {
    (void)unused;
    if (!tty_ready)
        return 0;
    if (ttys[tty].tcooff) {
        errno = EINVAL;
        return -1;
    }

    const char *buf = void_buf;
    spinlock_acquire(&ttys[tty].write_lock);
    for (size_t i = 0; i < count; i++)
        put_char(tty, buf[i]);
    spinlock_release(&ttys[tty].write_lock);
    return (int)count;
}

static void put_char(int tty, char c) {
    if (ttys[tty].escape) {
        escape_parse(tty, c);
        return;
    }
    switch (c) {
        case '\0':
            break;
        case '\e':
            ttys[tty].escape = 1;
            break;
        case '\t':
            if ((ttys[tty].cursor_x / ttys[tty].tabsize + 1) * ttys[tty].tabsize >= cols)
                break;
            set_cursor_pos(tty, (ttys[tty].cursor_x / ttys[tty].tabsize + 1) * ttys[tty].tabsize, ttys[tty].cursor_y);
            break;
        case '\r':
            set_cursor_pos(tty, 0, ttys[tty].cursor_y);
            break;
        case '\a':
            // dummy handler for bell
            break;
        case '\n':
            if (ttys[tty].cursor_y == (rows - 1)) {
                set_cursor_pos(tty, 0, (rows - 1));
                scroll(tty);
            } else {
                set_cursor_pos(tty, 0, (ttys[tty].cursor_y + 1));
            }
            break;
        case '\b':
            if (ttys[tty].cursor_x || ttys[tty].cursor_y) {
                clear_cursor(tty);
                if (ttys[tty].cursor_x) {
                    ttys[tty].cursor_x--;
                } else {
                    ttys[tty].cursor_y--;
                    ttys[tty].cursor_x = cols - 1;
                }
                draw_cursor(tty);
            }
            break;
        default:
            clear_cursor(tty);
            plot_char_grid(tty, c, ttys[tty].cursor_x++, ttys[tty].cursor_y, ttys[tty].text_fg_col, ttys[tty].text_bg_col);
            if (ttys[tty].cursor_x == cols) {
                ttys[tty].cursor_x = 0;
                ttys[tty].cursor_y++;
            }
            if (ttys[tty].cursor_y == rows) {
                ttys[tty].cursor_y--;
                scroll(tty);
            }
            draw_cursor(tty);
            break;
    }

    return;
}
