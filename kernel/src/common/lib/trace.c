#include <stdint.h>
#include <stddef.h>
#include <trace.h>
#include <klib.h>

char *get_symbol_from_address(size_t *displacement, size_t addr) {
    size_t i;

    for (i = 0; ; i++) {
        if (debug_symbols_addresses[i] > addr) {
            i--;
            *displacement = addr - debug_symbols_addresses[i];
            break;
        }
    }

    /* find the respective name */
    char *p = debug_symbols_names;
    for (size_t j = 0; j < i; j++) {
        p += kstrlen(p) + 1;
    }

    return p;
}

void print_stacktrace(int type) {
    size_t *bp;

    #ifdef __X86_64__
        asm volatile (
            "mov rax, rbp"
            : "=a" (bp)
        );
    #endif
    #ifdef __I386__
        asm volatile (
            "mov eax, ebp"
            : "=a" (bp)
        );
    #endif

    kprint(type, ">>> STACKTRACE BEGIN <<<");

	for (;;) {
		size_t ip = bp[1];
        size_t displacement;
        #ifdef __X86_64__
		if (ip == 0xffffffffffffffff)
        #endif
        #ifdef __I386__
        if (ip == 0xffffffff)
        #endif
            break;
		bp = (size_t *)bp[0];
        char *name = get_symbol_from_address(&displacement, ip);
        #ifdef __X86_64__
            kprint(type, "%s + %X", name, displacement);
        #endif
        #ifdef __I386__
            kprint(type, "%s + %x", name, displacement);
        #endif
	}

    kprint(type, ">>> STACKTRACE END <<<");

    return;
}
