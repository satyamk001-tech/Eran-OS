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

    // Corners (CP437 single line)
    gfx_put_char(x, y, (char)0xDA, fg, bg);
    gfx_put_char(x + w - 1, y, (char)0xBF, fg, bg);
    gfx_put_char(x, y + h - 1, (char)0xC0, fg, bg);
    gfx_put_char(x + w - 1, y + h - 1, (char)0xD9, fg, bg);

    // Horizontal edges
    for (int c = x + 1; c < x + w - 1; c++) {
        gfx_put_char(c, y, (char)0xC4, fg, bg);
        gfx_put_char(c, y + h - 1, (char)0xC4, fg, bg);
    }

    // Vertical edges
    for (int r = y + 1; r < y + h - 1; r++) {
        gfx_put_char(x, r, (char)0xB3, fg, bg);
        gfx_put_char(x + w - 1, r, (char)0xB3, fg, bg);
    }

    // Optional Title Header
    if (title) {
        int title_len = 0;
        const char *t = title;
        while (*t++) title_len++;

        int start_x = x + (w - title_len) / 2;
        if (start_x < x + 1) start_x = x + 1;
        gfx_draw_string(start_x, y, title, fg, bg);
    }
}

void gfx_draw_double_box(int x, int y, int w, int h, uint8_t fg, uint8_t bg, const char *title) {
    if (w < 2 || h < 2) return;

    // Interior
    gfx_fill_rect(x + 1, y + 1, w - 2, h - 2, ' ', fg, bg);

    // Corners (CP437 double line)
    gfx_put_char(x, y, (char)0xC9, fg, bg);
    gfx_put_char(x + w - 1, y, (char)0xBB, fg, bg);
    gfx_put_char(x, y + h - 1, (char)0xC8, fg, bg);
    gfx_put_char(x + w - 1, y + h - 1, (char)0xBC, fg, bg);

    // Horizontal edges
    for (int c = x + 1; c < x + w - 1; c++) {
        gfx_put_char(c, y, (char)0xCD, fg, bg);
        gfx_put_char(c, y + h - 1, (char)0xCD, fg, bg);
    }

    // Vertical edges
    for (int r = y + 1; r < y + h - 1; r++) {
        gfx_put_char(x, r, (char)0xBA, fg, bg);
        gfx_put_char(x + w - 1, r, (char)0xBA, fg, bg);
    }

    // Window controls and Title Header
    if (title) {
        int title_len = 0;
        const char *t = title;
        while (*t++) title_len++;

        // Draw left window control: [■]
        if (w >= 10) {
            gfx_put_char(x + 2, y, '[', VGA_COLOR_DARK_GREY, bg);
            gfx_put_char(x + 3, y, (char)0xFE, VGA_COLOR_LIGHT_RED, bg); // close box
            gfx_put_char(x + 4, y, ']', VGA_COLOR_DARK_GREY, bg);
        }

        // Draw Title centered
        int start_x = x + (w - title_len) / 2;
        if (start_x < x + 6) start_x = x + 6;
        gfx_draw_string(start_x, y, title, fg, bg);

        // Draw right window control: [^]
        if (w >= 14 && (x + w - 4) > start_x + title_len) {
            gfx_put_char(x + w - 4, y, '[', VGA_COLOR_DARK_GREY, bg);
            gfx_put_char(x + w - 3, y, (char)0x18, VGA_COLOR_LIGHT_GREEN, bg);
            gfx_put_char(x + w - 2, y, ']', VGA_COLOR_DARK_GREY, bg);
        }
    }
}

void gfx_draw_shadow(int x, int y, int w, int h) {
    // Right drop shadow
    for (int r = y + 1; r <= y + h && r < CANVAS_HEIGHT; r++) {
        if (x + w < CANVAS_WIDTH) {
            gfx_put_char(x + w, r, (char)0xB2, VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        }
    }
    // Bottom drop shadow
    for (int c = x + 1; c <= x + w && c < CANVAS_WIDTH; c++) {
        if (y + h < CANVAS_HEIGHT) {
            gfx_put_char(c, y + h, (char)0xB2, VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        }
    }
}

void gfx_draw_progress_bar(int x, int y, int width, int percent, uint8_t fg, uint8_t bg) {
    if (width < 3) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    int bar_width = width - 2;
    int filled = (percent * bar_width) / 100;

    gfx_put_char(x, y, '[', fg, bg);
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            gfx_put_char(x + 1 + i, y, (char)0xDB, fg, bg); // filled block █
        } else {
            gfx_put_char(x + 1 + i, y, (char)0xB0, VGA_COLOR_DARK_GREY, bg); // light shade ░
        }
    }
    gfx_put_char(x + width - 1, y, ']', fg, bg);
}

void gfx_flip_to_screen(void) {
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            uint16_t entry = (backbuffer[y][x].attr << 8) | (uint8_t)backbuffer[y][x].ch;
            vga_mem[y * CANVAS_WIDTH + x] = entry;
        }
    }
}
