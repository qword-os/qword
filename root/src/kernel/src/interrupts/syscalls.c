#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <smp.h>
#include <task.h>

//int syscall_name(ctx_t *ctx / void)

int test_syscall(struct ctx *ctx) {
    kprint(KPRN_DBG, "process %u issued test syscall.", cpu_locals[current_cpu].current_process);
    kprint(KPRN_DBG, " \\");
    kprint(KPRN_DBG, "   --- content: %s", ctx->rdi);

    return 0;
}
