#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <lib/lock.h>
#include <lib/klib.h>
#include <lib/qemu.h>
#include <devices/term/tty/tty.h>
#include <mm/mm.h>
#include <lib/time.h>
#include <fd/vfs/vfs.h>
#include <lib/cstring.h>

static const char *base_digits = "0123456789abcdef";

char *prefixed_itoa(const char *prefix, int64_t n, int base) {
    int prefix_len = strlen(prefix);
    char *final_buf = kalloc(prefix_len + 1);
    strcpy(final_buf, prefix);
    const int buf_size = 64;
    char *buf = kalloc(buf_size);

    int i;
    if (!n) {
        buf[buf_size - 2] = '0';
        i = buf_size - 2;
    } else {
        int sign = n < 0;
        if (sign)
            n = -n;

        for (i = buf_size - 2; n; i--) {
            buf[i] = base_digits[n % base];
            n /= base;
        }
        if (sign)
            buf[i] = '-';
        else
            i++;
    }

    final_buf = krealloc(final_buf, prefix_len + strlen(buf + i) + 1);
    strcpy(final_buf + prefix_len, buf + i);

    kfree(buf);

    return final_buf;
}

int islower(int c) {
    return c >= 'a' && c <= 'z';
}

int tolower(int c) {
    if (c >= 0x41 && c <= 0x5a)
        return c + 0x20;
    return c;
}

int toupper(int c) {
    if (islower(c)) return c - ('a' - 'A');
    else return c;
}

void readline(int fd, const char *prompt, char *str, size_t max) {
    size_t i;
    write(fd, prompt, strlen(prompt));
    for (i = 0; i < (max - 1); i++) {
        read(fd, &str[i], 1);
        if (str[i] == '\n')
            break;
    }
    str[i] = 0;
    return;
}

#define KPRINT_BUF_MAX 256

static void kputs(char *kprint_buf, size_t *kprint_buf_i, const char *string) {
    size_t i;

    for (i = 0; string[i]; i++) {
        if (*kprint_buf_i == (KPRINT_BUF_MAX - 1))
            break;
        kprint_buf[(*kprint_buf_i)++] = string[i];
    }

    kprint_buf[*kprint_buf_i] = 0;

    return;
}

static void knputs(char *kprint_buf, size_t *kprint_buf_i, const char *string, size_t len) {
    size_t i;

    for (i = 0; i < len; i++) {
        if (*kprint_buf_i == (KPRINT_BUF_MAX - 1))
            break;
        kprint_buf[(*kprint_buf_i)++] = string[i];
    }

    kprint_buf[*kprint_buf_i] = 0;

    return;
}

static void kputchar(char *kprint_buf, size_t *kprint_buf_i, char c) {
    if (*kprint_buf_i < (KPRINT_BUF_MAX - 1)) {
        kprint_buf[(*kprint_buf_i)++] = c;
    }

    kprint_buf[*kprint_buf_i] = 0;

    return;
}

static void kprint_buf_flush(char *kprint_buf, size_t *kprint_buf_i) {
    #ifdef _DBGOUT_QEMU_
        qemu_debug_puts(kprint_buf);
    #endif
    #ifdef _DBGOUT_TTY_
        tty_write(0, kprint_buf, 0, *kprint_buf_i);
    #endif
    (void)kprint_buf;
    (void)kprint_buf_i;
    return;
}

static void kprint_buf_flush_urgent(char *kprint_buf, size_t *kprint_buf_i) {
    qemu_debug_puts_urgent(kprint_buf);
    tty_write(0, kprint_buf, 0, *kprint_buf_i);
    return;
}

static void kprn_i(char *kprint_buf, size_t *kprint_buf_i, int64_t x) {
    int i;
    char buf[21] = {0};

    if (!x) {
        kputchar(kprint_buf, kprint_buf_i, '0');
        return;
    }

    int sign = x < 0;
    if (sign) x = -x;

    for (i = 19; x; i--) {
        buf[i] = (x % 10) + 0x30;
        x = x / 10;
    }
    if (sign)
        buf[i] = '-';
    else
        i++;

    kputs(kprint_buf, kprint_buf_i, buf + i);

    return;
}

static void kprn_ui(char *kprint_buf, size_t *kprint_buf_i, uint64_t x) {
    int i;
    char buf[21] = {0};

    if (!x) {
        kputchar(kprint_buf, kprint_buf_i, '0');
        return;
    }

    for (i = 19; x; i--) {
        buf[i] = (x % 10) + 0x30;
        x = x / 10;
    }

    i++;
    kputs(kprint_buf, kprint_buf_i, buf + i);

    return;
}

