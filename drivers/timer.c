#include "timer.h"
#include "../kernel/idt.h"
#include "../kernel/pic.h"
#include "../hal/io.h"

static volatile uint32_t timer_ticks = 0;
static uint32_t timer_frequency = 100;

static void timer_callback(registers_t *regs) {
    (void)regs;
    timer_ticks++;
}

void timer_init(uint32_t frequency) {
    timer_frequency = frequency ? frequency : 100;
    register_interrupt_handler(32, timer_callback);

    // The value we send to the PIT is the value to divide its input clock
    // (1193180 Hz) by, to get our required frequency.
    uint32_t divisor = 1193180 / timer_frequency;

    // Send the command byte (channel 0, access mode lobyte/hibyte, mode 3 square wave, binary)
    outb(0x43, 0x36);

    // Divisor must be sent byte-wise, low byte then high byte.
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    // Unmask IRQ0 on PIC
    pic_clear_mask(0);
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

uint32_t timer_get_uptime_seconds(void) {
    return timer_ticks / (timer_frequency ? timer_frequency : 100);
}

void sleep_ticks(uint32_t ticks) {
    uint32_t start = timer_ticks;
    while ((timer_ticks - start) < ticks) {
        __asm__ volatile ("hlt");
    }
}

void sleep_ms(uint32_t ms) {
    uint32_t ticks = (ms * timer_frequency) / 1000;
    if (ticks == 0) ticks = 1;
    sleep_ticks(ticks);
}
