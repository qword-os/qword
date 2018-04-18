#include <irq.h>
#include <pic.h>
#include <klib.h>
#include <time.h>
#include <pit.h>

void pit_handler(void) {
    if (!(++uptime_raw % PIT_FREQUENCY)) {
        uptime_sec++;
    }
    pic_send_eoi(0);
    return;
}
