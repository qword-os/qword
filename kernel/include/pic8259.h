#ifndef __PIC_H__
#define __PIC_H__

#include <stdint.h>

void init_pic8259(void);
void pic8259_eoi0(void);
void pic8259_eoi1(void);
void pic8259_remap(uint8_t, uint8_t);
void pic8259_set_mask(uint8_t, int);

#endif
