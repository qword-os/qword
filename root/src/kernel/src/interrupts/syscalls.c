#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <smp.h>
#include <task.h>
#include <lock.h>
#include <fs.h>
#include <task.h>
#include <mm.h>

static inline int privilege_check(size_t base, size_t len) {
    if ( base & (size_t)0x800000000000
     || (base + len) & (size_t)0x800000000000)
        return 1;
    else
        return 0;
}

/* Prototype syscall: int syscall_name(struct ctx_t *ctx) */

/* Conventional argument passing: rdi, rsi, rdx, r10, r8, r9 */

int syscall_set_fs_base(struct ctx_t *ctx) {
    // rdi: new fs base

    pid_t current_task = cpu_locals[current_cpu].current_task;

    struct thread_t *thread = task_table[current_task];

    thread->fs_base = ctx->rdi;
    load_fs_base(ctx->rdi);

    return 0;
}

void *syscall_alloc_at(struct ctx_t *ctx) {
    // rdi: virtual address / 0 for sbrk-like allocation
    // rsi: page count

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *process = process_table[current_process];

    size_t base_address;
    if (ctx->rdi) {
        base_address = ctx->rdi;
    } else {
        base_address = process->cur_brk;
        process->cur_brk += ctx->rsi * PAGE_SIZE;
    }

    for (size_t i = 0; i < ctx->rsi; i++) {
        void *ptr = pmm_alloc(1);
        if (!ptr)
            return (void *)0;
        if (map_page(process->pagemap, (size_t)ptr, base_address + i * PAGE_SIZE, 0x07)) {
            pmm_free(ptr, 1);
            return (void *)0;
        }
    }

    return (void *)base_address;
}

int syscall_debug_print(struct ctx_t *ctx) {
    // rdi: print type
    // rsi: string

    // Make sure the type isn't invalid
    if (ctx->rdi > KPRN_MAX_TYPE)
        return -1;

    // Make sure we're not trying to print memory that doesn't belong to us
    if (privilege_check(ctx->rsi, kstrlen((const char *)ctx->rsi)))
        return -1;

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

    if (privilege_check(ctx->rdi, kstrlen((const char *)ctx->rdi)))
        return -1;

    char abs_path[2048];
    vfs_get_absolute_path(abs_path, (const char *)ctx->rdi, process->cwd);
    int fd = open(abs_path, ctx->rsi, ctx->rdx);
    if (fd < 0)
        return fd;

    process->file_handles[local_fd] = fd;

    return local_fd;
}

int syscall_close(struct ctx_t *ctx) {
    // rdi: fd

    // TODO lock this stuff properly

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *process = process_table[current_process];

    if (process->file_handles[ctx->rdi] == -1)
        return -1;

    int ret = close(process->file_handles[ctx->rdi]);
    if (ret < 0)
        return ret;

    process->file_handles[ctx->rdi] = -1;

    return 0;
}

int syscall_lseek(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: offset
    // rdx: type

    // TODO lock this stuff properly

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *process = process_table[current_process];

    if (process->file_handles[ctx->rdi] == -1)
        return -1;

    return lseek(process->file_handles[ctx->rdi], ctx->rsi, ctx->rdx);
}

int syscall_read(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: buf
    // rdx: len

    // TODO lock this stuff properly

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *process = process_table[current_process];

    if (process->file_handles[ctx->rdi] == -1)
        return -1;

    if (privilege_check(ctx->rsi, ctx->rdx))
        return -1;

    return read(process->file_handles[ctx->rdi], (void *)ctx->rsi, ctx->rdx);
}

int syscall_write(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: buf
    // rdx: len

    // TODO lock this stuff properly

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *process = process_table[current_process];

    if (process->file_handles[ctx->rdi] == -1)
        return -1;

    if (privilege_check(ctx->rsi, ctx->rdx))
        return -1;

    return write(process->file_handles[ctx->rdi], (const void *)ctx->rsi, ctx->rdx);
}
