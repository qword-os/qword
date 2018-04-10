#ifndef __PIC_H__
#define __PIC_H__

#include <stdint.h>

void wait(void);
void remap_pic(uint8_t, uint8_t);
void clear_mask(uint8_t);

#endif
