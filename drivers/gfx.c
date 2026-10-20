#include "gfx.h"
#include "vga.h"

static canvas_cell_t backbuffer[CANVAS_HEIGHT][CANVAS_WIDTH];
static volatile uint16_t *vga_mem = (volatile uint16_t *)0xB8000;

void gfx_init(void) {
    gfx_clear(VGA_COLOR_BLACK);
}

void gfx_clear(uint8_t bg_color) {
    uint8_t attr = (bg_color << 4) | VGA_COLOR_WHITE;
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            backbuffer[y][x].ch = ' ';
            backbuffer[y][x].attr = attr;
        }
    }
}

void gfx_put_char(int x, int y, char ch, uint8_t fg, uint8_t bg) {
    if (x < 0 || x >= CANVAS_WIDTH || y < 0 || y >= CANVAS_HEIGHT) return;
    backbuffer[y][x].ch = ch;
    backbuffer[y][x].attr = (bg << 4) | (fg & 0x0F);
}

void gfx_draw_string(int x, int y, const char *str, uint8_t fg, uint8_t bg) {
    if (y < 0 || y >= CANVAS_HEIGHT || !str) return;
    int cur_x = x;
    while (*str && cur_x < CANVAS_WIDTH) {
        if (cur_x >= 0) {
            gfx_put_char(cur_x, y, *str, fg, bg);
        }
        cur_x++;
        str++;
    }
}

void gfx_fill_rect(int x, int y, int w, int h, char fill_ch, uint8_t fg, uint8_t bg) {
    for (int r = y; r < y + h; r++) {
        for (int c = x; c < x + w; c++) {
            gfx_put_char(c, r, fill_ch, fg, bg);
        }
    }
}

void gfx_draw_box(int x, int y, int w, int h, uint8_t fg, uint8_t bg, const char *title) {
    if (w < 2 || h < 2) return;

    // Interior
    gfx_fill_rect(x + 1, y + 1, w - 2, h - 2, ' ', fg, bg);

    // Corners
    gfx_put_char(x, y, '+', fg, bg);
    gfx_put_char(x + w - 1, y, '+', fg, bg);
    gfx_put_char(x, y + h - 1, '+', fg, bg);
    gfx_put_char(x + w - 1, y + h - 1, '+', fg, bg);

    // Horizontal edges
    for (int c = x + 1; c < x + w - 1; c++) {
        gfx_put_char(c, y, '-', fg, bg);
        gfx_put_char(c, y + h - 1, '-', fg, bg);
    }

    // Vertical edges
    for (int r = y + 1; r < y + h - 1; r++) {
        gfx_put_char(x, r, '|', fg, bg);
        gfx_put_char(x + w - 1, r, '|', fg, bg);
    }

    // Optional Title Header
    if (title) {
        int title_len = 0;
        const char *t = title;
        while (*t++) title_len++;

        int start_x = x + (w - title_len) / 2;
        if (start_x < x + 1) start_x = x + 1;
        gfx_draw_string(start_x, y, title, VGA_COLOR_WHITE, bg);
    }
}

void gfx_flip_to_screen(void) {
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            uint16_t entry = (backbuffer[y][x].attr << 8) | (uint8_t)backbuffer[y][x].ch;
            vga_mem[y * CANVAS_WIDTH + x] = entry;
        }
    }
}
