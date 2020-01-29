#ifndef __LIB__BITMAP_FONT_H__
#define __LIB__BITMAP_FONT_H__

#include <stdint.h>

#define bitmap_font_width  8
#define bitmap_font_height 16
#define bitmap_font_glyphs 256
#define bitmap_font_max    (bitmap_font_height * bitmap_font_glyphs)

extern uint8_t bitmap_font[bitmap_font_max];

#endif
