#ifndef __VGA_FONT_H__
#define __VGA_FONT_H__

#include <stdint.h>

#define vga_font_width 8
#define vga_font_height 16
extern uint8_t vga_font[vga_font_height * 256];

#endif
