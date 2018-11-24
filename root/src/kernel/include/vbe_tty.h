#ifndef __VBE_TTY_H__
#define __VBE_TTY_H__

#include <stdint.h>
#include <stddef.h>

extern int vbe_tty_available;

void init_vbe_tty(void);
void vbe_tty_write(const char *, size_t);

extern uint8_t vga_font[16 * 256];
void dump_vga_font(uint8_t *);

#endif
