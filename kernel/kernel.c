/* ==============================================================================
 * Eran OS - Kernel Main Entry Point
 * Architecture: 32-bit x86 Protected Mode (Freestanding C)
 * ============================================================================== */

#include "idt.h"
#include "pic.h"
#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../drivers/timer.h"
#include "../drivers/keyboard.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../hal/pci.h"
#include "../fs/vfs.h"

void kmain(void) {
    // 1. Initialize Display and Serial Drivers
    vga_init();
    serial_init(38400);

    // 2. Display System Boot Banner
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("===================================================================\n");
    kprintf("                    ERAN OS - KERNEL v0.2.0                        \n");
    kprintf("===================================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("[KERNEL] Target: x86 32-bit Protected Mode (Base: 0x10000)\n");

    // 3. Initialize Interrupt Descriptor Table (IDT) and Remap 8259 PIC
    kprintf("[KERNEL] Initializing Interrupt Descriptor Table (IDT)...\n");
    idt_init();
    kprintf("[KERNEL] IDT active (32 CPU Exceptions + 16 IRQs remapped).\n");

    // 4. Initialize Hardware Timers and Keyboard
    kprintf("[KERNEL] Initializing 8254 PIT Timer (100 Hz)...\n");
    timer_init(100);

    kprintf("[KERNEL] Initializing PS/2 Keyboard Driver (IRQ 1)...\n");
    keyboard_init();

    // Enable CPU Interrupts (STI)
    __asm__ volatile ("sti");
    kprintf("[KERNEL] Hardware interrupts globally enabled.\n");

    // 5. Initialize Physical Memory Management (Bitmap Allocator)
    kprintf("[KERNEL] Initializing Physical Memory Manager (32 MB RAM)...\n");
    pmm_init(32768); // 32 MB
    kprintf("[KERNEL] PMM initialized. Free: %u KB, Used: %u KB\n",
            pmm_get_free_memory_kb(), pmm_get_used_memory_kb());

    // 6. Initialize Kernel Heap Allocator (4 MB Heap at 2MB Mark)
    kprintf("[KERNEL] Initializing Kernel Dynamic Heap (0x200000, 4 MB)...\n");
    heap_init(0x00200000, 4 * 1024 * 1024);
    kprintf("[KERNEL] Kernel Heap initialized (%u bytes free).\n", heap_get_free());

    // 7. Enumerate PCI Bus and Hardware Controllers
    kprintf("[KERNEL] Scanning PCI Bus for hardware controllers...\n");
    pci_init();
    kprintf("[KERNEL] Discovered %u PCI peripheral devices.\n", pci_get_device_count());

    // 8. Initialize Virtual Filesystem (VFS) and System Mounts
    kprintf("[KERNEL] Mounting Virtual Filesystem (RamFS)...\n");
    vfs_init();
    kprintf("[KERNEL] VFS root '/' mounted with initial system files.\n");

    // 9. Startup Complete - Launch Interactive Shell
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("\n[STATUS] Eran OS kernel initialization completed successfully!\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("Type 'help' for available commands, 'devices' for hardware list.\n\n");

    shell_init();

    // 10. Kernel Interactive Event Loop
    for (;;) {
        shell_run_prompt();
        __asm__ volatile ("hlt"); // Sleep until next hardware interrupt (timer, keyboard)
    }
}
