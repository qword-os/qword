#ifndef __PANIC_H__
#define __PANIC_H__

#include <stdint.h>
#include <stddef.h>

void panic(const char *, uint64_t, uint64_t);
void kexcept(const char *, size_t, size_t, size_t, size_t);

#endif
