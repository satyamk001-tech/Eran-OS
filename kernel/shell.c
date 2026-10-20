#include "shell.h"
#include "syscall.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/serial.h"
#include "../drivers/ata.h"
#include "../drivers/usb.h"
#include "../drivers/e1000.h"
#include "../drivers/audio.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../mm/vmm.h"
#include "../hal/pci.h"
#include "../hal/io.h"
#include "../hal/acpi.h"
#include "../hal/cpu.h"
#include "../fs/vfs.h"
#include "../net/net.h"
#include "../ui/desktop.h"
#include "../user/ai_engine.h"
#include "../process/process.h"

#define CMD_BUFFER_SIZE 128

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static void cmd_help(void) {
    kprintf("===================================================================\n");
    kprintf("                       ERAN OS SHELL HELP                          \n");
    kprintf("===================================================================\n");
    kprintf(" help              - Display this list of supported commands\n");
    kprintf(" version           - Show Eran OS kernel build and architecture\n");
    kprintf(" clear             - Clear the VGA text console\n");
    kprintf(" cpuinfo           - Display processor state and segment selectors\n");
    kprintf(" cpufeatures       - Enumerate CPUID instruction sets & 64-bit Long Mode\n");
    kprintf(" meminfo           - Display physical page frames and heap statistics\n");
    kprintf(" vmm               - Virtual Memory Manager & two-level paging status\n");
    kprintf(" uptime            - Show system uptime from PIT hardware timer\n");
    kprintf(" processes         - Enumerate active tasks and scheduler state\n");
    kprintf(" atainfo           - Display Parallel ATA / IDE storage status\n");
    kprintf(" usb               - USB Host Controller & enumerated devices\n");
    kprintf(" devices           - Enumerate all detected PCI hardware controllers\n");
    kprintf(" ifconfig          - Network interface configuration (IP/MAC/packets)\n");
    kprintf(" ping <ip>         - Send ICMP Echo Request to IP address\n");
    kprintf(" beep              - Play 880 Hz system tone via PC Speaker\n");
    kprintf(" chime             - Play synthesized 4-note melodic chime\n");
    kprintf(" desktop           - Launch 2D graphical desktop environment\n");
    kprintf(" acpi              - Query ACPI power management & RSDP tables\n");
    kprintf(" battery           - Display laptop battery charge and AC line status\n");
    kprintf(" ai <prompt>       - Run offline natural language AI assistant\n");
    kprintf(" ls [path]         - List files and directories from the VFS\n");
    kprintf(" cat <path>        - Display text file contents\n");
    kprintf(" echo <text>       - Print text to screen and serial log\n");
    kprintf(" syscall           - Test INT 0x80 system call dispatch\n");
    kprintf(" reboot            - Perform warm hardware reboot via ACPI / 8042\n");
    kprintf(" shutdown          - Power off the system (ACPI / QEMU)\n");
    kprintf(" panic             - Trigger an intentional CPU exception test\n");
    kprintf("===================================================================\n");
}

static void cmd_version(void) {
    kprintf("Eran OS Kernel v0.3.0 (i386-pc-freestanding)\n");
    kprintf("Architect: Eran OS Principal Systems Team\n");
    kprintf("Target   : x86 32-bit Protected Mode (Ring 0 / Ring 3 Isolated)\n");
    kprintf("Features : GDT+TSS, IDT (32 Exceptions + 16 IRQs), PIC 8259, PIT 100Hz,\n");
    kprintf("           PS/2 Keyboard, 16550 Serial UART, Direct VGA MMIO, Bitmap PMM,\n");
    kprintf("           Two-Level Paging (VMM), Kernel Heap, ATA PIO, RamFS VFS,\n");
    kprintf("           Intel e1000 NIC, IPv4/ARP/ICMP, PC Speaker Audio, Desktop UI,\n");
    kprintf("           ACPI Power/Battery, USB Discovery, Native Offline AI Engine.\n");
}

static void cmd_cpuinfo(void) {
    uint32_t cr0, eflags;
    uint16_t cs, ds, ss;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("pushfl; popl %0" : "=r"(eflags));
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));

    kprintf("Processor State (x86 32-bit Protected Mode):\n");
    kprintf("  CR0 Register  : %x (PE=%d, PG=%d)\n", cr0, (cr0 & 1), (cr0 >> 31) & 1);
    kprintf("  EFLAGS        : %x (IF=%d, IOPL=%d)\n", eflags, (eflags >> 9) & 1, (eflags >> 12) & 3);
    kprintf("  Segment Selectors: CS=%x  DS=%x  SS=%x\n", cs, ds, ss);
}

static void cmd_meminfo(void) {
    uint32_t total_kb = pmm_get_total_memory_kb();
    uint32_t free_kb  = pmm_get_free_memory_kb();
    uint32_t used_kb  = pmm_get_used_memory_kb();

    kprintf("Physical Memory Manager (PMM - 4KB Frames):\n");
    kprintf("  Total RAM : %u KB (%u MB)\n", total_kb, total_kb / 1024);
    kprintf("  Used RAM  : %u KB (%u MB)\n", used_kb, used_kb / 1024);
    kprintf("  Free RAM  : %u KB (%u MB)\n", free_kb, free_kb / 1024);

    size_t heap_used = heap_get_used();
    size_t heap_free = heap_get_free();
    kprintf("Kernel Dynamic Heap Allocator (0x200000 - 0x600000):\n");
    kprintf("  Heap Used : %u bytes\n", heap_used);
    kprintf("  Heap Free : %u bytes\n", heap_free);
}

