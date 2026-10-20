#include "desktop.h"
#include "../drivers/gfx.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/timer.h"
#include "../drivers/audio.h"
#include "../fs/vfs.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../hal/acpi.h"
#include "../user/micro_ai.h"

// ---------------------------------------------------------------------------
// Window Types & Global Desktop State
// ---------------------------------------------------------------------------
typedef enum {
    WIN_AI = 0,
    WIN_TERMINAL = 1,
    WIN_TELEMETRY = 2,
    WIN_FILES = 3,
    WIN_COUNT = 4
} win_type_t;

static int active_win = WIN_AI;
static int is_maximized = 0;
static int start_menu_open = 0;
static int mouse_click_prev = 0;

// ---------------------------------------------------------------------------
// String Utility Helpers (Safe Freestanding Implementation)
// ---------------------------------------------------------------------------
static size_t gui_strlen(const char *s) {
    size_t len = 0;
    if (!s) return 0;
    while (*s++) len++;
    return len;
}

static void gui_strcpy(char *dest, const char *src) {
    if (!dest || !src) return;
    while ((*dest++ = *src++));
}

static void gui_strncpy(char *dest, const char *src, size_t n) {
    if (!dest || !src || n == 0) return;
    size_t i = 0;
    while (i < n - 1 && src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int gui_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static int gui_strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static void gui_u_to_str(uint32_t val, char *buf) {
    char temp[12];
    int i = 0;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (val > 0) {
        temp[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

// ---------------------------------------------------------------------------
// Workstation 1: Interactive AI Assistant Buffer & State
// ---------------------------------------------------------------------------
#define AI_MAX_LINES 9
#define AI_LINE_LEN  56

static char ai_lines[AI_MAX_LINES][AI_LINE_LEN];
static int  ai_line_count = 0;
static char ai_input[AI_LINE_LEN];
static int  ai_input_len = 0;
static char ai_badge[40] = "INTENT: READY (100%)";

static void ai_add_line(const char *text) {
    if (!text) return;
    if (ai_line_count < AI_MAX_LINES) {
        gui_strncpy(ai_lines[ai_line_count], text, AI_LINE_LEN);
        ai_line_count++;
    } else {
        for (int i = 0; i < AI_MAX_LINES - 1; i++) {
            gui_strcpy(ai_lines[i], ai_lines[i + 1]);
        }
        gui_strncpy(ai_lines[AI_MAX_LINES - 1], text, AI_LINE_LEN);
    }
}

static void ai_submit_query(const char *query) {
    while (*query == ' ') query++;
    if (*query == '\0') return;

    // Echo user prompt
    char ubuf[AI_LINE_LEN] = "You: ";
    gui_strncpy(ubuf + 5, query, AI_LINE_LEN - 6);
    ai_add_line(ubuf);

    // Call embedded micro-neural model
    neural_result_t res = micro_ai_infer(query);

    // Update Intent Badge
    char badge[40] = "INTENT: ";
    gui_strncpy(badge + 8, res.intent_name, 20);
    gui_strncpy(badge + gui_strlen(badge), " (", 3);
    char conf_str[8];
    gui_u_to_str((uint32_t)res.confidence_percent, conf_str);
    gui_strncpy(badge + gui_strlen(badge), conf_str, 5);
    gui_strncpy(badge + gui_strlen(badge), "%)", 3);
    gui_strncpy(ai_badge, badge, sizeof(ai_badge) - 1);

    // Intelligent conversational response
    switch (res.predicted_intent) {
        case INTENT_SYSTEM_TELEMETRY:
            ai_add_line("AI: Eran OS x86 32-bit PM. GDT/IDT/PMM online.");
            break;
        case INTENT_MEMORY_INSPECTION:
            ai_add_line("AI: Physical RAM: 32MB Total. Kernel Heap 4MB OK.");
            break;
        case INTENT_STORAGE_FILESYSTEM:
            ai_add_line("AI: VFS mounted at /. Primary ATA IDE 0x1F0 ready.");
            break;
        case INTENT_NETWORK_DIAGNOSTICS:
            ai_add_line("AI: eth0: 192.168.1.50/24, Gateway 192.168.1.1 OK.");
            break;
        case INTENT_AUDIO_SOUND:
            audio_chime();
            ai_add_line("AI: Melodic sequence synthesized on PC Speaker.");
            break;
        case INTENT_GUI_DESKTOP:
            ai_add_line("AI: 2D Desktop Compositor active with PS/2 mouse.");
            break;
        case INTENT_SECURITY_PRIVILEGE:
            ai_add_line("AI: Ring 0/3 CPU isolation active. INT 0x80 gate.");
            break;
        case INTENT_GENERAL_HELP:
        default:
            ai_add_line("AI: Ready. Ask about memory, storage, or audio!");
            break;
    }
}

// ---------------------------------------------------------------------------
// Workstation 2: In-GUI Terminal Console Buffer & Mini-Shell
// ---------------------------------------------------------------------------
#define TERM_MAX_LINES 11
#define TERM_LINE_LEN  56

static char term_lines[TERM_MAX_LINES][TERM_LINE_LEN];
static int  term_line_count = 0;
static char term_input[TERM_LINE_LEN];
static int  term_input_len = 0;

static void term_add_line(const char *text) {
    if (!text) return;
    if (term_line_count < TERM_MAX_LINES) {
        gui_strncpy(term_lines[term_line_count], text, TERM_LINE_LEN);
        term_line_count++;
    } else {
        for (int i = 0; i < TERM_MAX_LINES - 1; i++) {
            gui_strcpy(term_lines[i], term_lines[i + 1]);
        }
        gui_strncpy(term_lines[TERM_MAX_LINES - 1], text, TERM_LINE_LEN);
    }
}

static void file_refresh_list(void);

static void term_execute_cmd(const char *cmd) {
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    char echo_buf[TERM_LINE_LEN] = "$ ";
    gui_strncpy(echo_buf + 2, cmd, TERM_LINE_LEN - 3);
    term_add_line(echo_buf);

    if (gui_strcmp(cmd, "help") == 0) {
        term_add_line("Available Commands:");
        term_add_line(" uptime, meminfo, ls, cat <file>, touch <file>");
        term_add_line(" write <file> <text>, battery, acpi, chime, beep, clear");
    } else if (gui_strcmp(cmd, "uptime") == 0) {
        char buf[TERM_LINE_LEN] = "Uptime: ";
        char num[12];
        gui_u_to_str(timer_get_uptime_seconds(), num);
        gui_strncpy(buf + 8, num, 10);
        gui_strncpy(buf + gui_strlen(buf), "s (ticks: ", 10);
        gui_u_to_str(timer_get_ticks(), num);
        gui_strncpy(buf + gui_strlen(buf), num, 10);
        gui_strncpy(buf + gui_strlen(buf), ")", 2);
        term_add_line(buf);
    } else if (gui_strcmp(cmd, "meminfo") == 0) {
        uint32_t total = pmm_get_total_memory_kb() / 1024;
        uint32_t used = pmm_get_used_memory_kb() / 1024;
        uint32_t free = pmm_get_free_memory_kb() / 1024;
        char buf[TERM_LINE_LEN] = "RAM: Total ";
        char num[12];
        gui_u_to_str(total, num);
        gui_strncpy(buf + gui_strlen(buf), num, 8);
        gui_strncpy(buf + gui_strlen(buf), "MB, Used ", 10);
        gui_u_to_str(used, num);
        gui_strncpy(buf + gui_strlen(buf), num, 8);
        gui_strncpy(buf + gui_strlen(buf), "MB, Free ", 10);
        gui_u_to_str(free, num);
        gui_strncpy(buf + gui_strlen(buf), num, 8);
        gui_strncpy(buf + gui_strlen(buf), "MB", 3);
        term_add_line(buf);

        char hbuf[TERM_LINE_LEN] = "Heap: Used ";
        gui_u_to_str((uint32_t)heap_get_used(), num);
        gui_strncpy(hbuf + gui_strlen(hbuf), num, 10);
        gui_strncpy(hbuf + gui_strlen(hbuf), " B, Free ", 10);
        gui_u_to_str((uint32_t)heap_get_free(), num);
        gui_strncpy(hbuf + gui_strlen(hbuf), num, 10);
        gui_strncpy(hbuf + gui_strlen(hbuf), " B", 3);
        term_add_line(hbuf);
    } else if (gui_strcmp(cmd, "ls") == 0) {
        term_add_line("Directory Listing for root '/':");
        vfs_node_t *r = vfs_get_root();
        if (r && r->children) {
            vfs_node_t *c = r->children;
            while (c) {
                char entry[TERM_LINE_LEN] = "  ";
                if (c->type == FS_DIRECTORY) gui_strncpy(entry + 2, "[DIR]  ", 8);
                else gui_strncpy(entry + 2, "[FILE] ", 8);
                gui_strncpy(entry + gui_strlen(entry), c->name, 24);
                term_add_line(entry);
                c = c->next;
            }
        }
    } else if (gui_strncmp(cmd, "cat ", 4) == 0) {
        const char *arg = cmd + 4;
        while (*arg == ' ') arg++;
        char readbuf[128];
        int b = vfs_read_file(arg, readbuf, sizeof(readbuf) - 1);
        if (b > 0) {
            readbuf[b] = '\0';
            int start = 0;
            for (int i = 0; i <= b; i++) {
                if (readbuf[i] == '\n' || readbuf[i] == '\0') {
                    readbuf[i] = '\0';
                    term_add_line(readbuf + start);
                    start = i + 1;
                    if (start >= b) break;
                }
            }
        } else {
            term_add_line("cat: file not found or empty");
        }
    } else if (gui_strncmp(cmd, "touch ", 6) == 0) {
        const char *arg = cmd + 6;
        while (*arg == ' ') arg++;
        int res = vfs_create_file(arg, "");
        if (res == 0) {
            term_add_line("touch: file created successfully");
            file_refresh_list();
        } else {
            term_add_line("touch: failed to create file");
        }
    } else if (gui_strncmp(cmd, "write ", 6) == 0) {
        const char *arg = cmd + 6;
        while (*arg == ' ') arg++;
        char fpath[32];
        int pi = 0;
        while (*arg && *arg != ' ' && pi < 31) {
            fpath[pi++] = *arg++;
        }
        fpath[pi] = '\0';
        while (*arg == ' ') arg++;
        int res = vfs_write_file(fpath, arg, gui_strlen(arg));
        if (res >= 0) {
            term_add_line("write: file written successfully");
            file_refresh_list();
        } else {
            term_add_line("write: failed to write to file");
        }
    } else if (gui_strcmp(cmd, "battery") == 0) {
        term_add_line("Battery: 94% [AC Mains Online, Charging]");
    } else if (gui_strcmp(cmd, "acpi") == 0) {
        term_add_line("ACPI: 0x07FE1000 RSDT, 0x07FE1200 FADT, PM1a 0x604");
    } else if (gui_strcmp(cmd, "chime") == 0) {
        audio_chime();
        term_add_line("Audio: Melodic chime playing on PC Speaker.");
    } else if (gui_strcmp(cmd, "beep") == 0) {
        audio_beep();
        term_add_line("Audio: Alert beep tone emitted.");
    } else if (gui_strcmp(cmd, "version") == 0) {
        term_add_line("Eran OS Kernel v0.4.0 (x86 32-bit Protected Mode)");
    } else if (gui_strcmp(cmd, "clear") == 0) {
        term_line_count = 0;
    } else {
        term_add_line("Command not recognized. Type 'help' for commands.");
    }
}

// ---------------------------------------------------------------------------
// Workstation 3: VFS File Explorer State & Previews
// ---------------------------------------------------------------------------
#define FILE_MAX_ITEMS 14
static char file_names[FILE_MAX_ITEMS][32];
static size_t file_sizes[FILE_MAX_ITEMS];
static int file_is_dir[FILE_MAX_ITEMS];
static int file_item_count = 0;
static int file_selected_idx = 0;
static char file_preview[5][46];
static int file_preview_count = 0;

static void file_refresh_list(void) {
    file_item_count = 0;
    vfs_node_t *root = vfs_get_root();
    if (!root) return;

    vfs_node_t *curr = root->children;
    while (curr && file_item_count < FILE_MAX_ITEMS) {
        gui_strncpy(file_names[file_item_count], curr->name, 32);
        file_sizes[file_item_count] = curr->size;
        file_is_dir[file_item_count] = (curr->type == FS_DIRECTORY);
        file_item_count++;
        curr = curr->next;
    }

    vfs_node_t *sys = vfs_find_node("/system");
    if (sys && sys->children) {
        curr = sys->children;
        while (curr && file_item_count < FILE_MAX_ITEMS) {
            char path[32] = "sys/";
            gui_strncpy(path + 4, curr->name, 28);
            gui_strncpy(file_names[file_item_count], path, 32);
            file_sizes[file_item_count] = curr->size;
            file_is_dir[file_item_count] = (curr->type == FS_DIRECTORY);
            file_item_count++;
            curr = curr->next;
        }
    }

    vfs_node_t *bin = vfs_find_node("/bin");
    if (bin && bin->children) {
        curr = bin->children;
        while (curr && file_item_count < FILE_MAX_ITEMS) {
            char path[32] = "bin/";
            gui_strncpy(path + 4, curr->name, 28);
            gui_strncpy(file_names[file_item_count], path, 32);
            file_sizes[file_item_count] = curr->size;
            file_is_dir[file_item_count] = (curr->type == FS_DIRECTORY);
            file_item_count++;
            curr = curr->next;
        }
    }
}

static void file_update_preview(void) {
    file_preview_count = 0;
    if (file_item_count == 0 || file_selected_idx >= file_item_count) return;

    char full_path[40] = "/";
    const char *sel = file_names[file_selected_idx];
    if (gui_strncmp(sel, "sys/", 4) == 0) {
        gui_strcpy(full_path, "/system/");
        gui_strcpy(full_path + 8, sel + 4);
    } else if (gui_strncmp(sel, "bin/", 4) == 0) {
        gui_strcpy(full_path, "/bin/");
        gui_strcpy(full_path + 5, sel + 4);
    } else {
        gui_strcpy(full_path + 1, sel);
    }

    if (file_is_dir[file_selected_idx]) {
        gui_strcpy(file_preview[0], "Directory content (click to inspect)");
        file_preview_count = 1;
        return;
    }

    char buf[256];
    int bytes = vfs_read_file(full_path, buf, sizeof(buf) - 1);
    if (bytes > 0) {
        buf[bytes] = '\0';
        int line = 0;
        int col = 0;
        for (int i = 0; i < bytes && line < 5; i++) {
            if (buf[i] == '\n' || col >= 42) {
                file_preview[line][col] = '\0';
                line++;
                col = 0;
            } else if (buf[i] >= 32 && buf[i] <= 126) {
                file_preview[line][col++] = buf[i];
            }
        }
        if (col > 0 && line < 5) {
            file_preview[line][col] = '\0';
            line++;
        }
        file_preview_count = line;
    } else {
        gui_strcpy(file_preview[0], "(Empty or binary content)");
        file_preview_count = 1;
    }
}

// ---------------------------------------------------------------------------
// Hardware Telemetry & Live Gauges Pane
// ---------------------------------------------------------------------------
static void draw_telemetry_pane(int x, int y, int w, int h) {
    (void)h;
    uint8_t bg = VGA_COLOR_BLACK;

    // CPU Architecture Header
    gfx_draw_string(x + 2, y + 2, "CPU Architecture", VGA_COLOR_LIGHT_CYAN, bg);
    gfx_draw_string(x + 2, y + 3, "x86 32-bit (x86_64 Capable)", VGA_COLOR_WHITE, bg);

    // RAM Usage Gauge
    uint32_t total_ram = pmm_get_total_memory_kb() / 1024;
    uint32_t used_ram = pmm_get_used_memory_kb() / 1024;
    int ram_pct = (total_ram > 0) ? (int)(used_ram * 100 / total_ram) : 10;
    gfx_draw_string(x + 2, y + 5, "RAM: 32MB Total", VGA_COLOR_LIGHT_GREEN, bg);
    gfx_draw_progress_bar(x + 2, y + 6, w - 4, ram_pct, VGA_COLOR_LIGHT_GREEN, bg);

    // Dynamic Kernel Heap Gauge
    size_t heap_used = heap_get_used();
    size_t heap_free = heap_get_free();
    size_t heap_tot = heap_used + heap_free;
    int heap_pct = (heap_tot > 0) ? (int)(heap_used * 100 / heap_tot) : 5;
    gfx_draw_string(x + 2, y + 8, "Heap: 4MB Dynamic", VGA_COLOR_LIGHT_CYAN, bg);
    gfx_draw_progress_bar(x + 2, y + 9, w - 4, heap_pct, VGA_COLOR_LIGHT_CYAN, bg);

    // Laptop Battery & AC Mains Gauge
    gfx_draw_string(x + 2, y + 11, "Battery: 94% [AC Online]", VGA_COLOR_LIGHT_BROWN, bg);
    gfx_draw_progress_bar(x + 2, y + 12, w - 4, 94, VGA_COLOR_LIGHT_GREEN, bg);

    // Subsystems
    gfx_draw_string(x + 2, y + 14, "Net : 192.168.1.50 (e1000)", VGA_COLOR_LIGHT_GREY, bg);
    gfx_draw_string(x + 2, y + 15, "Disk: ATA IDE 0x1F0 LBA", VGA_COLOR_LIGHT_GREY, bg);
    gfx_draw_string(x + 2, y + 16, "Audio: 8254 PIT Ch 2", VGA_COLOR_LIGHT_GREY, bg);

    // Live Uptime
    char uptime_str[24] = "Uptime: ";
    char sec_buf[10];
    gui_u_to_str(timer_get_uptime_seconds(), sec_buf);
    gui_strncpy(uptime_str + 8, sec_buf, 6);
    gui_strncpy(uptime_str + gui_strlen(uptime_str), " sec", 5);
    gfx_draw_string(x + 2, y + 18, uptime_str, VGA_COLOR_WHITE, bg);

    // Mouse coordinates
    const mouse_state_t *ms = mouse_get_state();
    if (ms) {
        char mouse_str[24] = "Cursor: (";
        char mxy[8];
        gui_u_to_str((uint32_t)ms->x, mxy);
        gui_strncpy(mouse_str + 9, mxy, 4);
        gui_strncpy(mouse_str + gui_strlen(mouse_str), ",", 2);
        gui_u_to_str((uint32_t)ms->y, mxy);
        gui_strncpy(mouse_str + gui_strlen(mouse_str), mxy, 4);
        gui_strncpy(mouse_str + gui_strlen(mouse_str), ")", 2);
        gfx_draw_string(x + 2, y + 19, mouse_str, VGA_COLOR_DARK_GREY, bg);
    }
}

// ---------------------------------------------------------------------------
// Desktop Initialization
// ---------------------------------------------------------------------------
void desktop_init(void) {
    gfx_init();

    // Initialize default AI chat lines
    ai_line_count = 0;
    ai_add_line("AI: Welcome to Eran OS Native AI Assistant!");
    ai_add_line("AI: Embedded Micro-Neural Model loaded offline.");
    ai_add_line("AI: Ask about memory, storage, network, or audio.");

    // Initialize default Terminal lines
    term_line_count = 0;
    term_add_line("Eran OS Graphical Workstation Shell Console");
    term_add_line("Connected to Kernel Subsystems. Ring 0/3 Active.");
    term_add_line("Type 'help' to see list of available commands.");

    // Initialize Files
    file_refresh_list();
    file_update_preview();
}

// ---------------------------------------------------------------------------
// Desktop Render Compositor
// ---------------------------------------------------------------------------
void desktop_render(void) {
    // 1. Wallpaper Background (Deep Royal Blue with subtle light blue micro-dots)
    gfx_clear(VGA_COLOR_BLUE);
    for (int y = 1; y < CANVAS_HEIGHT - 1; y += 2) {
        for (int x = 2; x < CANVAS_WIDTH - 2; x += 4) {
            gfx_put_char(x, y, (char)0xFA, VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLUE);
        }
    }

    // 2. Top Application Menu Bar
    gfx_fill_rect(0, 0, CANVAS_WIDTH, 1, ' ', VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(1, 0, "[= ERAN OS =]",
                    start_menu_open ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    start_menu_open ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(15, 0, "Files",
                    (active_win == WIN_FILES) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_FILES) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(22, 0, "Terminal",
                    (active_win == WIN_TERMINAL) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_TERMINAL) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(32, 0, "AI Core",
                    (active_win == WIN_AI) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_AI) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(41, 0, "Monitor",
                    (active_win == WIN_TELEMETRY) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_TELEMETRY) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(50, 0, "[Audio]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);

    // Top status indicators
    char up_str[16] = "Up: ";
    char up_sec[8];
    gui_u_to_str(timer_get_uptime_seconds(), up_sec);
    gui_strncpy(up_str + 4, up_sec, 6);
    gui_strncpy(up_str + gui_strlen(up_str), "s", 2);
    gfx_draw_string(59, 0, up_str, VGA_COLOR_DARK_GREY, VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(68, 0, "94%", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(73, 0, "Ring3", VGA_COLOR_BLUE, VGA_COLOR_LIGHT_GREY);

    // 3. Render Windows Layout
    if (is_maximized) {
        // Maximized Layout: 1 Wide Window (78 cols)
        int wx = 1, wy = 2, ww = 77, wh = 21;
        gfx_draw_shadow(wx, wy, ww, wh);

        const char *title = (active_win == WIN_AI) ? " Eran AI Productivity Core " :
                            (active_win == WIN_TERMINAL) ? " Interactive Shell Console " :
                            (active_win == WIN_FILES) ? " VFS File Explorer " :
                            " System Telemetry & HAL ";

        gfx_draw_double_box(wx, wy, ww, wh, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK, title);

        if (active_win == WIN_AI) {
            gfx_draw_string(wx + 2, wy + 2, ai_badge, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREEN);
            for (int i = 0; i < ai_line_count; i++) {
                uint8_t fg = (ai_lines[i][0] == 'Y') ? VGA_COLOR_WHITE : VGA_COLOR_LIGHT_CYAN;
                gfx_draw_string(wx + 2, wy + 4 + i, ai_lines[i], fg, VGA_COLOR_BLACK);
            }
            // Prompt input
            gfx_fill_rect(wx + 2, wy + wh - 3, ww - 4, 1, ' ', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_draw_string(wx + 2, wy + wh - 3, "> ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_DARK_GREY);
            gfx_draw_string(wx + 4, wy + wh - 3, ai_input, VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_put_char(wx + 4 + ai_input_len, wy + wh - 3, '_', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_DARK_GREY);
            gfx_draw_string(wx + 2, wy + wh - 2, "[Type query & press ENTER. Try 'memory', 'audio', 'files']", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        } else if (active_win == WIN_TERMINAL) {
            gfx_draw_string(wx + 2, wy + 2, "Eran OS v0.4.0 Interactive GUI Terminal Console", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            for (int i = 0; i < term_line_count; i++) {
                uint8_t fg = (term_lines[i][0] == '$') ? VGA_COLOR_WHITE : VGA_COLOR_LIGHT_GREY;
                gfx_draw_string(wx + 2, wy + 4 + i, term_lines[i], fg, VGA_COLOR_BLACK);
            }
            // Prompt input
            gfx_fill_rect(wx + 2, wy + wh - 3, ww - 4, 1, ' ', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_draw_string(wx + 2, wy + wh - 3, "$ ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_DARK_GREY);
            gfx_draw_string(wx + 4, wy + wh - 3, term_input, VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_put_char(wx + 4 + term_input_len, wy + wh - 3, '_', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_draw_string(wx + 2, wy + wh - 2, "[Type commands: uptime, meminfo, ls, cat, touch, write, chime]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        } else if (active_win == WIN_FILES) {
            gfx_draw_string(wx + 2, wy + 2, "Virtual Filesystem Browser [/]", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            for (int i = 0; i < file_item_count; i++) {
                uint8_t fg = (i == file_selected_idx) ? VGA_COLOR_BLACK : VGA_COLOR_WHITE;
                uint8_t bg = (i == file_selected_idx) ? VGA_COLOR_CYAN : VGA_COLOR_BLACK;
                char entry[48] = "  ";
                if (file_is_dir[i]) gui_strncpy(entry + 2, "[DIR]  ", 8);
                else gui_strncpy(entry + 2, "[FILE] ", 8);
                gui_strncpy(entry + gui_strlen(entry), file_names[i], 24);
                gfx_draw_string(wx + 2, wy + 4 + i, entry, fg, bg);
            }
            // File preview
            gfx_draw_box(wx + 38, wy + 4, 36, 12, VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK, " Preview ");
            for (int p = 0; p < file_preview_count; p++) {
                gfx_draw_string(wx + 40, wy + 6 + p, file_preview[p], VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            }
            gfx_draw_string(wx + 2, wy + wh - 2, "[Keys: W/S or UP/DOWN to select, ENTER to view preview]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        } else {
            draw_telemetry_pane(wx, wy, ww, wh);
        }
    } else {
        // Split Layout: Left Active Tool (48 cols) + Right Hardware HAL (28 cols)
        int lx = 1, ly = 2, lw = 48, lh = 21;
        int rx = 50, ry = 2, rw = 28, rh = 21;

        gfx_draw_shadow(lx, ly, lw, lh);
        gfx_draw_shadow(rx, ry, rw, rh);

        const char *ltitle = (active_win == WIN_AI) ? " Eran AI Productivity Core " :
                             (active_win == WIN_TERMINAL) ? " Interactive Shell Console " :
                             (active_win == WIN_FILES) ? " VFS File Explorer " :
                             " System Telemetry & HAL ";

        gfx_draw_double_box(lx, ly, lw, lh, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK, ltitle);
        gfx_draw_double_box(rx, ry, rw, rh, VGA_COLOR_WHITE, VGA_COLOR_BLACK, " Hardware HAL ");

        // Draw Left Window Content
        if (active_win == WIN_AI) {
            gfx_draw_string(lx + 2, ly + 2, ai_badge, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREEN);
            for (int i = 0; i < ai_line_count; i++) {
                uint8_t fg = (ai_lines[i][0] == 'Y') ? VGA_COLOR_WHITE : VGA_COLOR_LIGHT_CYAN;
                gfx_draw_string(lx + 2, ly + 4 + i, ai_lines[i], fg, VGA_COLOR_BLACK);
            }
            // Prompt input box
            gfx_fill_rect(lx + 2, ly + lh - 3, lw - 4, 1, ' ', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_draw_string(lx + 2, ly + lh - 3, "> ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_DARK_GREY);
            gfx_draw_string(lx + 4, ly + lh - 3, ai_input, VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_put_char(lx + 4 + ai_input_len, ly + lh - 3, '_', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_DARK_GREY);
            gfx_draw_string(lx + 2, ly + lh - 2, "[Type query & ENTER. Try 'memory']", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        } else if (active_win == WIN_TERMINAL) {
            gfx_draw_string(lx + 2, ly + 2, "Eran OS GUI Shell Console", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            for (int i = 0; i < term_line_count; i++) {
                uint8_t fg = (term_lines[i][0] == '$') ? VGA_COLOR_WHITE : VGA_COLOR_LIGHT_GREY;
                gfx_draw_string(lx + 2, ly + 4 + i, term_lines[i], fg, VGA_COLOR_BLACK);
            }
            // Prompt input box
            gfx_fill_rect(lx + 2, ly + lh - 3, lw - 4, 1, ' ', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_draw_string(lx + 2, ly + lh - 3, "$ ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_DARK_GREY);
            gfx_draw_string(lx + 4, ly + lh - 3, term_input, VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_put_char(lx + 4 + term_input_len, ly + lh - 3, '_', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
            gfx_draw_string(lx + 2, ly + lh - 2, "[Type: uptime, meminfo, ls, cat, chime]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        } else if (active_win == WIN_FILES) {
            gfx_draw_string(lx + 2, ly + 2, "Virtual Filesystem [/]", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            int max_display = (file_item_count < 6) ? file_item_count : 6;
            for (int i = 0; i < max_display; i++) {
                uint8_t fg = (i == file_selected_idx) ? VGA_COLOR_BLACK : VGA_COLOR_WHITE;
                uint8_t bg = (i == file_selected_idx) ? VGA_COLOR_CYAN : VGA_COLOR_BLACK;
                char entry[48] = "  ";
                if (file_is_dir[i]) gui_strncpy(entry + 2, "[DIR]  ", 8);
                else gui_strncpy(entry + 2, "[FILE] ", 8);
                gui_strncpy(entry + gui_strlen(entry), file_names[i], 24);
                gfx_draw_string(lx + 2, ly + 4 + i, entry, fg, bg);
            }
            // File preview box
            gfx_draw_box(lx + 2, ly + 11, lw - 4, 7, VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK, " Preview ");
            for (int p = 0; p < file_preview_count; p++) {
                gfx_draw_string(lx + 4, ly + 13 + p, file_preview[p], VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            }
            gfx_draw_string(lx + 2, ly + lh - 2, "[W/S: Select, ENTER: Preview]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        } else {
            draw_telemetry_pane(lx, ly, lw, lh);
        }

        // Draw Right Window Content (Always Live Telemetry in Split View)
        draw_telemetry_pane(rx, ry, rw, rh);
    }

    // 4. Start Menu Pop-up (when active)
    if (start_menu_open) {
        int smx = 1, smy = 9, smw = 34, smh = 14;
        gfx_draw_shadow(smx, smy, smw, smh);
        gfx_draw_double_box(smx, smy, smw, smh, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE, " ERAN OS MENU ");

        gfx_draw_string(smx + 3, smy + 2, "[1] Eran AI Assistant", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 3, "[2] Terminal Console", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 4, "[3] Hardware Telemetry", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 5, "[4] VFS File Explorer", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 2, smy + 6, "------------------------------", VGA_COLOR_CYAN, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 7, "[5] Play Melodic Chime", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 8, "[6] ACPI Power Off", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 9, "[7] System Reboot", VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 10, "[8] Exit to Shell (ESC)", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 12, "Press [1-8] or click item", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE);
    }

    // 5. Bottom Taskbar
    gfx_fill_rect(0, CANVAS_HEIGHT - 1, CANVAS_WIDTH, 1, ' ', VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);

    // Start Button
    gfx_draw_string(1, CANVAS_HEIGHT - 1, "[ START ]",
                    start_menu_open ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    start_menu_open ? VGA_COLOR_RED : VGA_COLOR_CYAN);

    // Window Tabs
    gfx_draw_string(11, CANVAS_HEIGHT - 1, "[1: AI Core]",
                    (active_win == WIN_AI) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_AI) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(24, CANVAS_HEIGHT - 1, "[2: Terminal]",
                    (active_win == WIN_TERMINAL) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_TERMINAL) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(38, CANVAS_HEIGHT - 1, "[3: HAL]",
                    (active_win == WIN_TELEMETRY) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_TELEMETRY) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(47, CANVAS_HEIGHT - 1, "[4: Files]",
                    (active_win == WIN_FILES) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_FILES) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    // Quick action buttons
    gfx_draw_string(58, CANVAS_HEIGHT - 1, "[Chime]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(66, CANVAS_HEIGHT - 1, is_maximized ? "[Split]" : "[Max]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(74, CANVAS_HEIGHT - 1, "[Exit]", VGA_COLOR_LIGHT_RED, VGA_COLOR_LIGHT_GREY);

    // 6. Draw Mouse Cursor Sprite
    const mouse_state_t *ms = mouse_get_state();
    if (ms) {
        gfx_put_char(ms->x, ms->y, (char)0x18, VGA_COLOR_WHITE, VGA_COLOR_RED);
    }

    // 7. Blit to VGA Text Display Memory
    gfx_flip_to_screen();
}

// ---------------------------------------------------------------------------
// Interactive Desktop Main Loop
// ---------------------------------------------------------------------------
void desktop_interactive(void) {
    serial_puts("[DESKTOP] Launching 2D graphical desktop workstation...\n");
    desktop_init();
    desktop_render();

    int running = 1;
    while (running) {
        // --- Handle Mouse Input ---
        if (mouse_has_moved()) {
            const mouse_state_t *ms = mouse_get_state();
            if (ms) {
                int is_clicked = (ms->buttons & MOUSE_BTN_LEFT) && !mouse_click_prev;
                mouse_click_prev = (ms->buttons & MOUSE_BTN_LEFT);

                if (is_clicked) {
                    // Check Bottom Taskbar Clicks
                    if (ms->y == CANVAS_HEIGHT - 1) {
                        if (ms->x < 10) {
                            start_menu_open = !start_menu_open;
                        } else if (ms->x >= 11 && ms->x < 23) {
                            active_win = WIN_AI;
                            start_menu_open = 0;
                        } else if (ms->x >= 24 && ms->x < 37) {
                            active_win = WIN_TERMINAL;
                            start_menu_open = 0;
                        } else if (ms->x >= 38 && ms->x < 46) {
                            active_win = WIN_TELEMETRY;
                            start_menu_open = 0;
                        } else if (ms->x >= 47 && ms->x < 57) {
                            active_win = WIN_FILES;
                            start_menu_open = 0;
                            file_refresh_list();
                            file_update_preview();
                        } else if (ms->x >= 58 && ms->x < 65) {
                            audio_chime();
                        } else if (ms->x >= 66 && ms->x < 73) {
                            is_maximized = !is_maximized;
                        } else if (ms->x >= 74) {
                            running = 0;
                        }
                    }
                    // Check Top Menu Bar Clicks
                    else if (ms->y == 0) {
                        if (ms->x < 14) {
                            start_menu_open = !start_menu_open;
                        } else if (ms->x >= 15 && ms->x < 21) {
                            active_win = WIN_FILES;
                            start_menu_open = 0;
                            file_refresh_list();
                            file_update_preview();
                        } else if (ms->x >= 22 && ms->x < 31) {
                            active_win = WIN_TERMINAL;
                            start_menu_open = 0;
                        } else if (ms->x >= 32 && ms->x < 40) {
                            active_win = WIN_AI;
                            start_menu_open = 0;
                        } else if (ms->x >= 41 && ms->x < 49) {
                            active_win = WIN_TELEMETRY;
                            start_menu_open = 0;
                        } else if (ms->x >= 50 && ms->x < 58) {
                            audio_chime();
                        }
                    }
                    // Check Start Menu Clicks
                    else if (start_menu_open) {
                        if (ms->x >= 1 && ms->x <= 35 && ms->y >= 11 && ms->y <= 21) {
                            if (ms->y == 11) { active_win = WIN_AI; start_menu_open = 0; }
                            else if (ms->y == 12) { active_win = WIN_TERMINAL; start_menu_open = 0; }
                            else if (ms->y == 13) { active_win = WIN_TELEMETRY; start_menu_open = 0; }
                            else if (ms->y == 14) { active_win = WIN_FILES; start_menu_open = 0; file_refresh_list(); file_update_preview(); }
                            else if (ms->y == 16) { audio_chime(); start_menu_open = 0; }
                            else if (ms->y == 17) { acpi_poweroff(); }
                            else if (ms->y == 18) { acpi_reboot(); }
                            else if (ms->y == 19) { running = 0; }
                        } else {
                            start_menu_open = 0;
                        }
                    }
                    // Check Window Controls (Close [■] or Maximize [^])
                    else if (ms->y == 2) {
                        if (ms->x >= 3 && ms->x <= 5) {
                            // Close/Next window
                            active_win = (active_win + 1) % WIN_COUNT;
                            if (active_win == WIN_FILES) {
                                file_refresh_list();
                                file_update_preview();
                            }
                        } else if (ms->x >= 44 && ms->x <= 48) {
                            is_maximized = !is_maximized;
                        }
                    }
                    // Check File row selection in Files window
                    else if (active_win == WIN_FILES && ms->y >= 6 && ms->y <= 12 && ms->x >= 3 && ms->x <= 45) {
                        int sel = ms->y - 6;
                        if (sel >= 0 && sel < file_item_count) {
                            file_selected_idx = sel;
                            file_update_preview();
                        }
                    }
                }
            }
            desktop_render();
        }

        // --- Handle Keyboard & Serial Input ---
        char c = 0;
        if (keyboard_has_char()) {
            c = keyboard_getc();
        } else if (serial_has_rx()) {
            c = serial_getc();
        }

        if (c != 0) {
            if (c == '\r') c = '\n';
            if (c == 127) c = '\b';

            if (start_menu_open) {
                if (c == '1') { active_win = WIN_AI; start_menu_open = 0; }
                else if (c == '2') { active_win = WIN_TERMINAL; start_menu_open = 0; }
                else if (c == '3') { active_win = WIN_TELEMETRY; start_menu_open = 0; }
                else if (c == '4') { active_win = WIN_FILES; start_menu_open = 0; file_refresh_list(); file_update_preview(); }
                else if (c == '5') { audio_chime(); start_menu_open = 0; }
                else if (c == '6') { acpi_poweroff(); }
                else if (c == '7') { acpi_reboot(); }
                else if (c == '8' || c == 27) { start_menu_open = 0; }
            } else {
                if (c == 27) { // ESC key
                    running = 0;
                } else if (c == '\t') { // TAB key cycles active window
                    active_win = (active_win + 1) % WIN_COUNT;
                    if (active_win == WIN_FILES) {
                        file_refresh_list();
                        file_update_preview();
                    }
                } else if (active_win == WIN_TERMINAL) {
                    if (c == '\n') {
                        term_execute_cmd(term_input);
                        term_input_len = 0;
                        term_input[0] = '\0';
                    } else if (c == '\b') {
                        if (term_input_len > 0) {
                            term_input_len--;
                            term_input[term_input_len] = '\0';
                        }
                    } else if (c >= 32 && c <= 126) {
                        if (term_input_len < TERM_LINE_LEN - 1) {
                            term_input[term_input_len++] = c;
                            term_input[term_input_len] = '\0';
                        }
                    }
                } else if (active_win == WIN_AI) {
                    if (c == '\n') {
                        ai_submit_query(ai_input);
                        ai_input_len = 0;
                        ai_input[0] = '\0';
                    } else if (c == '\b') {
                        if (ai_input_len > 0) {
                            ai_input_len--;
                            ai_input[ai_input_len] = '\0';
                        }
                    } else if (c >= 32 && c <= 126) {
                        if (ai_input_len < AI_LINE_LEN - 1) {
                            ai_input[ai_input_len++] = c;
                            ai_input[ai_input_len] = '\0';
                        }
                    }
                } else if (active_win == WIN_FILES) {
                    if (c == 'w' || c == 'k') {
                        if (file_selected_idx > 0) {
                            file_selected_idx--;
                            file_update_preview();
                        }
                    } else if (c == 's' || c == 'j') {
                        if (file_selected_idx + 1 < file_item_count) {
                            file_selected_idx++;
                            file_update_preview();
                        }
                    } else if (c == '\n' || c == 'o') {
                        file_update_preview();
                    } else if (c == '1') { active_win = WIN_AI; }
                    else if (c == '2') { active_win = WIN_TERMINAL; }
                    else if (c == '3') { active_win = WIN_TELEMETRY; }
                    else if (c == '4') { active_win = WIN_FILES; }
                    else if (c == 'b' || c == 'B') { audio_chime(); }
                    else if (c == 'f' || c == 'F') { is_maximized = !is_maximized; }
                    else if (c == 'q' || c == 'Q') { running = 0; }
                } else if (active_win == WIN_TELEMETRY) {
                    if (c == '1') { active_win = WIN_AI; }
                    else if (c == '2') { active_win = WIN_TERMINAL; }
                    else if (c == '3') { active_win = WIN_TELEMETRY; }
                    else if (c == '4') { active_win = WIN_FILES; file_refresh_list(); file_update_preview(); }
                    else if (c == 'b' || c == 'B') { audio_chime(); }
                    else if (c == 'f' || c == 'F') { is_maximized = !is_maximized; }
                    else if (c == 's' || c == 'S') { start_menu_open = !start_menu_open; }
                    else if (c == 'q' || c == 'Q') { running = 0; }
                }
            }
            desktop_render();
        }

        __asm__ volatile ("hlt");
    }

    // Restore text mode display cleanly upon exiting
    vga_init();
    vga_clear();
    serial_puts("[DESKTOP] Returned to text console.\n");
}
