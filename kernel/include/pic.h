#ifndef __PIC_H__
#define __PIC_H__

#include <pic_8259.h>
#include <apic.h>

void send_eoi(uint8_t);
void init_pic(void);

#endif
