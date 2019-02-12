#include <lib/rand.h>
#include <lib/klib.h>

static uint64_t seed;

void init_rand(void) {
    if (!rdrand_supported) {
        kprint(KPRN_WARN, "kernel: rdrand instruction not supported\n"
                          "        /dev/random entropy might suffer");
        seed = rdtsc(uint64_t);
    } else {
        seed = rdrand(uint64_t);
    }
}

// from: https://stackoverflow.com/questions/24005459/implementation-of-the-random-number-generator-in-c-c
int rand(void) {
    seed = seed * 1103515245 + 12345;
    return (int)(seed / 65536) % RAND_MAX;
}
