#include "vga.h"
#include "serial.h"
#include "../hal/io.h"
#include <stdarg.h>

static volatile uint16_t * const VGA_BUFFER = (volatile uint16_t *)0x000B8000;
static size_t vga_row = 0;
static size_t vga_col = 0;
static uint8_t vga_color = 0x07; // Light grey on black

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | ((uint16_t)color << 8);
}

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    vga_row = 0;
    vga_col = 0;
    vga_color = (VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_GREY;
    vga_clear();
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', vga_color);
        }
    }
    vga_row = 0;
    vga_col = 0;
    update_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

static void scroll(void) {
    if (vga_row >= VGA_HEIGHT) {
        for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFFER[y * VGA_WIDTH + x] = VGA_BUFFER[(y + 1) * VGA_WIDTH + x];
            }
        }
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color);
        }
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_backspace(void) {
    if (vga_col > 0) {
        vga_col--;
        VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color);
        update_cursor();
        serial_putc('\b');
        serial_putc(' ');
        serial_putc('\b');
    }
}

void vga_putc(char c) {
    serial_putc(c);

    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        scroll();
        update_cursor();
        return;
    }
    if (c == '\r') {
        vga_col = 0;
        update_cursor();
        return;
    }
    if (c == '\b') {
        vga_backspace();
        return;
    }
    if (c == '\t') {
        size_t next_tab = (vga_col + 4) & ~3;
        while (vga_col < next_tab && vga_col < VGA_WIDTH) {
            VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color);
            vga_col++;
        }
        update_cursor();
        return;
    }

    VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_entry((unsigned char)c, vga_color);
    vga_col++;
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
    }
    scroll();
    update_cursor();
}

void vga_puts(const char *str) {
    if (!str) return;
    while (*str) {
        vga_putc(*str++);
    }
}

void vga_puts_color(const char *str, vga_color_t fg, vga_color_t bg) {
    uint8_t old_color = vga_color;
    vga_set_color(fg, bg);
    vga_puts(str);
    vga_color = old_color;
}

static void print_dec(int32_t val) {
    if (val < 0) {
        vga_putc('-');
        val = -val;
    }
    uint32_t uval = (uint32_t)val;
    char buf[12];
    int idx = 0;
    if (uval == 0) {
        vga_putc('0');
        return;
    }
    while (uval > 0) {
        buf[idx++] = (char)('0' + (uval % 10));
        uval /= 10;
    }
    for (int i = idx - 1; i >= 0; i--) {
        vga_putc(buf[i]);
    }
}

static void print_udec(uint32_t val) {
    char buf[12];
    int idx = 0;
    if (val == 0) {
        vga_putc('0');
        return;
    }
    while (val > 0) {
        buf[idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int i = idx - 1; i >= 0; i--) {
        vga_putc(buf[i]);
    }
}

static void print_hex(uint32_t val, int prefix) {
    if (prefix) {
        vga_puts("0x");
    }
    const char hex_chars[] = "0123456789ABCDEF";
    int leading_zeros = 1;
    for (int i = 28; i >= 0; i -= 4) {
        int nibble = (val >> i) & 0xF;
        if (nibble != 0 || i == 0 || !leading_zeros) {
            leading_zeros = 0;
            vga_putc(hex_chars[nibble]);
        }
    }
}

void kprintf(const char *fmt, ...) {
    if (!fmt) return;
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    vga_puts(s ? s : "(null)");
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    vga_putc(c);
                    break;
                }
                case 'd':
                case 'i': {
                    int32_t d = va_arg(args, int32_t);
                    print_dec(d);
                    break;
                }
                case 'u': {
                    uint32_t u = va_arg(args, uint32_t);
                    print_udec(u);
                    break;
                }
                case 'x':
                case 'X':
                case 'p': {
                    uint32_t x = va_arg(args, uint32_t);
                    print_hex(x, 1);
                    break;
                }
                case '%': {
                    vga_putc('%');
                    break;
                }
                default: {
                    vga_putc('%');
                    vga_putc(*fmt);
                    break;
                }
            }
        } else {
            vga_putc(*fmt);
        }
        fmt++;
    }

    va_end(args);
}
