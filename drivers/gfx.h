#ifndef ERAN_DRIVERS_GFX_H
#define ERAN_DRIVERS_GFX_H

#include <stdint.h>
#include <stddef.h>

#define CANVAS_WIDTH  80
#define CANVAS_HEIGHT 25

typedef struct {
    char ch;
    uint8_t attr;
} canvas_cell_t;

void gfx_init(void);
void gfx_clear(uint8_t bg_color);
void gfx_put_char(int x, int y, char ch, uint8_t fg, uint8_t bg);
void gfx_draw_string(int x, int y, const char *str, uint8_t fg, uint8_t bg);
void gfx_fill_rect(int x, int y, int w, int h, char fill_ch, uint8_t fg, uint8_t bg);
void gfx_draw_box(int x, int y, int w, int h, uint8_t fg, uint8_t bg, const char *title);
void gfx_flip_to_screen(void);

#endif /* ERAN_DRIVERS_GFX_H */
