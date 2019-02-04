#ifndef __VGA_FONT_H__
#define __VGA_FONT_H__

#include <stdint.h>

extern uint8_t vga_font[16 * 256];
void dump_vga_font(uint8_t *);
#define vga_font_width 8
#define vga_font_height 16

#endif
