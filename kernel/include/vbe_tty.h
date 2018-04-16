#ifndef __VBE_TTY_H__
#define __VBE_TTY_H__

#include <stdint.h>

extern int vbe_tty_available;

void init_vbe_tty(void);
void vbe_tty_putchar(char);
void vbe_tty_enable_cursor(void);
void vbe_tty_disable_cursor(void);
void vbe_tty_clear(void);
void vbe_tty_set_cursor_pos(int, int);
void vbe_tty_refresh(void);

extern uint8_t vga_font[16 * 256];
void dump_vga_font(uint8_t *);

#endif
