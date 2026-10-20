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

// Sensitivity controls: 1=Slow(50%), 2=Normal(100%), 3=Fast(150%), 4=High(200%)
void mouse_set_sensitivity(int percent);
int  mouse_get_sensitivity(void);
void mouse_set_position(int x, int y);

#endif /* ERAN_DRIVERS_MOUSE_H */