static void cmd_uptime(void) {
    uint32_t sec = timer_get_uptime_seconds();
    uint32_t ticks = timer_get_ticks();
    kprintf("System Uptime: %u seconds (%u hardware timer ticks @ 100Hz)\n", sec, ticks);
}

static void cmd_panic(void) {
    kprintf("Triggering intentional divide-by-zero exception...\n");
    volatile int a = 42;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}

void shell_execute(const char *cmdline) {
    // Skip leading whitespace
    while (*cmdline == ' ' || *cmdline == '\t') cmdline++;
    if (*cmdline == '\0') return;

    if (strcmp(cmdline, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmdline, "version") == 0) {
        cmd_version();
    } else if (strcmp(cmdline, "clear") == 0) {
        vga_clear();
    } else if (strcmp(cmdline, "cpuinfo") == 0) {
        cmd_cpuinfo();
    } else if (strcmp(cmdline, "cpufeatures") == 0 || strcmp(cmdline, "cpuid") == 0) {
        cpu_print_features();
    } else if (strcmp(cmdline, "meminfo") == 0) {
        cmd_meminfo();
    } else if (strcmp(cmdline, "vmm") == 0) {
        vmm_print_info();
    } else if (strcmp(cmdline, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(cmdline, "processes") == 0) {
        process_list();
    } else if (strcmp(cmdline, "atainfo") == 0) {
        ata_print_status();
    } else if (strcmp(cmdline, "usb") == 0) {
        usb_print_status();
    } else if (strcmp(cmdline, "devices") == 0) {
        pci_print_devices();
    } else if (strcmp(cmdline, "ifconfig") == 0) {
        net_print_ifconfig();
    } else if (strncmp(cmdline, "ping", 4) == 0) {
        const char *arg = cmdline + 4;
        while (*arg == ' ') arg++;
        if (*arg == '\0') arg = "192.168.1.1";
        net_ping(arg);
    } else if (strcmp(cmdline, "beep") == 0) {
        audio_beep();
    } else if (strcmp(cmdline, "chime") == 0) {
        audio_chime();
    } else if (strcmp(cmdline, "desktop") == 0) {
        desktop_interactive();
    } else if (strcmp(cmdline, "acpi") == 0) {
        acpi_print_status();
    } else if (strcmp(cmdline, "battery") == 0) {
        acpi_print_battery();
    } else if (strncmp(cmdline, "ai", 2) == 0 && (cmdline[2] == ' ' || cmdline[2] == '\0')) {
        const char *arg = cmdline + 2;
        while (*arg == ' ') arg++;
        ai_process_query(arg);
    } else if (strcmp(cmdline, "syscall") == 0) {
        int32_t pid = sys_getpid();
        int32_t sec = sys_uptime();
        int32_t devs = sys_devinfo();
        kprintf("[SYSCALL TEST] INT 0x80 successfully executed:\n");
        kprintf("  SYS_GETPID -> PID: %d\n", pid);
        kprintf("  SYS_UPTIME -> Uptime: %d seconds\n", sec);
        kprintf("  SYS_DEVINFO -> PCI Devices: %d\n", devs);
        sys_write("[SYSCALL TEST] String printed via sys_write()!\n");
    } else if (strcmp(cmdline, "reboot") == 0) {
        acpi_reboot();
    } else if (strcmp(cmdline, "shutdown") == 0) {
        acpi_poweroff();
    } else if (strcmp(cmdline, "panic") == 0) {
        cmd_panic();
    } else if (strncmp(cmdline, "ls", 2) == 0) {
        const char *arg = cmdline + 2;
        while (*arg == ' ') arg++;
        if (*arg == '\0') arg = "/";
        vfs_list_dir(arg);
    } else if (strncmp(cmdline, "cat", 3) == 0) {
        const char *arg = cmdline + 3;
        while (*arg == ' ') arg++;
        if (*arg == '\0') {
            kprintf("Usage: cat <filename>\n");
        } else {
            char buffer[512];
            int bytes = vfs_read_file(arg, buffer, sizeof(buffer));
            if (bytes >= 0) {
                kprintf("%s\n", buffer);
            } else {
                kprintf("cat: File not found: %s\n", arg);
            }
        }
    } else if (strncmp(cmdline, "echo", 4) == 0) {
        const char *arg = cmdline + 4;
        while (*arg == ' ') arg++;
        kprintf("%s\n", arg);
    } else {
        kprintf("eran: command not found: '%s'. Type 'help' for commands.\n", cmdline);
    }
}

void shell_init(void) {
    vga_puts_color("eran> ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
}

void shell_run_prompt(void) {
    static char buffer[CMD_BUFFER_SIZE];
    static size_t buf_idx = 0;

    char c = 0;
    if (keyboard_has_char()) {
        c = keyboard_getc();
    } else if (serial_has_rx()) {
        c = serial_getc();
    }

    if (c != 0) {
        if (c == '\n' || c == '\r') {
            vga_putc('\n');
            buffer[buf_idx] = '\0';
            shell_execute(buffer);
            buf_idx = 0;
            vga_puts_color("eran> ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        } else if (c == '\b' || c == 127) {
            if (buf_idx > 0) {
                buf_idx--;
                vga_backspace();
            }
        } else if (c >= 32 && c < 127) {
            if (buf_idx < CMD_BUFFER_SIZE - 1) {
                buffer[buf_idx++] = c;
                vga_putc(c);
            }
        }
    }
}
