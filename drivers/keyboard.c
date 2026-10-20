#include "keyboard.h"
#include "../kernel/idt.h"
#include "../kernel/pic.h"
#include "../hal/io.h"

#define KBD_DATA_PORT 0x60
#define KBD_BUFFER_SIZE 256

static volatile char kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint8_t kbd_head = 0;
static volatile uint8_t kbd_tail = 0;

static int shift_active = 0;
static int caps_lock = 0;

static const char scancode_ascii_lower[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char scancode_ascii_upper[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static void keyboard_callback(registers_t *regs) {
    (void)regs;
    uint8_t scancode = inb(KBD_DATA_PORT);

    if (scancode == 0x2A || scancode == 0x36) { // Left / Right Shift down
        shift_active = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) { // Left / Right Shift up
        shift_active = 0;
        return;
    }
    if (scancode == 0x3A) { // Caps lock
        caps_lock = !caps_lock;
        return;
    }

    if (scancode & 0x80) {
        // Key release event: ignore
        return;
    }

    // Key press event
    char c = 0;
    int use_upper = (shift_active ^ caps_lock);

    if (scancode < 128) {
        c = use_upper ? scancode_ascii_upper[scancode] : scancode_ascii_lower[scancode];
    }

    if (c) {
        uint8_t next_head = (uint8_t)((kbd_head + 1) % KBD_BUFFER_SIZE);
        if (next_head != kbd_tail) {
            kbd_buffer[kbd_head] = c;
            kbd_head = next_head;
        }
    }
}

void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_callback);
    pic_clear_mask(1); // Unmask IRQ1
}

int keyboard_has_char(void) {
    return kbd_head != kbd_tail;
}

char keyboard_getc(void) {
    if (kbd_head == kbd_tail) {
        return 0;
    }
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (uint8_t)((kbd_tail + 1) % KBD_BUFFER_SIZE);
    return c;
}

char keyboard_getchar(void) {
    while (!keyboard_has_char()) {
        __asm__ volatile ("hlt");
    }
    return keyboard_getc();
}
