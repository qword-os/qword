#include <lib/rand.h>
#include <lib/klib.h>
#include <lib/lock.h>

static uint64_t seed;

void init_rand(void) {
    if (!rdrand_supported) {
        kprint(KPRN_WARN, "kernel: rdrand instruction not supported");
        seed = rdtsc(uint64_t);
        srand(0x948f2a057ef959b7ULL);
    } else {
        seed = rdrand(uint64_t);
        srand(rdrand(uint64_t));
    }
}

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
// source: http://www.pcg-random.org/download.html

static uint64_t inc = 0;

int rand(void) {
    uint64_t oldstate = locked_read(uint64_t, &seed);
    // Advance internal state
    locked_write(uint64_t, &seed, oldstate * 6364136223846793005ULL + (inc|1));
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (int)((xorshifted >> rot) | (xorshifted << ((-rot) & 31)));
}

int srand(uint64_t c) {
    locked_write(uint64_t, &inc, c);
}
