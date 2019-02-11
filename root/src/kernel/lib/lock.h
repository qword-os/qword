#ifndef __LOCK_H__
#define __LOCK_H__

#include <stdint.h>
#include <lib/qemu.h>

struct last_acquirer_t {
    const char *file;
    const char *func;
    int line;
};

typedef struct {
    int lock;
    struct last_acquirer_t last_acquirer;
} lock_t;

__attribute__((unused)) static const lock_t new_lock = {
    1,
    { "N/A", "N/A", 0 }
};

__attribute__((unused)) static const lock_t new_lock_acquired = {
    0,
    { "N/A", "N/A", 0 }
};

#define locked_read(type, var) ({ \
    type ret = 0; \
    asm volatile ( \
        "lock xadd %1, %0;" \
        : "+r" (ret) \
        : "m" (*var) \
        : "memory", "cc" \
    ); \
    ret; \
})

#define locked_inc(var) ({ \
    int ret; \
    asm volatile ( \
        "lock inc %1;" \
        : "=@ccnz" (ret) \
        : "m" (*var) \
        : "memory" \
    ); \
    ret; \
})

#define locked_dec(var) ({ \
    int ret; \
    asm volatile ( \
        "lock dec %1;" \
        : "=@ccnz" (ret) \
        : "m" (*var) \
        : "memory" \
    ); \
    ret; \
})

__attribute__((noinline)) __attribute__((unused)) static void deadlock_detect(const char *file,
                       const char *function,
                       int line,
                       const char *lockname,
                       lock_t *lock) {
    qemu_debug_puts("\n---\npossible deadlock at: spinlock_acquire(");
    qemu_debug_puts(lockname);
    qemu_debug_puts(");");
    qemu_debug_puts("\nfile: ");
    qemu_debug_puts(file);
    qemu_debug_puts("\nfunction: ");
    qemu_debug_puts(function);
    qemu_debug_puts("\nline: ");
    if (!line) {
        qemu_debug_puts("0");
    } else {
        int i;
        char buf[21] = {0};
        for (i = 19; line; i--) {
            buf[i] = (line % 10) + '0';
            line /= 10;
        }
        i++;
        qemu_debug_puts(buf + i);
    }
    qemu_debug_puts("\n---\nlast acquirer:");
    qemu_debug_puts("\nfile: ");
    qemu_debug_puts(lock->last_acquirer.file);
    qemu_debug_puts("\nfunction: ");
    qemu_debug_puts(lock->last_acquirer.func);
    qemu_debug_puts("\nline: ");
    if (!lock->last_acquirer.line) {
        qemu_debug_puts("0");
    } else {
        int i;
        char buf[21] = {0};
        for (i = 19; lock->last_acquirer.line; i--) {
            buf[i] = (lock->last_acquirer.line % 10) + '0';
            lock->last_acquirer.line /= 10;
        }
        i++;
        qemu_debug_puts(buf + i);
    }
    qemu_debug_puts("\n---\n");
}

#define spinlock_acquire(lock) ({ \
    __label__ retry; \
    __label__ out; \
retry:; \
    for (int i = 0; i < 0xffffff; i++) \
        if (spinlock_test_and_acquire(lock)) \
            goto out; \
    deadlock_detect(__FILE__, __func__, __LINE__, #lock, lock); \
    goto retry; \
out:; \
})

#define spinlock_test_and_acquire(LOCK) ({ \
    int ret; \
    asm volatile ( \
        "lock btr %0, 0;" \
        : "+m" ((LOCK)->lock), "=@ccc" (ret) \
        : \
        : "memory" \
    ); \
    if (ret) { \
        (LOCK)->last_acquirer.file = __FILE__; \
        (LOCK)->last_acquirer.func = __func__; \
        (LOCK)->last_acquirer.line = __LINE__; \
    } \
    ret; \
})

__attribute__((always_inline)) __attribute__((unused)) static inline void spinlock_release(lock_t *lock) {
    asm volatile (
        "lock bts %0, 0;"
        : "+m" (lock->lock)
        :
        : "memory"
    );
}

#endif
