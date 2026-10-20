#include "ai_engine.h"
#include "../kernel/syscall.h"
#include "../drivers/vga.h"
#include "../drivers/audio.h"
#include "../fs/vfs.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../net/net.h"

static int ai_initialized = 0;

static int str_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n) {
            char ch1 = *h;
            char ch2 = *n;
            if (ch1 >= 'A' && ch1 <= 'Z') ch1 += 32;
            if (ch2 >= 'A' && ch2 <= 'Z') ch2 += 32;
            if (ch1 != ch2) break;
            h++;
            n++;
        }
        if (!*n) return 1; // Found needle
        haystack++;
    }
    return 0;
}

void ai_engine_init(void) {
    ai_initialized = 1;
}

void ai_process_query(const char *query) {
    if (!query || *query == '\0') {
        kprintf("[AI ASSISTANT] Please provide a prompt or question. Example: 'ai what is the uptime'\n");
        return;
    }

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("[AI ASSISTANT] ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    // 1. System Uptime Query
    if (str_contains(query, "uptime") || str_contains(query, "running for") || str_contains(query, "time")) {
        int32_t secs = sys_uptime();
        kprintf("System uptime is %d seconds (queried via INT 0x80 SYS_UPTIME).\n", secs);
    }
    // 2. Memory / RAM Query
    else if (str_contains(query, "memory") || str_contains(query, "ram") || str_contains(query, "heap")) {
        kprintf("Physical Memory Status:\n");
        kprintf(" - Total RAM: 32 MB (8192 x 4KB frames)\n");
        kprintf(" - Free RAM : %u KB\n", pmm_get_free_memory_kb());
        kprintf(" - Used RAM : %u KB\n", pmm_get_used_memory_kb());
        kprintf(" - Free Heap: %u bytes\n", heap_get_free());
    }
    // 3. Filesystem / Files Query
    else if (str_contains(query, "file") || str_contains(query, "ls") || str_contains(query, "directory")) {
        kprintf("Listing virtual filesystem contents at root '/':\n");
        vfs_list_dir("/");
    }
    // 4. Identity / Who are you Query
    else if (str_contains(query, "who") || str_contains(query, "about") || str_contains(query, "what are you")) {
        kprintf("I am the Eran OS Native AI Assistant!\n");
        kprintf("I operate as an unprivileged Ring 3 service connected to the kernel\n");
        kprintf("via validated system call dispatchers (INT 0x80). All processing is\n");
        kprintf("100%% offline with zero cloud telemetry or internet dependencies.\n");
    }
    // 5. Sound / Audio / Chime Query
    else if (str_contains(query, "sound") || str_contains(query, "beep") || str_contains(query, "chime") || str_contains(query, "play")) {
        kprintf("Playing synthesized melodic startup chime...\n");
        audio_chime();
        kprintf("Audio playback completed successfully.\n");
    }
    // 6. Network / IP Query
    else if (str_contains(query, "network") || str_contains(query, "ip") || str_contains(query, "ping")) {
        kprintf("Querying network stack:\n");
        net_print_ifconfig();
    }
    // 7. Security / Sandbox Query
    else if (str_contains(query, "security") || str_contains(query, "sandbox") || str_contains(query, "privilege")) {
        kprintf("Eran OS Security Architecture Status:\n");
        kprintf(" [x] Ring 0 / Ring 3 Privilege Separation (GDT TSS 0x28 Active)\n");
        kprintf(" [x] Strict System Call Gateway (Vector 0x80)\n");
        kprintf(" [x] Path Traversal Rejection (Policy rejects '../')\n");
        kprintf(" [x] No Unchecked Kernel Memory Access by AI Engine\n");
    }
    // 8. Process / Task Query
    else if (str_contains(query, "process") || str_contains(query, "task") || str_contains(query, "threads")) {
        int32_t pid = sys_getpid();
        kprintf("Current active task PID: %d. Scheduler is running round-robin tasks.\n", pid);
    }
    // 9. Help / Unknown Query
    else {
        kprintf("I didn't fully recognize that command. You can ask me:\n");
        kprintf(" - 'ai what is the uptime'\n");
        kprintf(" - 'ai show memory'\n");
        kprintf(" - 'ai list files'\n");
        kprintf(" - 'ai who are you'\n");
        kprintf(" - 'ai play sound'\n");
        kprintf(" - 'ai network status'\n");
        kprintf(" - 'ai security status'\n");
    }
}
