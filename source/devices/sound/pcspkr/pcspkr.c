#include <stdint.h>
#include <lib/cio.h>
#include <lib/time.h>

// Play sound using built in speaker
static void play_sound(uint32_t frequency) {
    // Set the PIT to the desired frequency
    uint32_t x = 1193180 / frequency;
    port_out_b(0x43, 0xb6);
    port_out_b(0x42, (uint8_t) (x) );
    port_out_b(0x42, (uint8_t) (x >> 8));

    // And play the sound using the PC speaker
    uint32_t tmp = port_in_b(0x61);

    if (tmp != (tmp | 3)) {
        port_out_b(0x61, tmp | 3);
    }
}

// Stop playback of sound
static void stop(void) {
    uint8_t tmp = port_in_b(0x61) & 0xFC;

    port_out_b(0x61, tmp);
}

// Make a beep
void beep(void) {
    play_sound(1000);
    ksleep(100);
    stop();
    //set_PIT_2(old_frequency);
}
