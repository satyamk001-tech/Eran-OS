#include "serial.h"
#include "../hal/io.h"

void serial_init(uint32_t baud) {
    uint16_t divisor = (uint16_t)(115200 / (baud ? baud : 38400));
    outb(COM1_PORT + 1, 0x00);    // Disable interrupts
    outb(COM1_PORT + 3, 0x80);    // Enable DLAB
    outb(COM1_PORT + 0, (uint8_t)(divisor & 0xFF));
    outb(COM1_PORT + 1, (uint8_t)((divisor >> 8) & 0xFF));
    outb(COM1_PORT + 3, 0x03);    // 8N1, clear DLAB
    outb(COM1_PORT + 2, 0xC7);    // Enable FIFO, clear TX/RX, 14-byte threshold
    outb(COM1_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void serial_putc(char c) {
    while ((inb(COM1_PORT + 5) & 0x20) == 0);
    outb(COM1_PORT, (uint8_t)c);
}

void serial_puts(const char *str) {
    if (!str) return;
    while (*str) {
        if (*str == '\n') {
            serial_putc('\r');
        }
        serial_putc(*str++);
    }
}

void serial_write_hex(uint32_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        serial_putc(hex_chars[(val >> i) & 0xF]);
    }
}

void serial_write_dec(uint32_t val) {
    if (val == 0) {
        serial_putc('0');
        return;
    }
    char buf[12];
    int idx = 0;
    while (val > 0) {
        buf[idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int i = idx - 1; i >= 0; i--) {
        serial_putc(buf[i]);
    }
}

int serial_has_rx(void) {
    return inb(COM1_PORT + 5) & 1;
}

char serial_getc(void) {
    while (!serial_has_rx());
    return (char)inb(COM1_PORT);
}
