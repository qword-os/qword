#ifndef __KLIB_H__
#define __KLIB_H__

#include <stdint.h>
#include <stddef.h>

size_t kstrlen(const char *);
int kstrcmp(const char *, const char *);
int kstrncmp(const char *, const char *, size_t);
void kputs(const char *);

#endif
