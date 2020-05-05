#ifndef __IDT_H__
#define __IDT_H__

#include <lib/event.h>

extern event_t int_event[256];

void init_idt(void);
int get_empty_int_vector(void);

#endif
