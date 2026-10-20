#ifndef ERAN_DRIVERS_KEYBOARD_H
#define ERAN_DRIVERS_KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
int  keyboard_has_char(void);
char keyboard_getc(void);
char keyboard_getchar(void);

#endif /* ERAN_DRIVERS_KEYBOARD_H */
