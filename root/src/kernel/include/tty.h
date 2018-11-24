#ifndef __TTY_H__
#define __TTY_H__

void init_tty(void);
void tty_putchar(char);
int tty_write(int, const void *, uint64_t, size_t);
int tty_read(int, void *, uint64_t, size_t);

#endif
