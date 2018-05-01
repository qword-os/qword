#include <ctx.h>
#include <task.h>
#include <smp.h>

#ifdef __I386__
void ctx_switch(ctx_t *next) {
    /* TODO get current context here */
    return;
} 
#endif

#ifdef __X86_64__
void set_ctx_krnl(ctx_t *ctx) {
    /* Setup sane defaults for new context */
    ctx->es = 0x10;
    ctx->ds = 0x10;
    ctx->r15 = 0;
    ctx->r14 = 0;
    ctx->r13 = 0;
    ctx->r12 = 0;
    ctx->r11 = 0;
    ctx->r10 = 0;
    ctx->r9 = 0;
    ctx->r8 = 0;
    ctx->rbp = 0;
    ctx->rdi = 0;
    ctx->rsi = 0;
    ctx->rdx = 0;
    ctx->rcx = 0;
    ctx->rbx = 0;
    ctx->rax = 0;
    ctx->cs = 0x08;
    ctx->rflags = 0x202;
    ctx->ss = 0x10;
    return;
}

#endif
