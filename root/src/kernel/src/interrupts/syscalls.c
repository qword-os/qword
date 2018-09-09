#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <smp.h>

//int syscall_name(ctx_t *ctx / void)

int test_syscall(void) {
    kprint(KPRN_DBG, "process %u issued test syscall.", cpu_locals[current_cpu].current_process);

    return 0;
}
