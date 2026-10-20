#include "vmm.h"
#include "pmm.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"

__attribute__((aligned(4096))) static uint32_t page_directory[1024];
__attribute__((aligned(4096))) static uint32_t page_tables[4][1024]; // 4 tables * 4MB = 16MB identity mapped

static int paging_enabled = 0;

void vmm_page_fault_handler(registers_t *regs) {
    uint32_t faulting_address;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(faulting_address));

    int present  = !(regs->err_code & 0x1); // Page not present
    int rw       = regs->err_code & 0x2;    // Write operation
    int us       = regs->err_code & 0x4;    // Processor was in user-mode
    int reserved = regs->err_code & 0x8;    // Overwritten CPU-reserved bits of page entry?

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n=======================================================\n");
    kprintf("                    PAGE FAULT EXCEPTION (CR2)         \n");
    kprintf("=======================================================\n");
    kprintf("Faulting Linear Address : %x\n", faulting_address);
    kprintf("Error Flags             : [%s] [%s] [%s] [%s]\n",
            present ? "Page-Not-Present" : "Protection-Violation",
            rw ? "Write-Fault" : "Read-Fault",
            us ? "User-Mode" : "Kernel-Mode",
            reserved ? "Reserved-Bit-Overwritten" : "Clean");
    kprintf("Instruction Pointer EIP : %x\n", regs->eip);
    kprintf("=======================================================\n");

    serial_puts("\n[KERNEL PANIC] Unhandled Page Fault Exception. Halting CPU.\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void vmm_init(void) {
    // 1. Initialize Page Directory entries to not present (all 0)
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002; // Supervisor, Read/Write, Not Present
    }

    // 2. Identity map the lower 16 MB of physical memory across 4 page tables
    for (int t = 0; t < 4; t++) {
        for (int p = 0; p < 1024; p++) {
            uint32_t phys_addr = (t * 1024 + p) * PAGE_SIZE;
            page_tables[t][p] = phys_addr | PAGE_PRESENT | PAGE_WRITE | PAGE_USER; // Kernel & user mode, present, writable
        }
        // Link Page Table into Page Directory
        page_directory[t] = ((uint32_t)&page_tables[t][0]) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }

    // 3. Register Page Fault Interrupt Handler (Vector 14)
    register_interrupt_handler(14, vmm_page_fault_handler);

    // 4. Load Page Directory physical address into CR3
    uint32_t pd_phys = (uint32_t)&page_directory[0];
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys));

    // 5. Enable Paging bit (Bit 31) in CR0
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // CR0.PG = 1
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    paging_enabled = 1;
}

void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x03FF;

    if (pd_idx < 4) {
        page_tables[pd_idx][pt_idx] = (phys & ~0xFFF) | (flags & 0xFFF);
        // Invalidate TLB for page
        __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    }
}

void vmm_unmap_page(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x03FF;

    if (pd_idx < 4) {
        page_tables[pd_idx][pt_idx] = 0;
        __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    }
}

void vmm_print_info(void) {
    uint32_t cr0, cr3;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    kprintf("===================================================================\n");
    kprintf("                VIRTUAL MEMORY & TWO-LEVEL PAGING (VMM)             \n");
    kprintf("===================================================================\n");
    kprintf(" Paging Status         : %s (CR0.PG = %d)\n", paging_enabled ? "ENABLED & ACTIVE" : "DISABLED", (cr0 >> 31) & 1);
    kprintf(" Page Directory (CR3)  : %x (1024 Entries)\n", cr3);
    kprintf(" Identity Mapped Space : 0x00000000 - 0x00FFFFFF (16 MB / 4 Tables)\n");
    kprintf(" Page Size / Frame     : 4096 bytes (4 KB standard)\n");
    kprintf(" Page Fault Handler    : Active on IDT Vector 14 (CR2 monitoring)\n");
    kprintf("===================================================================\n");
}
