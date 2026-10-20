#include "mouse.h"
#include "../hal/io.h"
#include "../kernel/idt.h"
#include "../drivers/vga.h"

#define MOUSE_PORT_DATA    0x60
#define MOUSE_PORT_STATUS  0x64
#define MOUSE_PORT_CMD     0x64

static mouse_state_t mouse_state;
static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3];
static int mouse_moved = 0;

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        // Wait until output buffer is full (data ready to read)
        while (timeout--) {
            if (inb(MOUSE_PORT_STATUS) & 1) return;
        }
    } else {
        // Wait until input buffer is empty (ready to send)
        while (timeout--) {
            if (!(inb(MOUSE_PORT_STATUS) & 2)) return;
        }
    }
}

static void mouse_write(uint8_t byte) {
    // Send command to tell controller that next byte is for auxiliary device (mouse)
    mouse_wait(1);
    outb(MOUSE_PORT_CMD, 0xD4);
    mouse_wait(1);
    outb(MOUSE_PORT_DATA, byte);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(MOUSE_PORT_DATA);
}

static void mouse_handler(registers_t *regs) {
    (void)regs;
    uint8_t status = inb(MOUSE_PORT_STATUS);
    if (!(status & 0x20)) {
        // Not from auxiliary mouse device
        return;
    }

    uint8_t data = inb(MOUSE_PORT_DATA);

    switch (mouse_cycle) {
        case 0:
            // Byte 0 must have bit 3 set to 1
            if ((data & 0x08) == 0x08) {
                mouse_bytes[0] = data;
                mouse_cycle = 1;
            }
            break;

        case 1:
            mouse_bytes[1] = data;
            mouse_cycle = 2;
            break;

        case 2:
            mouse_bytes[2] = data;
            mouse_cycle = 0;

            // Decode 3-byte packet
            uint8_t flags = mouse_bytes[0];
            int32_t dx = (int32_t)mouse_bytes[1];
            int32_t dy = (int32_t)mouse_bytes[2];

            if (flags & 0x10) dx |= 0xFFFFFF00; // Sign extend X
            if (flags & 0x20) dy |= 0xFFFFFF00; // Sign extend Y

            mouse_state.buttons = flags & 0x07;
            mouse_state.delta_x = dx;
            mouse_state.delta_y = dy;

            // Adjust screen coordinates (VGA 80x25 characters)
            mouse_state.x += (dx / 4);
            mouse_state.y -= (dy / 4); // Y is inverted in screen space

            if (mouse_state.x < 0) mouse_state.x = 0;
            if (mouse_state.x >= 80) mouse_state.x = 79;
            if (mouse_state.y < 0) mouse_state.y = 0;
            if (mouse_state.y >= 25) mouse_state.y = 24;

            mouse_moved = 1;
            break;
    }
}

void mouse_init(void) {
    mouse_state.x = 40;
    mouse_state.y = 12;
    mouse_state.buttons = 0;
    mouse_state.delta_x = 0;
    mouse_state.delta_y = 0;
    mouse_cycle = 0;
    mouse_moved = 0;

    // 1. Enable auxiliary mouse device on 8042 controller
    mouse_wait(1);
    outb(MOUSE_PORT_CMD, 0xA8);

    // 2. Read Compaq Status Byte
    mouse_wait(1);
    outb(MOUSE_PORT_CMD, 0x20);
    mouse_wait(0);
    uint8_t status = inb(MOUSE_PORT_DATA);

    // 3. Set bit 1 (enable IRQ 12), clear bit 5 (disable mouse clock inhibit)
    status |= 0x02;
    status &= ~0x20;

    // 4. Write back Compaq Status Byte
    mouse_wait(1);
    outb(MOUSE_PORT_CMD, 0x60);
    mouse_wait(1);
    outb(MOUSE_PORT_DATA, status);

    // 5. Send Set Defaults (0xF6) & Enable Data Reporting (0xF4)
    mouse_write(0xF6);
    mouse_read(); // ACK 0xFA

    mouse_write(0xF4);
    mouse_read(); // ACK 0xFA

    // 6. Register mouse interrupt on IRQ 12 (Vector 44)
    register_interrupt_handler(44, mouse_handler);
}

const mouse_state_t *mouse_get_state(void) {
    return &mouse_state;
}

int mouse_has_moved(void) {
    int ret = mouse_moved;
    mouse_moved = 0;
    return ret;
}

void mouse_print_status(void) {
    kprintf("===================================================================\n");
    kprintf("                    PS/2 AUXILIARY MOUSE CONTROLLER                \n");
    kprintf("===================================================================\n");
    kprintf(" Hardware Interface    : 8042 Auxiliary PS/2 Port (IRQ 12)\n");
    kprintf(" Packet Protocol       : Standard 3-Byte Relative Motion Stream\n");
    kprintf(" Current Cursor (X, Y) : (%d, %d) [Grid 80x25]\n", mouse_state.x, mouse_state.y);
    kprintf(" Buttons State         : [Left:%s] [Right:%s] [Middle:%s]\n",
            (mouse_state.buttons & MOUSE_BTN_LEFT) ? "PRESSED" : "UP",
            (mouse_state.buttons & MOUSE_BTN_RIGHT) ? "PRESSED" : "UP",
            (mouse_state.buttons & MOUSE_BTN_MIDDLE) ? "PRESSED" : "UP");
    kprintf(" Delta Movement        : dX = %d, dY = %d\n", mouse_state.delta_x, mouse_state.delta_y);
    kprintf("===================================================================\n");
}
