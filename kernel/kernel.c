/* ==============================================================================
 * Eran OS - Kernel Main Entry Point
 * Architecture: 32-bit x86 Protected Mode (Freestanding C)
 * ============================================================================== */

#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "shell.h"
#include "syscall.h"
#include "elf.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../drivers/timer.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/ata.h"
#include "../drivers/usb.h"
#include "../drivers/e1000.h"
#include "../drivers/audio.h"
#include "../drivers/gfx.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../mm/vmm.h"
#include "../hal/pci.h"
#include "../hal/acpi.h"
#include "../hal/cpu.h"
#include "../hal/longmode.h"
#include "../fs/vfs.h"
#include "../net/net.h"
#include "../net/tcp.h"
#include "../ui/desktop.h"
#include "../user/ai_engine.h"
#include "../user/micro_ai.h"
#include "../process/process.h"

static void task_background_monitor(void) {
    for (;;) {
        process_yield();
    }
}

void kmain(void) {
    // 1. Initialize Display and Serial Drivers
    vga_init();
    serial_init(38400);

    // 2. Display System Boot Banner
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("===================================================================\n");
    kprintf("                    ERAN OS - KERNEL v0.4.0                        \n");
    kprintf("===================================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("[KERNEL] Target: x86 32-bit Protected Mode (Base: 0x10000)\n");

    // 3. Initialize Global Descriptor Table (GDT) & Task State Segment (TSS)
    kprintf("[KERNEL] Initializing GDT (User Selectors & TSS)...\n");
    gdt_init();
    kprintf("[KERNEL] GDT active (Kernel/User Ring 0/3 segments + TSS 0x28).\n");

    // 4. Initialize Interrupt Descriptor Table (IDT) and Remap 8259 PIC
    kprintf("[KERNEL] Initializing Interrupt Descriptor Table (IDT)...\n");
    idt_init();
    kprintf("[KERNEL] IDT active (32 CPU Exceptions + 16 IRQs remapped).\n");

    // 5. Initialize Hardware Timers and Keyboard
    kprintf("[KERNEL] Initializing 8254 PIT Timer (100 Hz)...\n");
    timer_init(100);

    kprintf("[KERNEL] Initializing PS/2 Keyboard Driver (IRQ 1)...\n");
    keyboard_init();

    // Enable CPU Interrupts (STI)
    __asm__ volatile ("sti");
    kprintf("[KERNEL] Hardware interrupts globally enabled.\n");

    // 6. Initialize Physical Memory Management (Bitmap Allocator)
    kprintf("[KERNEL] Initializing Physical Memory Manager (32 MB RAM)...\n");
    pmm_init(32768); // 32 MB
    kprintf("[KERNEL] PMM initialized. Free: %u KB, Used: %u KB\n",
            pmm_get_free_memory_kb(), pmm_get_used_memory_kb());

    // 7. Initialize Kernel Heap Allocator (4 MB Heap at 2MB Mark)
    kprintf("[KERNEL] Initializing Kernel Dynamic Heap (0x200000, 4 MB)...\n");
    heap_init(0x00200000, 4 * 1024 * 1024);
    kprintf("[KERNEL] Kernel Heap initialized (%u bytes free).\n", heap_get_free());

    // 8. Initialize Virtual Memory Manager & Two-Level Paging
    kprintf("[KERNEL] Initializing Virtual Memory Management (Two-Level Paging)...\n");
    vmm_init();
    kprintf("[KERNEL] VMM Paging active (CR0.PG=1, lower 16MB identity mapped).\n");

    // 9. Initialize System Call Gate (INT 0x80)
    kprintf("[KERNEL] Initializing System Call Interface (INT 0x80)...\n");
    syscall_init();
    kprintf("[KERNEL] System call dispatcher active (SYS_YIELD, SYS_EXIT, SYS_WRITE...). \n");

    // 10. Initialize Process Management & Multitasking Scheduler
    kprintf("[KERNEL] Initializing Process Scheduler...\n");
    process_init();
    process_create("bg_monitor", task_background_monitor);
    kprintf("[KERNEL] Multitasking scheduler active (PID 0: kernel_main, PID 1: bg_monitor).\n");

    // 11. Enumerate PCI Bus and Hardware Controllers
    kprintf("[KERNEL] Scanning PCI Bus for hardware controllers...\n");
    pci_init();
    kprintf("[KERNEL] Discovered %u PCI peripheral devices.\n", pci_get_device_count());

    // 12. Initialize Parallel ATA / IDE Storage Driver
    kprintf("[KERNEL] Initializing Parallel ATA / IDE Storage Controller...\n");
    ata_init();
    if (ata_is_detected()) {
        kprintf("[KERNEL] Primary ATA master drive active (PIO Mode 0x1F0).\n");
    } else {
        kprintf("[KERNEL] Primary ATA controller initialized (Drive in standby).\n");
    }

    // 13. Initialize Virtual Filesystem (VFS) and System Mounts
    kprintf("[KERNEL] Mounting Virtual Filesystem (RamFS)...\n");
    vfs_init();
    kprintf("[KERNEL] VFS root '/' mounted with initial system files & /bin/hello.elf.\n");

    // 14. Initialize USB Host Controller and Root Hub
    kprintf("[KERNEL] Initializing USB Subsystem...\n");
    usb_init();
    kprintf("[KERNEL] USB host controller initialized.\n");

    // 15. Initialize Intel e1000 Gigabit NIC & Network Stack
    kprintf("[KERNEL] Initializing Network Stack & Intel e1000 NIC...\n");
    net_init();
    kprintf("[KERNEL] Network initialized (eth0: 192.168.1.50/24).\n");

    // 16. Initialize TCP Stream Protocol Manager
    kprintf("[KERNEL] Initializing TCP Stream Connection Manager...\n");
    tcp_init();
    kprintf("[KERNEL] TCP stack initialized (RFC 793, 4 sockets available).\n");

    // 17. Initialize Audio Subsystem & PC Speaker
    kprintf("[KERNEL] Initializing Audio Subsystem (PIT Ch 2)...\n");
    audio_init();
    kprintf("[KERNEL] Audio tone generator online.\n");

    // 18. Initialize PS/2 Mouse Driver
    kprintf("[KERNEL] Initializing 8042 PS/2 Auxiliary Mouse (IRQ 12)...\n");
    mouse_init();
    kprintf("[KERNEL] PS/2 mouse driver online (streaming mode enabled).\n");

    // 19. Initialize ACPI Power Management & Battery Telemetry
    kprintf("[KERNEL] Initializing ACPI Power Management...\n");
    acpi_init();
    kprintf("[KERNEL] ACPI tables parsed & power management ready.\n");

    // 20. Initialize CPU Architecture & Long Mode Detector
    kprintf("[KERNEL] Probing CPUID Architecture Features...\n");
    cpu_init();
    kprintf("[KERNEL] CPUID probed (%s).\n", cpu_get_info()->brand);

    // 21. Initialize 64-bit Long Mode Transition Paging
    kprintf("[KERNEL] Preparing 64-bit Long Mode (x86_64) Transition Tables...\n");
    longmode_init();
    kprintf("[KERNEL] Long Mode 4-level paging framework primed (PML4 initialized).\n");

    // 22. Initialize Native Offline AI Engine & Embedded Micro-Neural Model
    kprintf("[KERNEL] Initializing Native Offline AI Assistant & Neural Model...\n");
    ai_engine_init();
    micro_ai_init();
    kprintf("[KERNEL] Ring 3 unprivileged AI Engine & Micro-Neural Model active.\n");

    // 23. Startup Complete - Launch Interactive Shell
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("\n[STATUS] Eran OS kernel initialization completed successfully!\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("Type 'help' for commands, 'desktop' for GUI, 'run' for ELF binaries.\n\n");

    // Display prominent capital ERAN OS home screen on VGA display
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("================================================================================\n");
    kprintf("              ______ _____            _   _    ____   _____ \n");
    kprintf("             |  ____|  __ \\     /\\   | \\ | |  / __ \\ / ____|\n");
    kprintf("             | |__  | |__) |   /  \\  |  \\| | | |  | | (___  \n");
    kprintf("             |  __| |  _  /   / /\\ \\ | . ` | | |  | |\\___ \\ \n");
    kprintf("             | |____| | \\ \\  / ____ \\| |\\  | | |__| |____) |\n");
    kprintf("             |______|_|  \\_\\/_/    \\_\\_| \\_|  \\____/|_____/ \n");
    kprintf("================================================================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("                    E R A N   O S   -   v 0 . 4 . 0\n");
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprintf("================================================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf(" [OK] x86 32-bit Protected Mode | 32MB RAM | VFS Root '/' | e1000 NIC | ACPI\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf(" [OK] Ring 3 User Mode Handover | INT 0x80 Syscall Gate   | Embedded Neural AI\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("\n Quick Commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("   desktop          ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("- Launch 2D graphical workstation desktop & mouse\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("   help             ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("- Display all 35+ system shell commands\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("   ai <prompt>      ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("- Query unprivileged native AI assistant\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("   run <file.elf>   ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("- Execute standalone user binary in Ring 3\n\n");

    shell_init();

    // 24. Kernel Interactive Event Loop
    for (;;) {
        shell_run_prompt();
        __asm__ volatile ("hlt"); // Sleep until next hardware interrupt (timer, keyboard, mouse)
    }
}
