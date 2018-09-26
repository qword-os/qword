#ifndef __KBD_H__
#define __KBD_H__

extern char tty_bufs[8][256];
void kbd_handler(uint8_t);
void init_kbd(void);

#endif
