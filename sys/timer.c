#include <sys/hpet.h>
#include <sys/pit.h>
#include <lib/klib.h>
#include <sys/timer.h>

void init_timers(void) {
    if (!init_hpet()) {
        kprint(KPRN_WARN, "timer: HPET couldn't init, defaulting to PIT");

        if (!init_pit()) {
            kprint(KPRN_PANIC, "No timer could be initialised");
        }
    }
}
