#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <smp.h>
#include <task.h>
#include <lock.h>
#include <fs.h>
#include <task.h>

/* Prototype syscall: int syscall_name(struct ctx_t *ctx) */

/* Conventional argument passing: rdi, rsi, rdx, r10, r8, r9 */

int syscall_debug_print(struct ctx_t *ctx) {
    // rdi: print type
    // rsi: string

    // Make sure the type isn't invalid
    if (ctx->rdi > KPRN_MAX_TYPE)
        return -1;

    // Make sure we're not trying to print memory that doesn't belong to us
    //TODO:privilege_check_string((const char *)ctx->rsi);

    kprint(ctx->rdi, "[%u:%u:%u] %s",
           cpu_locals[current_cpu].current_process,
           cpu_locals[current_cpu].current_thread,
           current_cpu,
           ctx->rsi);

    return 0;
}

int syscall_open(struct ctx_t *ctx) {
    // rdi: path
    // rsi: mode
    // rdx: perms

    // TODO lock this stuff properly

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *process = process_table[current_process];

    int local_fd;

    for (local_fd = 0; process->file_handles[local_fd] != -1; local_fd++)
        if (local_fd + 1 == MAX_FILE_HANDLES)
            return -1;

    //TODO:privilege_check_string((const char *)ctx->rdi);

    int fd = open((const char *)ctx->rdi, ctx->rsi, ctx->rdx);
    if (fd == -1)
        return -1;

    process->file_handles[local_fd] = fd;

    return local_fd;
}

int syscall_close(struct ctx_t *ctx) {
    // rdi: fd

    // TODO lock this stuff properly

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *process = process_table[current_process];

    if (close(process->file_handles[ctx->rdi]) == -1)
        return -1;

    process->file_handles[ctx->rdi] = -1;

    return 0;
}

int syscall_read(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: buf
    // rdx: len

    //TODO:privilege_check_buf((const void *)ctx->rsi, ctx->rdx);

    // TODO lock this stuff properly

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *process = process_table[current_process];

    if (process->file_handles[ctx->rdi] == -1)
        return -1;

    return read(process->file_handles[ctx->rdi], (void *)ctx->rsi, ctx->rdx);
}

int syscall_write(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: buf
    // rdx: len

    //TODO:privilege_check_buf((const void *)ctx->rsi, ctx->rdx);

    // TODO lock this stuff properly

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *process = process_table[current_process];

    if (process->file_handles[ctx->rdi] == -1)
        return -1;

    return write(process->file_handles[ctx->rdi], (const void *)ctx->rsi, ctx->rdx);
}
