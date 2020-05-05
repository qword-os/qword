#include <lib/rand.h>
#include <lib/klib.h>
#include <lib/lock.h>

void init_rand(void) {
    uint32_t seed = ((uint32_t)0xc597060c * rdtsc(uint32_t))
                  * ((uint32_t)0xce86d624)
                  ^ ((uint32_t)0xee0da130 * rdtsc(uint32_t));

    if (!rdrand_supported) {
        kprint(KPRN_INFO, "rand: rdrand instruction not supported");
        srand(seed);
    } else {
        kprint(KPRN_INFO, "rand: rdrand instruction supported");
        seed *= (seed ^ rdrand(uint32_t));
        srand(seed);
    }
}

#define n ((int)624)
#define m ((int)397)
#define matrix_a ((uint32_t)0x9908b0df)
#define msb ((uint32_t)0x80000000)
#define lsbs ((uint32_t)0x7fffffff)

static uint32_t status[n];
static int ctr;

static lock_t rand_lock = new_lock;

void srand(uint32_t s) {
    spinlock_acquire(&rand_lock);
    status[0] = s;
    for (ctr = 1; ctr < n; ctr++)
        status[ctr] = (1812433253 * (status[ctr - 1] ^ (status[ctr - 1] >> 30)) + ctr);
    spinlock_release(&rand_lock);
}

uint32_t rand32(void) {
    spinlock_acquire(&rand_lock);

    const uint32_t mag01[2] = {0, matrix_a};

    if (ctr >= n) {
        for (int kk = 0; kk < n - m; kk++) {
            uint32_t y = (status[kk] & msb) | (status[kk + 1] & lsbs);
            status[kk] = status[kk + m] ^ (y >> 1) ^ mag01[y & 1];
        }

        for (int kk = n - m; kk < n - 1; kk++) {
            uint32_t y = (status[kk] & msb) | (status[kk + 1] & lsbs);
            status[kk] = status[kk + (m - n)] ^ (y >> 1) ^ mag01[y & 1];
        }

        uint32_t y = (status[n - 1] & msb) | (status[0] & lsbs);
        status[n - 1] = status[m - 1] ^ (y >> 1) ^ mag01[y & 1];

        ctr = 0;
    }

    uint32_t res = status[ctr++];

    res ^= (res >> 11);
    res ^= (res << 7) & 0x9d2c5680;
    res ^= (res << 15) & 0xefc60000;
    res ^= (res >> 18);

    spinlock_release(&rand_lock);
    return res;
}

uint64_t rand64(void) {
    return (((uint64_t)rand32()) << 32) | (uint64_t)rand32();
}
