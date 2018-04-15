#ifndef __PANIC_H__
#define __PANIC_H__

#include <stdint.h>

void panic(const char *, int, uint64_t);
void halt(void);

#endif
