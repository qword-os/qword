#ifndef __PIC_H__
#define __PIC_H__

#include <stdint.h>

void pic_send_eoi(uint8_t);
void pic_set_mask(int, int);
void init_pic(void);

#endif
