#include <klib.h>
#include <vga_textmode.h>

int kstrcmp(const char *dst, const char *src) {
    size_t i;

    for (i = 0; dst[i] == src[i]; i++) {
        if ((!dst[i]) && (!src[i])) return 0;
    }

    return 1;
}

int kstrncmp(const char *dst, const char *src, size_t count) {
    size_t i;

    for (i = 0; i < count; i++)
        if (dst[i] != src[i]) return 1;

    return 0;
}

size_t kstrlen(const char *str) {
    size_t len;

    for (len = 0; str[len]; len++);

    return len;
}

void kputs(const char *string) {
    size_t i;
    
    for (i = 0; string[i]; i++) {
        text_putchar(string[i]);
   }
}
