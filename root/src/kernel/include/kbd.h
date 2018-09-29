#ifndef __KBD_H__
#define __KBD_H__

int kbd_read(char *, size_t);
void kbd_handler(uint8_t);
void init_kbd(void);

#endif
