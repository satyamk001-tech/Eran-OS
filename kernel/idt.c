#include "idt.h"
#include "pic.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"

struct idt_entry_struct {
    uint16_t base_lo;             // Lower 16 bits of address to jump to
    uint16_t sel;                 // Kernel segment selector
    uint8_t  always0;             // Must always be zero
    uint8_t  flags;               // Flags (Present, Ring, Type)
    uint16_t base_hi;             // Upper 16 bits of address to jump to
} __attribute__((packed));
typedef struct idt_entry_struct idt_entry_t;

struct idt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
typedef struct idt_ptr_struct idt_ptr_t;

static idt_entry_t idt_entries[256];
static idt_ptr_t   idt_ptr;
static isr_t       interrupt_handlers[256];

// External assembly ISR declarations
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void isr128(void); // System Call Software Interrupt (0x80)

// External assembly IRQ declarations
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_lo = (uint16_t)(base & 0xFFFF);
    idt_entries[num].base_hi = (uint16_t)((base >> 16) & 0xFFFF);
    idt_entries[num].sel     = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags   = flags;
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Security Exception",
    "Reserved"
};

void isr_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
        return;
    }

    // Unhandled exception: Trigger kernel panic report
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n=======================================================\n");
    kprintf("                 KERNEL EXCEPTION PANIC                 \n");
    kprintf("=======================================================\n");
    if (regs->int_no < 32) {
        kprintf("Exception [%d]: %s\n", regs->int_no, exception_messages[regs->int_no]);
    } else {
        kprintf("Exception [%d]: Unknown interrupt\n", regs->int_no);
    }
    kprintf("Error Code : %x\n", regs->err_code);
    kprintf("EIP : %x    CS : %x    EFLAGS : %x\n", regs->eip, regs->cs, regs->eflags);
    kprintf("EAX : %x    EBX : %x   ECX : %x    EDX : %x\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
    kprintf("ESI : %x    EDI : %x   EBP : %x    ESP : %x\n", regs->esi, regs->edi, regs->ebp, regs->esp);
    kprintf("=======================================================\n");
    kprintf("System halted. Please reboot the computer.\n");

    serial_puts("\n[KERNEL PANIC] Exception triggered. Halting CPU.\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void irq_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    }

    pic_send_eoi((uint8_t)(regs->int_no - 32));
}

void idt_init(void) {
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, 0, 0, 0);
        interrupt_handlers[i] = 0;
    }

    // Remap PIC: IRQs 0-7 -> 32-39, IRQs 8-15 -> 40-47
    pic_remap(32, 40);

    // Install CPU Exception gates (0-31), Access flag 0x8E = Present, Ring 0, 32-bit Interrupt Gate
    idt_set_gate(0,  (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    // Install Hardware IRQ gates (32-47)
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    // Install System Call Software Interrupt Gate 0x80 (Vector 128)
    // Flags 0xEE: Present=1, DPL=3 (Ring 3 accessible), 32-bit Interrupt Gate
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);

    // Load IDT into CPU
    __asm__ volatile ("lidt (%0)" : : "r"(&idt_ptr));
}
