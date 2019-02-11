#ifndef __LOCK_H__
#define __LOCK_H__

#include <stdint.h>
#include <stddef.h>
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

#define uint_to_str(val) ({ \
    char buf[21] = {0}; \
    int i; \
    int val_copy = (val); \
    if (!(val)) { \
        buf[0] = '0'; \
        buf[1] = 0; \
        i = 0; \
    } else { \
        for (i = 19; val_copy; i--) { \
            buf[i] = (val_copy % 10) + '0'; \
            val_copy /= 10; \
        } \
        i++; \
    } \
    (const char *)(buf + i); \
})

__attribute__((noinline)) __attribute__((unused)) static void deadlock_detect(const char *file,
                       const char *function,
                       int line,
                       const char *lockname,
                       lock_t *lock,
                       size_t iter) {
    qemu_debug_puts_urgent("\n---\npossible deadlock at: spinlock_acquire(");
    qemu_debug_puts_urgent(lockname);
    qemu_debug_puts_urgent(");");
    qemu_debug_puts_urgent("\nfile: ");
    qemu_debug_puts_urgent(file);
    qemu_debug_puts_urgent("\nfunction: ");
    qemu_debug_puts_urgent(function);
    qemu_debug_puts_urgent("\nline: ");
    qemu_debug_puts_urgent(uint_to_str(line));
    qemu_debug_puts_urgent("\n---\nlast acquirer:");
    qemu_debug_puts_urgent("\nfile: ");
    qemu_debug_puts_urgent(lock->last_acquirer.file);
    qemu_debug_puts_urgent("\nfunction: ");
    qemu_debug_puts_urgent(lock->last_acquirer.func);
    qemu_debug_puts_urgent("\nline: ");
    qemu_debug_puts_urgent(uint_to_str(lock->last_acquirer.line));
    qemu_debug_puts_urgent("\n---\nassumed locked after it spun for ");
    qemu_debug_puts_urgent(uint_to_str(iter));
    qemu_debug_puts_urgent("iterations\n---");
}

#define spinlock_acquire(lock) ({ \
    __label__ retry; \
    __label__ out; \
    size_t i = 0x1000000; \
retry:; \
    for (; i; i--) \
        if (spinlock_test_and_acquire(lock)) \
            goto out; \
    deadlock_detect(__FILE__, __func__, __LINE__, #lock, lock, i); \
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
        : "memory", "cc"
    );
}

#endif
