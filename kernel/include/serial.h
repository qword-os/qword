#ifndef __SERIAL_H__
#define __SERIAL_H__

void serial_init(void);
int can_read();
char serial_read();
int can_transmit_empty();
void serial_write(char data);

#endif
