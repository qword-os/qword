#include <irq.h>
#include <pic.h>
#include <klib.h>

void pit_handler(void) {
    pic_send_eoi(0);
    kprint(KPRN_INFO, "Timer interrupt!");
}
