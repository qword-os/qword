#ifndef __CTX_H__
#define __CTX_H__

#include <stdint.h>

#ifdef __I386__
typedef struct {
    uint32_t esp;
    uint32_t ebp;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t edi;
    uint32_t esi;
    uint32_t eflags;
} ctx_t;
#endif /* I386 */

#ifdef __X86_64__
typedef struct {
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rflags;
} ctx_t;
#endif /* X86_64 */

ctx_t *new_ctx(void);
void ctx_switch(ctx_t *);

#endif
