#ifndef __LIB__CMEM_H__
#define __LIB__CMEM_H__

#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memcpy64(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memset64(void *s, uint64_t c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

#endif
