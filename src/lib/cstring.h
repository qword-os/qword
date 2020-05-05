#ifndef __LIB__CSTRING_H__
#define __LIB__CSTRING_H__

#include <stddef.h>

char *strchrnul(const char *s, int c);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *str);

#endif
