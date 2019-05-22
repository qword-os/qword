#include <stdint.h>
#include <lib/cio.h>
#include <lib/time.h>

//Play sound using built in speaker
static void play_sound(uint32_t nFrequence) {
    uint32_t Div;
    uint8_t tmp;

    //Set the PIT to the desired frequency
    Div = 1193180 / nFrequence;
    port_out_b(0x43, 0xb6);
    port_out_b(0x42, (uint8_t) (Div) );
    port_out_b(0x42, (uint8_t) (Div >> 8));

    //And play the sound using the PC speaker
    tmp = port_in_b(0x61);

    if (tmp != (tmp | 3)) {
        port_out_b(0x61, tmp | 3);
    }
}

//make it shutup
static void nosound(void) {
    uint8_t tmp = port_in_b(0x61) & 0xFC;

    port_out_b(0x61, tmp);
}

//Make a beep
void beep(void) {
 	play_sound(1000);
 	ksleep(100);
 	nosound();
    //set_PIT_2(old_frequency);
}
