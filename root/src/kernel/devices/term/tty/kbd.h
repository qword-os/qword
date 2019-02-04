#ifndef __KBD_H__
#define __KBD_H__

int kbd_read(char *, size_t);
void kbd_handler(void *);
void init_kbd(void);

#endif
