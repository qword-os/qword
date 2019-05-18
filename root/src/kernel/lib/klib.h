#ifndef __KLIB_H__
#define __KLIB_H__

#include <stdint.h>
#include <stddef.h>
#include <proc/task.h>
#include <stdarg.h>

#define KPRN_MAX_TYPE 3

#define KPRN_INFO   0
#define KPRN_WARN   1
#define KPRN_ERR    2
#define KPRN_DBG    3
#define KPRN_PANIC  4

#define EMPTY ((void *)(size_t)(-1))

#define stringify(x) #x
#define expand_stringify(x) stringify(x)

__attribute__((always_inline)) inline int is_printable(char c) {
    return (c >= 0x20 && c <= 0x7e);
}

__attribute__((always_inline)) inline void atomic_fetch_add_int(int *p, int *v, int x) {
    int h = x;
    asm volatile (
        "lock xadd dword ptr [%1], %0;"
        : "+r" (h)
        : "r" (p)
        : "memory"
    );
    *v = h;
}

__attribute__((always_inline)) inline void atomic_add_uint64_relaxed(uint64_t *p, uint64_t x) {
    asm volatile (
        "lock xadd qword ptr [%1], %0;"
        : "+r" (x)
        : "r" (p)
    );
}

int exec(pid_t, const char *, const char **, const char **);

pid_t kexec(const char *, const char **, const char **,
            const char *, const char *, const char *);

char *prefixed_itoa(const char *, int64_t, int);
int tolower(int);
char *strchrnul(const char *, int);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
void kprint(int type, const char *fmt, ...);
void kvprint(int type, const char *fmt, va_list args);

void *memset(void *, int, size_t);
void *memset64(void *, uint64_t, size_t);
void *memcpy(void *, const void *, size_t);
void *memcpy64(void *, const void *, size_t);
int memcmp(const void *, const void *, size_t);
void *memmove(void *, const void *, size_t);

void readline(int, const char *, char *, size_t);

#endif