static const char hex_to_ascii_tab[] = "0123456789abcdef";

static void kprn_x(char *kprint_buf, size_t *kprint_buf_i, uint64_t x, int pad) {
    int i = 15;
    char buf[] = "0000000000000000";

    if (x) {
        for ( ; x; i--) {
            buf[i] = hex_to_ascii_tab[(x % 16)];
            x /= 16;
        }
        i++;
    }

    kputs(kprint_buf, kprint_buf_i, "0x");
    kputs(kprint_buf, kprint_buf_i, buf + (pad ? (16 - pad) : i));
}

static void print_timestamp(char *kprint_buf, size_t *kprint_buf_i, int type) {
    kputs(kprint_buf, kprint_buf_i, "\e[37m[");
    kprn_ui(kprint_buf, kprint_buf_i, uptime_sec);
    kputs(kprint_buf, kprint_buf_i, ".");
    kprn_ui(kprint_buf, kprint_buf_i, uptime_raw);
    kputs(kprint_buf, kprint_buf_i, "] ");

    switch (type) {
        case KPRN_INFO:
            kputs(kprint_buf, kprint_buf_i, "\e[36minfo\e[37m: ");
            break;
        case KPRN_WARN:
            kputs(kprint_buf, kprint_buf_i, "\e[33mwarning\e[37m: ");
            break;
        case KPRN_ERR:
            kputs(kprint_buf, kprint_buf_i, "\e[31mERROR\e[37m: ");
            break;
        case KPRN_PANIC:
            kputs(kprint_buf, kprint_buf_i, "\e[31mPANIC\e[37m: ");
            break;
        default:
        case KPRN_DBG:
            kputs(kprint_buf, kprint_buf_i, "\e[36mDEBUG\e[37m: ");
            break;
    }
}

void kprint(int type, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    kvprint(type, fmt, args);
    va_end(args);

    return;
}

void kvprint(int type, const char *fmt, va_list args) {
    char kprint_buf[KPRINT_BUF_MAX];
    size_t kprint_buf_i = 0;

    print_timestamp(kprint_buf, &kprint_buf_i, type);

    char *str;
    size_t str_len;

    for (;;) {
        char c;

        while (*fmt && *fmt != '%') {
            kputchar(kprint_buf, &kprint_buf_i, *fmt);
            if (*fmt == '\n')
                print_timestamp(kprint_buf, &kprint_buf_i, type);
            fmt++;
        }
        if (!*fmt++) {
            kputchar(kprint_buf, &kprint_buf_i, '\n');
            goto out;
        }
        int val = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            val *= 10;
            val += *fmt - '0';
            fmt++;
        }
        switch (*fmt++) {
            case 's':
                str = (char *)va_arg(args, const char *);
                if (!str)
                    kputs(kprint_buf, &kprint_buf_i, "(null)");
                else
                    kputs(kprint_buf, &kprint_buf_i, str);
                break;
            case 'S':
                str_len = va_arg(args, size_t);
                str = (char *)va_arg(args, const char *);
                knputs(kprint_buf, &kprint_buf_i, str, str_len);
                break;
            case 'd':
                kprn_i(kprint_buf, &kprint_buf_i, (int64_t)va_arg(args, int));
                break;
            case 'D':
                kprn_i(kprint_buf, &kprint_buf_i, (int64_t)va_arg(args, int64_t));
                break;
            case 'u':
                kprn_ui(kprint_buf, &kprint_buf_i, (uint64_t)va_arg(args, unsigned int));
                break;
            case 'U':
                kprn_ui(kprint_buf, &kprint_buf_i, (uint64_t)va_arg(args, uint64_t));
                break;
            case 'x':
                kprn_x(kprint_buf, &kprint_buf_i, (uint64_t)va_arg(args, unsigned int), val);
                break;
            case 'X':
                kprn_x(kprint_buf, &kprint_buf_i, (uint64_t)va_arg(args, uint64_t), val);
                break;
            case 'c':
                c = (char)va_arg(args, int);
                kputchar(kprint_buf, &kprint_buf_i, c);
                break;
            default:
                kputchar(kprint_buf, &kprint_buf_i, '?');
                break;
        }
    }

out:
    if (type == KPRN_INFO || type == KPRN_DBG) {
        kprint_buf_flush(kprint_buf, &kprint_buf_i);
    } else {
        kprint_buf_flush_urgent(kprint_buf, &kprint_buf_i);
    }

    return;
}
