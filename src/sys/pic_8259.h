#ifndef __PIC_8259_H__
#define __PIC_8259_H__

#include <stdint.h>

void pic_8259_eoi(uint8_t);
void pic_8259_remap(uint8_t, uint8_t);
void pic_8259_set_mask(uint8_t, int);
void pic_8259_mask_all(void);

#endif
