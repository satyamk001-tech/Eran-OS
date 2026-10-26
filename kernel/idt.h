#ifndef ERAN_KERNEL_IDT_H
#define ERAN_KERNEL_IDT_H

#include <stdint.h>

typedef struct {
    uint32_t ds;                                      // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // Pushed by pusha
    uint32_t int_no, err_code;                        // Interrupt number and error code
    uint32_t eip, cs, eflags, useresp, ss;            // Pushed by processor automatically
} __attribute__((packed)) registers_t;

typedef void (*isr_t)(registers_t *regs);

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void register_interrupt_handler(uint8_t n, isr_t handler);
void isr_handler(registers_t *regs);
registers_t *irq_handler(registers_t *regs);

#endif /* ERAN_KERNEL_IDT_H */
