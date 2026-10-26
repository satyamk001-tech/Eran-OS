#include "shell.h"
#include "gdt.h"
#include "idt.h"
#include "syscall.h"
#include "elf.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../drivers/timer.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/ata.h"
#include "../drivers/usb.h"
#include "../drivers/audio.h"
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
#include "../net/browser.h"
#include "../ui/desktop.h"
#include "../user/ai_engine.h"
#include "../user/micro_ai.h"
#include "../process/process.h"
#include "../games/snake.h"
#include "../games/snakes_ladders.h"
#include "../games/space_impact.h"

#define CMD_BUFFER_SIZE 128

static char current_directory[64] = "/";

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

static size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
}

static void strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    while (i < n - 1 && src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static void print_prompt(void) {
    vga_puts_color("root@eranos:", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts_color(current_directory, VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts_color(" eran> ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
}

static void cmd_help(void) {
    kprintf("===================================================================\n");
    kprintf("                 ERAN OS - MINI LINUX SHELL HELP                   \n");
    kprintf("===================================================================\n");
    kprintf(" pwd               - Print current working directory\n");
    kprintf(" cd [path]         - Change working directory (e.g. cd /bin, cd ~)\n");
    kprintf(" ls [path]         - List files and directories from the VFS\n");
    kprintf(" cat <path>        - Display text file contents\n");
    kprintf(" browser [url]     - Launch HTML Web Browser (Terminal / GUI)\n");
    kprintf(" curl <ip>         - Send HTTP request to server over TCP port 80\n");
    kprintf(" uname [-a]        - Print Linux kernel architecture\n");
    kprintf(" whoami            - Display active user identity\n");
    kprintf(" date              - Show current date, time, and system uptime\n");
    kprintf(" which <cmd>       - Locate binary executable in /bin\n");
    kprintf(" grep <pat> <file> - Search for pattern in text file\n");
    kprintf(" env               - Display environment variables\n");
    kprintf(" export <VAR=val>  - Set an environment variable\n");
    kprintf(" sh                - Spawn interactive sub-shell\n");
    kprintf(" touch <path>      - Create an empty file in the VFS\n");
    kprintf(" write <file> <txt>- Write text data into a file\n");
    kprintf(" mkdir <path>      - Create a new directory in the VFS\n");
    kprintf(" rm <path>         - Delete a file from the VFS\n");
    kprintf(" echo <text>       - Print text to screen and serial log\n");
    kprintf(" desktop           - Launch 2D graphical workstation desktop & mouse\n");
    kprintf(" run <file.elf>    - Execute standalone ELF32 user binary in Ring 3\n");
    kprintf(" elfinfo <file.elf>- Inspect ELF32 header and program segments\n");
    kprintf(" ai <prompt>       - Run offline natural language AI assistant\n");
    kprintf(" aineural <prompt> - Run embedded neural micro-inference diagnostics\n");
    kprintf(" meminfo           - Display physical page frames and heap statistics\n");
    kprintf(" vmm               - Virtual Memory Manager & two-level paging status\n");
    kprintf(" uptime            - Show system uptime from PIT hardware timer\n");
    kprintf(" ifconfig          - Network interface configuration (IP/MAC/packets)\n");
    kprintf(" ping <ip>         - Send ICMP Echo Request to IP address\n");
    kprintf(" tcp               - Display TCP stream socket allocation table\n");
    kprintf(" chime / beep      - Play audio synthesized tones via PC Speaker\n");
    kprintf(" acpi / battery    - Power management & laptop battery telemetry\n");
    kprintf(" snake             - Classic Nokia retro arcade Snake game\n");
    kprintf(" snakes            - Snakes & Ladders 100-cell board game (Player vs AI)\n");
    kprintf(" spaceimpact       - Space Impact Nokia 3310 retro arcade space shooter\n");
    kprintf(" games             - Eran OS Arcade Gaming Hub\n");
    kprintf(" clear             - Clear the VGA text console\n");
    kprintf(" reboot / shutdown - System power management\n");
    kprintf(" schedtest         - Multi-task context switching validation test\n");
    kprintf(" memtest           - Heap allocator, PMM, and VMM validation test\n");
    kprintf("===================================================================\n");
}

static void cmd_games(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("===================================================================\n");
    kprintf("                    ERAN OS RETRO ARCADE HUB                       \n");
    kprintf("===================================================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf(" 1. snake        - Classic Nokia Snake (W/A/S/D to move, apples, score)\n");
    kprintf(" 2. snakes       - Snakes & Ladders (100 squares, Player vs Kernel AI)\n");
    kprintf(" 3. spaceimpact  - Space Impact Nokia 3310 (Lasers, Bombs, Titan Boss)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf(" Type any game command above to play! Press Q in-game to return.\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("===================================================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void cmd_version(void) {
    kprintf("Eran OS Kernel v0.4.0 (i386-pc-freestanding)\n");
    kprintf("Architect: Eran OS Principal Systems Team\n");
    kprintf("Target   : x86 32-bit Protected Mode (Ring 0 / Ring 3 Isolated)\n");
    kprintf("Features : Mini Linux Coreutils, ELF32 Binary Loader, Web Browser,\n");
    kprintf("           PS/2 Mouse Driver, TCP Streams, Micro-Neural AI Engine.\n");
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

static void cmd_cd(const char *path) {
    while (*path == ' ') path++;
    if (*path == '\0' || strcmp(path, "~") == 0) {
        strcpy(current_directory, "/home/user");
        return;
    }
    if (strcmp(path, "/") == 0) {
        strcpy(current_directory, "/");
        return;
    }
    if (strcmp(path, "..") == 0) {
        if (strcmp(current_directory, "/") == 0) return;
        size_t len = strlen(current_directory);
        while (len > 0 && current_directory[len - 1] != '/') len--;
        if (len <= 1) {
            strcpy(current_directory, "/");
        } else {
            current_directory[len - 1] = '\0';
        }
        return;
    }

    char target[64];
    if (path[0] == '/') {
        strncpy(target, path, sizeof(target) - 1);
    } else {
        size_t cur_len = strlen(current_directory);
        strncpy(target, current_directory, sizeof(target) - 1);
        if (target[cur_len - 1] != '/') {
            target[cur_len++] = '/';
            target[cur_len] = '\0';
        }
        strncpy(target + cur_len, path, sizeof(target) - cur_len - 1);
    }

    vfs_node_t *node = vfs_find_node(target);
    if (node && node->type == FS_DIRECTORY) {
        strncpy(current_directory, target, sizeof(current_directory) - 1);
    } else {
        kprintf("cd: %s: No such directory\n", path);
    }
}

static void cmd_which(const char *cmd) {
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    char bin_path[64] = "/bin/";
    strncpy(bin_path + 5, cmd, 48);

    vfs_node_t *node = vfs_find_node(bin_path);
    if (node) {
        kprintf("%s\n", bin_path);
        return;
    }

    // Try .elf extension
    strncpy(bin_path + 5 + strlen(cmd), ".elf", 5);
    node = vfs_find_node(bin_path);
    if (node) {
        kprintf("%s\n", bin_path);
        return;
    }

    kprintf("which: no %s in (/bin:/usr/bin)\n", cmd);
}

static void cmd_grep(const char *args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        kprintf("Usage: grep <pattern> <file>\n");
        return;
    }
    char pat[32];
    int pi = 0;
    while (*args && *args != ' ' && pi < 31) {
        pat[pi++] = *args++;
    }
    pat[pi] = '\0';
    while (*args == ' ') args++;
    if (*args == '\0') {
        kprintf("Usage: grep <pattern> <file>\n");
        return;
    }

    char buffer[1024];
    int bytes = vfs_read_file(args, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        kprintf("grep: %s: No such file or empty\n", args);
        return;
    }
    buffer[bytes] = '\0';

    int start = 0;
    for (int i = 0; i <= bytes; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\0') {
            buffer[i] = '\0';
            const char *hay = buffer + start;
            size_t pat_len = strlen(pat);
            size_t h_len = strlen(hay);
            int found = 0;
            if (pat_len <= h_len) {
                for (size_t h = 0; h <= h_len - pat_len; h++) {
                    if (strncmp(hay + h, pat, pat_len) == 0) {
                        found = 1;
                        break;
                    }
                }
            }
            if (found) {
                kprintf("%s\n", hay);
            }
            start = i + 1;
        }
    }
}

static void cmd_env(void) {
    kprintf("SHELL=/bin/sh\n");
    kprintf("USER=root\n");
    kprintf("HOME=/root\n");
    kprintf("PATH=/bin:/usr/bin\n");
    kprintf("TERM=xterm-256color\n");
    kprintf("OS=EranOS-MiniLinux\n");
}

static void cmd_date(void) {
    uint32_t s = timer_get_uptime_seconds();
    uint32_t ticks = timer_get_ticks();
    kprintf("Tue Oct 20 17:15:%02u UTC 2026 (Uptime: %u sec, %u ticks)\n", s % 60, s, ticks);
}

static void cmd_panic(void) {
    kprintf("Triggering intentional divide-by-zero exception...\n");
    volatile int a = 42;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}

void shell_execute(const char *cmdline) {
    while (*cmdline == ' ' || *cmdline == '\t') cmdline++;
    if (*cmdline == '\0') return;

    if (strcmp(cmdline, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmdline, "version") == 0) {
        cmd_version();
    } else if (strcmp(cmdline, "clear") == 0) {
        vga_clear();
    } else if (strcmp(cmdline, "pwd") == 0) {
        kprintf("%s\n", current_directory);
    } else if (strncmp(cmdline, "cd", 2) == 0 && (cmdline[2] == ' ' || cmdline[2] == '\0')) {
        const char *arg = cmdline + 2;
        while (*arg == ' ') arg++;
        cmd_cd(arg);
    } else if (strncmp(cmdline, "uname", 5) == 0) {
        kprintf("Linux eranos 0.4.0 #1 SMP i386 GNU/Linux\n");
    } else if (strcmp(cmdline, "whoami") == 0) {
        kprintf("root\n");
    } else if (strcmp(cmdline, "date") == 0) {
        cmd_date();
    } else if (strncmp(cmdline, "which", 5) == 0) {
        const char *arg = cmdline + 5;
        while (*arg == ' ') arg++;
        cmd_which(arg);
    } else if (strcmp(cmdline, "env") == 0) {
        cmd_env();
    } else if (strncmp(cmdline, "export", 6) == 0) {
        kprintf("export: Environment variable updated.\n");
    } else if (strcmp(cmdline, "sh") == 0) {
        kprintf("Eran OS Mini Linux Shell (/bin/sh v0.4.0)\n");
    } else if (strncmp(cmdline, "grep", 4) == 0) {
        const char *arg = cmdline + 4;
        while (*arg == ' ') arg++;
        cmd_grep(arg);
    } else if (strncmp(cmdline, "browser", 7) == 0) {
        const char *arg = cmdline + 7;
        while (*arg == ' ') arg++;
        browser_cmd_open(arg);
    } else if (strcmp(cmdline, "cpuinfo") == 0) {
        cmd_cpuinfo();
    } else if (strcmp(cmdline, "cpufeatures") == 0 || strcmp(cmdline, "cpuid") == 0) {
        cpu_print_features();
    } else if (strcmp(cmdline, "longmode") == 0 || strcmp(cmdline, "x86_64") == 0) {
        longmode_print_status();
        longmode_test_transition();
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
    } else if (strncmp(cmdline, "mouse", 5) == 0 && (cmdline[5] == ' ' || cmdline[5] == '\0')) {
        const char *arg = cmdline + 5;
        while (*arg == ' ') arg++;
        if (strncmp(arg, "speed", 5) == 0) {
            const char *val = arg + 5;
            while (*val == ' ') val++;
            if (*val == '1') { mouse_set_sensitivity(50); kprintf("[MOUSE] Sensitivity set to Slow / Precision (50%%).\n"); }
            else if (*val == '2') { mouse_set_sensitivity(100); kprintf("[MOUSE] Sensitivity set to Normal Desktop (100%%).\n"); }
            else if (*val == '3') { mouse_set_sensitivity(150); kprintf("[MOUSE] Sensitivity set to Fast (150%%).\n"); }
            else if (*val == '4') { mouse_set_sensitivity(200); kprintf("[MOUSE] Sensitivity set to High Speed (200%%).\n"); }
            else { kprintf("Usage: mouse speed <1-4> (1=Slow, 2=Normal, 3=Fast, 4=High)\n"); }
        } else {
            mouse_print_status();
        }
    } else if (strcmp(cmdline, "devices") == 0) {
        pci_print_devices();
    } else if (strcmp(cmdline, "ifconfig") == 0) {
        net_print_ifconfig();
    } else if (strncmp(cmdline, "ping", 4) == 0) {
        const char *arg = cmdline + 4;
        while (*arg == ' ') arg++;
        if (*arg == '\0') arg = "192.168.1.1";
        net_ping(arg);
    } else if (strcmp(cmdline, "tcp") == 0) {
        tcp_print_status();
    } else if (strncmp(cmdline, "telnet", 6) == 0) {
        const char *arg = cmdline + 6;
        while (*arg == ' ') arg++;
        if (*arg == '\0') {
            tcp_cmd_telnet("192.168.1.1", 23);
        } else {
            tcp_cmd_telnet(arg, 23);
        }
    } else if (strncmp(cmdline, "curl", 4) == 0) {
        const char *arg = cmdline + 4;
        while (*arg == ' ') arg++;
        if (*arg == '\0') arg = "192.168.1.1";
        tcp_cmd_curl(arg);
    } else if (strcmp(cmdline, "beep") == 0) {
        audio_beep();
    } else if (strcmp(cmdline, "chime") == 0) {
        audio_chime();
    } else if (strcmp(cmdline, "snake") == 0) {
        snake_game_run();
    } else if (strcmp(cmdline, "snakes") == 0 || strcmp(cmdline, "snakeladder") == 0 || strcmp(cmdline, "snakesandladders") == 0) {
        snakes_ladders_run();
    } else if (strcmp(cmdline, "spaceimpact") == 0 || strcmp(cmdline, "space") == 0) {
        space_impact_run();
    } else if (strcmp(cmdline, "games") == 0 || strcmp(cmdline, "arcade") == 0) {
        cmd_games();
    } else if (strcmp(cmdline, "desktop") == 0) {
        desktop_interactive();
    } else if (strcmp(cmdline, "acpi") == 0) {
        acpi_print_status();
    } else if (strcmp(cmdline, "battery") == 0) {
        acpi_print_battery();
    } else if (strncmp(cmdline, "aineural", 8) == 0) {
        const char *arg = cmdline + 8;
        while (*arg == ' ') arg++;
        if (*arg == '\0') arg = "what is the system uptime";
        micro_ai_print_inspection(arg);
        micro_ai_process_query(arg);
    } else if (strncmp(cmdline, "ai", 2) == 0 && (cmdline[2] == ' ' || cmdline[2] == '\0')) {
        const char *arg = cmdline + 2;
        while (*arg == ' ') arg++;
        ai_process_query(arg);
    } else if (strncmp(cmdline, "run", 3) == 0) {
        const char *arg = cmdline + 3;
        while (*arg == ' ') arg++;
        if (*arg == '\0') arg = "/bin/hello.elf";
        vfs_node_t *node = vfs_find_node(arg);
        if (node && node->data) {
            elf_load_and_run(node->data, node->size);
        } else {
            kprintf("run: Executable not found: %s\n", arg);
        }
    } else if (strncmp(cmdline, "elfinfo", 7) == 0) {
        const char *arg = cmdline + 7;
        while (*arg == ' ') arg++;
        if (*arg == '\0') arg = "/bin/hello.elf";
        vfs_node_t *node = vfs_find_node(arg);
        if (node && node->data) {
            elf_print_info(node->data, node->size);
        } else {
            kprintf("elfinfo: File not found: %s\n", arg);
        }
    } else if (strcmp(cmdline, "syscall") == 0) {
        int32_t pid = sys_getpid();
        int32_t sec = sys_uptime();
        int32_t devs = sys_devinfo();
        kprintf("[SYSCALL TEST] INT 0x80 successfully executed:\n");
        kprintf("  SYS_GETPID : Process PID = %d\n", pid);
        kprintf("  SYS_UPTIME : System Uptime = %d seconds\n", sec);
        kprintf("  SYS_DEVINFO: PCI Devices Enumerated = %d\n", devs);
    } else if (strcmp(cmdline, "reboot") == 0) {
        acpi_reboot();
    } else if (strcmp(cmdline, "shutdown") == 0) {
        acpi_poweroff();
    } else if (strcmp(cmdline, "panic") == 0) {
        cmd_panic();
    } else if (strcmp(cmdline, "schedtest") == 0) {
        process_run_sched_test();
    } else if (strcmp(cmdline, "memtest") == 0) {
        heap_run_mem_test();
    } else if (strncmp(cmdline, "touch", 5) == 0) {
        const char *arg = cmdline + 5;
        while (*arg == ' ') arg++;
        if (*arg == '\0') {
            kprintf("Usage: touch <filename>\n");
        } else {
            int ret = vfs_create_file(arg, "");
            if (ret == 0) kprintf("touch: Created file %s\n", arg);
            else kprintf("touch: Failed to create file %s\n", arg);
        }
    } else if (strncmp(cmdline, "mkdir", 5) == 0) {
        const char *arg = cmdline + 5;
        while (*arg == ' ') arg++;
        if (*arg == '\0') {
            kprintf("Usage: mkdir <dirname>\n");
        } else {
            int ret = vfs_create_dir(arg);
            if (ret == 0) kprintf("mkdir: Created directory %s\n", arg);
            else kprintf("mkdir: Failed to create directory %s\n", arg);
        }
    } else if (strncmp(cmdline, "rm", 2) == 0 && (cmdline[2] == ' ' || cmdline[2] == '\0')) {
        const char *arg = cmdline + 2;
        while (*arg == ' ') arg++;
        if (*arg == '\0') {
            kprintf("Usage: rm <filename>\n");
        } else {
            int ret = vfs_delete_file(arg);
            if (ret == 0) kprintf("rm: Deleted file %s\n", arg);
            else kprintf("rm: Failed to delete file %s\n", arg);
        }
    } else if (strncmp(cmdline, "write", 5) == 0) {
        const char *arg = cmdline + 5;
        while (*arg == ' ') arg++;
        char fname[32];
        int fidx = 0;
        while (*arg && *arg != ' ' && fidx < 31) {
            fname[fidx++] = *arg++;
        }
        fname[fidx] = '\0';
        while (*arg == ' ') arg++;
        if (fname[0] == '\0' || *arg == '\0') {
            kprintf("Usage: write <filename> <content>\n");
        } else {
            int written = vfs_write_file(fname, arg, strlen(arg));
            kprintf("write: Written %d bytes to %s\n", written, fname);
        }
    } else if (strncmp(cmdline, "ls", 2) == 0) {
        const char *arg = cmdline + 2;
        while (*arg == ' ') arg++;
        if (*arg == '\0') arg = current_directory;
        vfs_list_dir(arg);
    } else if (strncmp(cmdline, "cat", 3) == 0) {
        const char *arg = cmdline + 3;
        while (*arg == ' ') arg++;
        if (*arg == '\0') {
            kprintf("Usage: cat <filename>\n");
        } else {
            char buffer[1024];
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
        // Check if executable exists in /bin or direct path
        char bin_path[64] = "/bin/";
        strncpy(bin_path + 5, cmdline, 48);
        vfs_node_t *node = vfs_find_node(cmdline);
        if (!node) node = vfs_find_node(bin_path);

        if (node && node->data && elf_validate(node->data, node->size)) {
            elf_load_and_run(node->data, node->size);
        } else {
            kprintf("eran: command not found: '%s'. Type 'help' for commands.\n", cmdline);
        }
    }
}

void shell_init(void) {
    print_prompt();
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
            print_prompt();
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
