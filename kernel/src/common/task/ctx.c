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
#endif
