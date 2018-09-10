#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <smp.h>
#include <task.h>
#include <lock.h>

//int syscall_name(ctx_t *ctx / void)

int test_syscall(struct ctx *ctx) {
    spinlock_acquire(&scheduler_lock);

    kprint(KPRN_DBG, "process %u (CPU %u) issued test syscall.", cpu_locals[current_cpu].current_process, current_cpu);
    kprint(KPRN_DBG, " \\");
    kprint(KPRN_DBG, "   --- content: %s", ctx->rdi);

    spinlock_release(&scheduler_lock);

    return 0;
}
