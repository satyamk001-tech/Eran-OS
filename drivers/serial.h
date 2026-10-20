#ifndef ERAN_DRIVERS_SERIAL_H
#define ERAN_DRIVERS_SERIAL_H

#include <stdint.h>
#include <stddef.h>

#define COM1_PORT 0x3F8

void serial_init(uint32_t baud);
void serial_putc(char c);
void serial_puts(const char *str);
void serial_write_hex(uint32_t val);
void serial_write_dec(uint32_t val);
int  serial_has_rx(void);
char serial_getc(void);

#endif /* ERAN_DRIVERS_SERIAL_H */
