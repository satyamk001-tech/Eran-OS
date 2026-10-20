#ifndef ERAN_DRIVERS_MOUSE_H
#define ERAN_DRIVERS_MOUSE_H

#include <stdint.h>
#include <stddef.h>

#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

typedef struct {
    int32_t x;
    int32_t y;
    uint8_t buttons;
    int32_t delta_x;
    int32_t delta_y;
} mouse_state_t;

void mouse_init(void);
const mouse_state_t *mouse_get_state(void);
int  mouse_has_moved(void);
void mouse_print_status(void);

#endif /* ERAN_DRIVERS_MOUSE_H */
