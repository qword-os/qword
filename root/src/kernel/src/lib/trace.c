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

    asm volatile (
        "mov rax, rbp"
        : "=a" (bp)
    );

    kprint(type, ">>> STACKTRACE BEGIN <<<");

	for (;;) {
		if (!bp)
            break;
		size_t ip = bp[1];
        size_t displacement;
		bp = (size_t *)bp[0];
        char *name = get_symbol_from_address(&displacement, ip);
        kprint(type, "%X: %s + %X", ip, name, displacement);
	}

    kprint(type, ">>> STACKTRACE END <<<");

    return;
}
