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
#include "../net/browser.h"
#include "../games/snake.h"
#include "../games/snakes_ladders.h"
#include "../games/space_impact.h"

// ---------------------------------------------------------------------------
// Window Types & Global Desktop State
// ---------------------------------------------------------------------------
typedef enum {
    WIN_HOME = 0,
    WIN_TERMINAL = 1,
    WIN_BROWSER = 2,
    WIN_GAMES = 3,
    WIN_FILES = 4,
    WIN_AI = 5,
    WIN_TELEMETRY = 6,
    WIN_COUNT = 7
} win_type_t;

static int active_win = WIN_HOME;
static int is_maximized = 1;
static int start_menu_open = 0;
static int mouse_click_prev = 0;

// Home input buffer (for typing directly from dashboard)
static char home_input[64] = "";
static int  home_input_len = 0;

// Browser input state in GUI
static char browser_url_input[64] = "http://eranos.local/";
static int  browser_url_len = 20;

// Terminal current directory in GUI
static char term_cwd[32] = "/";

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
            ai_add_line("AI: VFS mounted at /. Mini Linux /bin active.");
            break;
        case INTENT_NETWORK_DIAGNOSTICS:
            ai_add_line("AI: eth0: 192.168.1.50/24, Gateway 192.168.1.1 OK.");
            break;
        case INTENT_AUDIO_SOUND:
            audio_chime();
            ai_add_line("AI: Melodic sequence synthesized on PC Speaker.");
            break;
        case INTENT_GUI_DESKTOP:
            ai_add_line("AI: 2D Desktop Compositor active with Web Browser.");
            break;
        case INTENT_SECURITY_PRIVILEGE:
            ai_add_line("AI: Ring 0/3 CPU isolation active. INT 0x80 gate.");
            break;
        case INTENT_GENERAL_HELP:
        default:
            ai_add_line("AI: Ready. Ask about browser, linux, files, audio!");
            break;
    }
}

// ---------------------------------------------------------------------------
// Workstation 2: In-GUI Terminal Console Buffer & Mini Linux Shell
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

    char echo_buf[TERM_LINE_LEN] = "root@eranos:$ ";
    gui_strncpy(echo_buf + 14, cmd, TERM_LINE_LEN - 15);
    term_add_line(echo_buf);

    if (gui_strcmp(cmd, "help") == 0) {
        term_add_line("Mini Linux Commands:");
        term_add_line(" pwd, cd [dir], ls, cat <file>, browser [url]");
        term_add_line(" snake, snakes, spaceimpact, games, chime");
    } else if (gui_strcmp(cmd, "snake") == 0) {
        term_add_line("Launching Retro Snake Arcade...");
        snake_game_run();
    } else if (gui_strcmp(cmd, "snakes") == 0 || gui_strcmp(cmd, "snakeladder") == 0) {
        term_add_line("Launching Snakes & Ladders Board Game...");
        snakes_ladders_run();
    } else if (gui_strcmp(cmd, "spaceimpact") == 0 || gui_strcmp(cmd, "space") == 0) {
        term_add_line("Launching Space Impact Nokia 3310 Shooter...");
        space_impact_run();
    } else if (gui_strcmp(cmd, "games") == 0 || gui_strcmp(cmd, "arcade") == 0) {
        term_add_line("Eran OS Retro Games:");
        term_add_line(" 1. snake       - Nokia Retro Snake");
        term_add_line(" 2. snakes      - Snakes & Ladders (vs AI)");
        term_add_line(" 3. spaceimpact - Space Impact Nokia Shooter");
    } else if (gui_strcmp(cmd, "pwd") == 0) {
        term_add_line(term_cwd);
    } else if (gui_strncmp(cmd, "cd", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) {
        const char *arg = cmd + 2;
        while (*arg == ' ') arg++;
        if (*arg == '\0' || gui_strcmp(arg, "~") == 0) {
            gui_strcpy(term_cwd, "/home/user");
        } else if (gui_strcmp(arg, "/") == 0) {
            gui_strcpy(term_cwd, "/");
        } else {
            gui_strncpy(term_cwd, arg, sizeof(term_cwd) - 1);
        }
        char msg[TERM_LINE_LEN] = "Changed directory to ";
        gui_strncpy(msg + 21, term_cwd, 32);
        term_add_line(msg);
    } else if (gui_strncmp(cmd, "uname", 5) == 0) {
        term_add_line("Linux eranos 0.4.0 #1 SMP i386 GNU/Linux");
    } else if (gui_strcmp(cmd, "whoami") == 0) {
        term_add_line("root");
    } else if (gui_strcmp(cmd, "date") == 0) {
        char buf[TERM_LINE_LEN] = "Uptime: ";
        char num[12];
        gui_u_to_str(timer_get_uptime_seconds(), num);
        gui_strncpy(buf + 8, num, 10);
        gui_strncpy(buf + gui_strlen(buf), "s (ticks: ", 10);
        gui_u_to_str(timer_get_ticks(), num);
        gui_strncpy(buf + gui_strlen(buf), num, 10);
        gui_strncpy(buf + gui_strlen(buf), ")", 2);
        term_add_line(buf);
    } else if (gui_strncmp(cmd, "browser", 7) == 0) {
        const char *arg = cmd + 7;
        while (*arg == ' ') arg++;
        if (*arg == '\0') arg = "http://eranos.local/";
        browser_navigate(arg);
        const browser_page_t *bp = browser_get_current_page();
        char bmsg[TERM_LINE_LEN] = "Browser Title: ";
        gui_strncpy(bmsg + 15, bp->title, 38);
        term_add_line(bmsg);
        for (int i = 0; i < bp->line_count && i < 3; i++) {
            term_add_line(bp->lines[i]);
        }
    } else if (gui_strcmp(cmd, "uptime") == 0) {
        char buf[TERM_LINE_LEN] = "Uptime: ";
        char num[12];
        gui_u_to_str(timer_get_uptime_seconds(), num);
        gui_strncpy(buf + 8, num, 10);
        gui_strncpy(buf + gui_strlen(buf), "s", 3);
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
    } else if (gui_strcmp(cmd, "ls") == 0) {
        term_add_line("Directory Listing:");
        vfs_node_t *r = vfs_find_node(term_cwd);
        if (!r) r = vfs_get_root();
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
// Workstation 4: VFS File Explorer State & Previews
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

    vfs_node_t *www = vfs_find_node("/var/www");
    if (www && www->children) {
        curr = www->children;
        while (curr && file_item_count < FILE_MAX_ITEMS) {
            char path[32] = "www/";
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

    char full_path[48] = "/";
    const char *sel = file_names[file_selected_idx];
    if (gui_strncmp(sel, "bin/", 4) == 0) {
        gui_strcpy(full_path, "/bin/");
        gui_strcpy(full_path + 5, sel + 4);
    } else if (gui_strncmp(sel, "www/", 4) == 0) {
        gui_strcpy(full_path, "/var/www/");
        gui_strcpy(full_path + 9, sel + 4);
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

    gfx_draw_string(x + 2, y + 2, "CPU Architecture", VGA_COLOR_LIGHT_CYAN, bg);
    gfx_draw_string(x + 2, y + 3, "x86 32-bit (x86_64 Capable)", VGA_COLOR_WHITE, bg);

    uint32_t total_ram = pmm_get_total_memory_kb() / 1024;
    uint32_t used_ram = pmm_get_used_memory_kb() / 1024;
    int ram_pct = (total_ram > 0) ? (int)(used_ram * 100 / total_ram) : 10;
    gfx_draw_string(x + 2, y + 5, "RAM: 32MB Total", VGA_COLOR_LIGHT_GREEN, bg);
    gfx_draw_progress_bar(x + 2, y + 6, w - 4, ram_pct, VGA_COLOR_LIGHT_GREEN, bg);

    size_t heap_used = heap_get_used();
    size_t heap_free = heap_get_free();
    size_t heap_tot = heap_used + heap_free;
    int heap_pct = (heap_tot > 0) ? (int)(heap_used * 100 / heap_tot) : 5;
    gfx_draw_string(x + 2, y + 8, "Heap: 4MB Dynamic", VGA_COLOR_LIGHT_CYAN, bg);
    gfx_draw_progress_bar(x + 2, y + 9, w - 4, heap_pct, VGA_COLOR_LIGHT_CYAN, bg);

    gfx_draw_string(x + 2, y + 11, "Battery: 94% [AC Online]", VGA_COLOR_LIGHT_BROWN, bg);
    gfx_draw_progress_bar(x + 2, y + 12, w - 4, 94, VGA_COLOR_LIGHT_GREEN, bg);

    gfx_draw_string(x + 2, y + 14, "Net : 192.168.1.50 (e1000)", VGA_COLOR_LIGHT_GREY, bg);
    gfx_draw_string(x + 2, y + 15, "Disk: ATA IDE 0x1F0 LBA", VGA_COLOR_LIGHT_GREY, bg);
    gfx_draw_string(x + 2, y + 16, "Web : EranWebKit v1.0", VGA_COLOR_LIGHT_CYAN, bg);

    char uptime_str[24] = "Uptime: ";
    char sec_buf[10];
    gui_u_to_str(timer_get_uptime_seconds(), sec_buf);
    gui_strncpy(uptime_str + 8, sec_buf, 6);
    gui_strncpy(uptime_str + gui_strlen(uptime_str), " sec", 5);
    gfx_draw_string(x + 2, y + 18, uptime_str, VGA_COLOR_WHITE, bg);

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
// Workstation 0: Simple Home Dashboard & Application Launcher
// ---------------------------------------------------------------------------
static void draw_home_pane(int x, int y, int w, int h) {
    (void)h;
    uint8_t bg = VGA_COLOR_BLACK;

    gfx_draw_string(x + 2, y + 2, "WELCOME TO ERAN OS WORKSTATION", VGA_COLOR_LIGHT_CYAN, bg);
    gfx_draw_string(x + 33, y + 2, "| Simple, Clean & Fast", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 2, y + 3, "Click any card with mouse or press key [1 - 6] to launch app:", VGA_COLOR_LIGHT_GREY, bg);

    // Row 1 Cards
    // Card 1: Terminal
    gfx_draw_double_box(x + 2, y + 5, 23, 4, VGA_COLOR_LIGHT_GREEN, bg, " [1] TERMINAL ");
    gfx_draw_string(x + 4, y + 6, "> Mini Linux Shell", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 4, y + 7, "  pwd, ls, cat, etc", VGA_COLOR_DARK_GREY, bg);

    // Card 2: Web Browser
    gfx_draw_double_box(x + 27, y + 5, 23, 4, VGA_COLOR_LIGHT_CYAN, bg, " [2] WEB BROWSER ");
    gfx_draw_string(x + 29, y + 6, "> HTML Web & Search", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 29, y + 7, "  EranSearch & Docs", VGA_COLOR_DARK_GREY, bg);

    // Card 3: Retro Games
    gfx_draw_double_box(x + 52, y + 5, 23, 4, VGA_COLOR_YELLOW, bg, " [3] RETRO GAMES ");
    gfx_draw_string(x + 54, y + 6, "> Snake & Space", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 54, y + 7, "  Nokia Arcade Hub", VGA_COLOR_DARK_GREY, bg);

    // Row 2 Cards
    // Card 4: AI Assistant
    gfx_draw_double_box(x + 2, y + 10, 23, 4, VGA_COLOR_LIGHT_MAGENTA, bg, " [4] AI ASSISTANT ");
    gfx_draw_string(x + 4, y + 11, "> Micro-Neural AI", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 4, y + 12, "  Offline inference", VGA_COLOR_DARK_GREY, bg);

    // Card 5: File Explorer
    gfx_draw_double_box(x + 27, y + 10, 23, 4, VGA_COLOR_LIGHT_BLUE, bg, " [5] FILE EXPLORER ");
    gfx_draw_string(x + 29, y + 11, "> Virtual Filesystem", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 29, y + 12, "  /bin, /etc, /var", VGA_COLOR_DARK_GREY, bg);

    // Card 6: System Monitor
    gfx_draw_double_box(x + 52, y + 10, 23, 4, VGA_COLOR_WHITE, bg, " [6] MONITOR / HAL ");
    gfx_draw_string(x + 54, y + 11, "> System Telemetry", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 54, y + 12, "  RAM & CPU health", VGA_COLOR_DARK_GREY, bg);

    // Help & Quick Guidance
    gfx_draw_box(x + 2, y + 15, w - 4, 4, VGA_COLOR_CYAN, bg, " Quick Guidance ");
    if (home_input_len > 0) {
        gfx_draw_string(x + 4, y + 16, "Quick Command: ", VGA_COLOR_YELLOW, bg);
        gfx_draw_string(x + 19, y + 16, home_input, VGA_COLOR_WHITE, bg);
        gfx_put_char(x + 19 + home_input_len, y + 16, '_', VGA_COLOR_LIGHT_GREEN, bg);
        gfx_draw_string(x + 4, y + 17, "[Press ENTER to execute in Mini Linux Shell | ESC to clear]", VGA_COLOR_LIGHT_GREY, bg);
    } else {
        gfx_draw_string(x + 4, y + 16, "* Left-click any card or bottom taskbar button to switch apps.", VGA_COLOR_LIGHT_GREEN, bg);
        gfx_draw_string(x + 4, y + 17, "* Click [START] bottom-left for full menu | Press [ESC] to exit.", VGA_COLOR_LIGHT_GREY, bg);
    }
}

// ---------------------------------------------------------------------------
// Workstation 4: Retro Arcade Games Hub
// ---------------------------------------------------------------------------
static void draw_games_pane(int x, int y, int w, int h) {
    (void)h;
    uint8_t bg = VGA_COLOR_BLACK;

    gfx_draw_string(x + 2, y + 2, "ERAN OS RETRO ARCADE HUB", VGA_COLOR_YELLOW, bg);
    gfx_draw_string(x + 28, y + 2, "| Pure Retro Gaming with PC Speaker Audio", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 2, y + 3, "Click a game or press key 1, 2, or 3 to play immediately:", VGA_COLOR_LIGHT_GREY, bg);

    // Game 1: Snake
    gfx_draw_double_box(x + 2, y + 5, w - 4, 4, VGA_COLOR_LIGHT_GREEN, bg, " [1] NOKIA RETRO SNAKE ");
    gfx_draw_string(x + 4, y + 6, "> Classic arcade snake! Eat apples, grow longer, avoid walls.", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 4, y + 7, "  Controls: W/A/S/D or Arrow keys to steer, Q to return to desktop.", VGA_COLOR_LIGHT_GREEN, bg);

    // Game 2: Snakes & Ladders
    gfx_draw_double_box(x + 2, y + 10, w - 4, 4, VGA_COLOR_LIGHT_CYAN, bg, " [2] SNAKES & LADDERS (100 SQUARES) ");
    gfx_draw_string(x + 4, y + 11, "> Traditional board game against Kernel AI! Ladders go up, snakes go down.", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 4, y + 12, "  Controls: SPACE / ENTER to roll dice, Q to return to desktop.", VGA_COLOR_LIGHT_CYAN, bg);

    // Game 3: Space Impact
    gfx_draw_double_box(x + 2, y + 15, w - 4, 4, VGA_COLOR_LIGHT_RED, bg, " [3] SPACE IMPACT (NOKIA 3310 SHOOTER) ");
    gfx_draw_string(x + 4, y + 16, "> Legendary space shooter! Blast enemy fleets, drop smart bombs, defeat Boss.", VGA_COLOR_WHITE, bg);
    gfx_draw_string(x + 4, y + 17, "  Controls: W/S/A/D to fly, SPACE for lasers, B for bomb, Q to return.", VGA_COLOR_LIGHT_RED, bg);
}

// ---------------------------------------------------------------------------
// Workstation 3: Web Browser Canvas Renderer
// ---------------------------------------------------------------------------
static void draw_browser_pane(int x, int y, int w, int h) {
    uint8_t bg = VGA_COLOR_BLACK;

    // Header & Document Title
    const browser_page_t *bp = browser_get_current_page();
    gfx_draw_string(x + 2, y + 2, "WebKit v1.0 |", VGA_COLOR_LIGHT_CYAN, bg);
    gfx_draw_string(x + 16, y + 2, bp->title, VGA_COLOR_WHITE, bg);

    // Address Bar
    gfx_fill_rect(x + 2, y + 3, w - 4, 1, ' ', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
    gfx_draw_string(x + 2, y + 3, "URL:", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_DARK_GREY);
    gfx_draw_string(x + 7, y + 3, browser_url_input, VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
    if (active_win == WIN_BROWSER) {
        gfx_put_char(x + 7 + browser_url_len, y + 3, '_', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_DARK_GREY);
    }
    if (w >= 36) {
        gfx_draw_string(x + w - 7, y + 3, "[GO]", VGA_COLOR_BLACK, VGA_COLOR_CYAN);
    }

    // Navigation Buttons Toolbar
    uint8_t back_fg = browser_can_go_back() ? VGA_COLOR_BLACK : VGA_COLOR_DARK_GREY;
    uint8_t back_bg = browser_can_go_back() ? VGA_COLOR_LIGHT_GREY : VGA_COLOR_BLACK;
    gfx_draw_string(x + 2, y + 4, "[Back]", back_fg, back_bg);
    gfx_draw_string(x + 9, y + 4, "[Home]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(x + 16, y + 4, "[About]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(x + 24, y + 4, "[Help]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    if (w >= 48) {
        gfx_draw_string(x + 31, y + 4, "[Search]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
        gfx_draw_string(x + 40, y + 4, "[Games]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    }
    if (w >= 60) {
        uint8_t st_color = (bp->status_code == 200) ? VGA_COLOR_LIGHT_GREEN : VGA_COLOR_LIGHT_RED;
        const char *st_text = (bp->status_code == 200) ? "[200 OK]" : "[Notice]";
        gfx_draw_string(x + 49, y + 4, st_text, st_color, bg);
    }

    // Rendered Web Page Viewport
    for (int i = 0; i < bp->line_count && y + 6 + i < y + h - 2; i++) {
        uint8_t fg = VGA_COLOR_LIGHT_GREY;
        if (bp->lines[i][0] == '=' && bp->lines[i][1] == '=') fg = VGA_COLOR_LIGHT_CYAN;
        else if (bp->lines[i][0] == '*') fg = VGA_COLOR_WHITE;
        else if (bp->lines[i][0] == '-' && bp->lines[i][1] == '-') fg = VGA_COLOR_DARK_GREY;

        // Highlight lines containing interactive hyperlinks
        for (int l = 0; l < bp->link_count; l++) {
            if (bp->links[l].line == i) {
                fg = VGA_COLOR_LIGHT_GREEN;
                break;
            }
        }

        gfx_draw_string(x + 2, y + 6 + i, bp->lines[i], fg, bg);
    }

    gfx_draw_string(x + 2, y + h - 2, "[Click [GO]/[Links] or type query & press ENTER]", VGA_COLOR_DARK_GREY, bg);
}

// ---------------------------------------------------------------------------
// Desktop Initialization
// ---------------------------------------------------------------------------
void desktop_init(void) {
    gfx_init();
    browser_init();

    // Default AI lines
    ai_line_count = 0;
    ai_add_line("AI: Welcome to Eran OS Mini Linux Assistant!");
    ai_add_line("AI: Embedded Micro-Neural Model loaded offline.");
    ai_add_line("AI: Ask about browser, linux, files, or audio.");

    // Default Terminal lines
    term_line_count = 0;
    term_add_line("Eran OS Mini Linux Workstation Shell Console");
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
    // 1. Wallpaper Background (Deep Royal Blue with light blue micro-dots)
    gfx_clear(VGA_COLOR_BLUE);
    for (int y = 1; y < CANVAS_HEIGHT - 1; y += 2) {
        for (int x = 2; x < CANVAS_WIDTH - 2; x += 4) {
            gfx_put_char(x, y, (char)0xFA, VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLUE);
        }
    }

    // 2. Top Application Menu Bar
    gfx_fill_rect(0, 0, CANVAS_WIDTH, 1, ' ', VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(1, 0, "[ ERAN OS ]",
                    start_menu_open ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    start_menu_open ? VGA_COLOR_RED : VGA_COLOR_CYAN);

    gfx_draw_string(13, 0, "Home",
                    (active_win == WIN_HOME) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_HOME) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(19, 0, "Terminal",
                    (active_win == WIN_TERMINAL) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_TERMINAL) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(29, 0, "Browser",
                    (active_win == WIN_BROWSER) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_BROWSER) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(38, 0, "Games",
                    (active_win == WIN_GAMES) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_GAMES) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(45, 0, "Files",
                    (active_win == WIN_FILES) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_FILES) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(52, 0, "AI",
                    (active_win == WIN_AI) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_AI) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(56, 0, "Monitor",
                    (active_win == WIN_TELEMETRY) ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    (active_win == WIN_TELEMETRY) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(65, 0, "[Audio]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(73, 0, "[Exit]", VGA_COLOR_LIGHT_RED, VGA_COLOR_LIGHT_GREY);

    // 3. Render Windows Layout
    int wx = 1, wy = 2, ww = 77, wh = 21;
    gfx_draw_shadow(wx, wy, ww, wh);

    const char *title = (active_win == WIN_HOME) ? " Eran OS Workstation Home Dashboard " :
                        (active_win == WIN_TERMINAL) ? " Interactive Mini Linux Terminal " :
                        (active_win == WIN_BROWSER) ? " Eran Web Browser & Search " :
                        (active_win == WIN_GAMES) ? " Eran OS Retro Arcade Games " :
                        (active_win == WIN_FILES) ? " VFS File Explorer " :
                        (active_win == WIN_AI) ? " Eran AI Productivity Core " :
                        " System Telemetry & Hardware HAL ";

    gfx_draw_double_box(wx, wy, ww, wh, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK, title);

    if (active_win == WIN_HOME) {
        draw_home_pane(wx, wy, ww, wh);
    } else if (active_win == WIN_GAMES) {
        draw_games_pane(wx, wy, ww, wh);
    } else if (active_win == WIN_TERMINAL) {
        gfx_draw_string(wx + 2, wy + 2, "Eran OS Mini Linux Terminal Console", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        for (int i = 0; i < term_line_count; i++) {
            uint8_t fg = (term_lines[i][0] == 'r') ? VGA_COLOR_WHITE : VGA_COLOR_LIGHT_GREY;
            gfx_draw_string(wx + 2, wy + 4 + i, term_lines[i], fg, VGA_COLOR_BLACK);
        }
        gfx_fill_rect(wx + 2, wy + wh - 3, ww - 4, 1, ' ', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
        gfx_draw_string(wx + 2, wy + wh - 3, "$ ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_DARK_GREY);
        gfx_draw_string(wx + 4, wy + wh - 3, term_input, VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
        gfx_put_char(wx + 4 + term_input_len, wy + wh - 3, '_', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
        gfx_draw_string(wx + 2, wy + wh - 2, "[Type: pwd, cd, ls, cat, browser, snake, spaceimpact, games, help]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    } else if (active_win == WIN_BROWSER) {
        draw_browser_pane(wx, wy, ww, wh);
    } else if (active_win == WIN_FILES) {
        gfx_draw_string(wx + 2, wy + 2, "Virtual Filesystem Explorer [/]", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        for (int i = 0; i < file_item_count; i++) {
            uint8_t fg = (i == file_selected_idx) ? VGA_COLOR_BLACK : VGA_COLOR_WHITE;
            uint8_t bg = (i == file_selected_idx) ? VGA_COLOR_CYAN : VGA_COLOR_BLACK;
            char entry[48] = "  ";
            if (file_is_dir[i]) gui_strncpy(entry + 2, "[DIR]  ", 8);
            else gui_strncpy(entry + 2, "[FILE] ", 8);
            gui_strncpy(entry + gui_strlen(entry), file_names[i], 24);
            gfx_draw_string(wx + 2, wy + 4 + i, entry, fg, bg);
        }
        gfx_draw_box(wx + 38, wy + 4, 36, 12, VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK, " Preview ");
        for (int p = 0; p < file_preview_count; p++) {
            gfx_draw_string(wx + 40, wy + 6 + p, file_preview[p], VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        }
        gfx_draw_string(wx + 2, wy + wh - 2, "[Keys: W/S or UP/DOWN to select, ENTER to view preview]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    } else if (active_win == WIN_AI) {
        gfx_draw_string(wx + 2, wy + 2, ai_badge, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREEN);
        for (int i = 0; i < ai_line_count; i++) {
            uint8_t fg = (ai_lines[i][0] == 'Y') ? VGA_COLOR_WHITE : VGA_COLOR_LIGHT_CYAN;
            gfx_draw_string(wx + 2, wy + 4 + i, ai_lines[i], fg, VGA_COLOR_BLACK);
        }
        gfx_fill_rect(wx + 2, wy + wh - 3, ww - 4, 1, ' ', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
        gfx_draw_string(wx + 2, wy + wh - 3, "> ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_DARK_GREY);
        gfx_draw_string(wx + 4, wy + wh - 3, ai_input, VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
        gfx_put_char(wx + 4 + ai_input_len, wy + wh - 3, '_', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_DARK_GREY);
        gfx_draw_string(wx + 2, wy + wh - 2, "[Type query & press ENTER. Try 'browser', 'memory', 'files']", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    } else {
        draw_telemetry_pane(wx, wy, ww, wh);
    }

    // 4. Start Menu Pop-up (when active)
    if (start_menu_open) {
        int smx = 1, smy = 7, smw = 36, smh = 16;
        gfx_draw_shadow(smx, smy, smw, smh);
        gfx_draw_double_box(smx, smy, smw, smh, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE, " ERAN OS MENU ");

        gfx_draw_string(smx + 3, smy + 2,  "[1] Home Dashboard", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 3,  "[2] Terminal Shell", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 4,  "[3] Web Browser & Search", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 5,  "[4] Retro Games Hub", VGA_COLOR_YELLOW, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 6,  "[5] Virtual Filesystem", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 7,  "[6] AI Assistant", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 8,  "[7] Hardware Monitor", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 2, smy + 9,  "--------------------------------", VGA_COLOR_CYAN, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 10, "[8] Play Melodic Chime", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 11, "[9] System Power Off", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 12, "[0] System Reboot", VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 13, "[X] Exit to Shell (ESC)", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        gfx_draw_string(smx + 3, smy + 14, "Click item or press [0 - 9]", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE);
    }

    // 5. Bottom Taskbar
    gfx_fill_rect(0, CANVAS_HEIGHT - 1, CANVAS_WIDTH, 1, ' ', VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);

    // Start Button
    gfx_draw_string(1, CANVAS_HEIGHT - 1, "[ START ]",
                    start_menu_open ? VGA_COLOR_WHITE : VGA_COLOR_BLACK,
                    start_menu_open ? VGA_COLOR_RED : VGA_COLOR_CYAN);

    // Window Tabs
    gfx_draw_string(11, CANVAS_HEIGHT - 1, "[Home]",
                    (active_win == WIN_HOME) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_HOME) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(18, CANVAS_HEIGHT - 1, "[Term]",
                    (active_win == WIN_TERMINAL) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_TERMINAL) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(25, CANVAS_HEIGHT - 1, "[Web]",
                    (active_win == WIN_BROWSER) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_BROWSER) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(31, CANVAS_HEIGHT - 1, "[Games]",
                    (active_win == WIN_GAMES) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_GAMES) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(39, CANVAS_HEIGHT - 1, "[Files]",
                    (active_win == WIN_FILES) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_FILES) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(47, CANVAS_HEIGHT - 1, "[AI]",
                    (active_win == WIN_AI) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_AI) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(52, CANVAS_HEIGHT - 1, "[Stats]",
                    (active_win == WIN_TELEMETRY) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win == WIN_TELEMETRY) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(60, CANVAS_HEIGHT - 1, "[Chime]", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(68, CANVAS_HEIGHT - 1, "[ESC:Exit]", VGA_COLOR_WHITE, VGA_COLOR_RED);

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
                        if (ms->x < 11) {
                            start_menu_open = !start_menu_open;
                        } else if (ms->x >= 11 && ms->x < 18) {
                            active_win = WIN_HOME; start_menu_open = 0;
                        } else if (ms->x >= 18 && ms->x < 25) {
                            active_win = WIN_TERMINAL; start_menu_open = 0;
                        } else if (ms->x >= 25 && ms->x < 31) {
                            active_win = WIN_BROWSER; start_menu_open = 0;
                        } else if (ms->x >= 31 && ms->x < 39) {
                            active_win = WIN_GAMES; start_menu_open = 0;
                        } else if (ms->x >= 39 && ms->x < 47) {
                            active_win = WIN_FILES; start_menu_open = 0; file_refresh_list(); file_update_preview();
                        } else if (ms->x >= 47 && ms->x < 52) {
                            active_win = WIN_AI; start_menu_open = 0;
                        } else if (ms->x >= 52 && ms->x < 60) {
                            active_win = WIN_TELEMETRY; start_menu_open = 0;
                        } else if (ms->x >= 60 && ms->x < 68) {
                            audio_chime();
                        } else if (ms->x >= 68) {
                            running = 0;
                        }
                    }
                    // Check Top Menu Bar Clicks
                    else if (ms->y == 0) {
                        if (ms->x < 13) {
                            start_menu_open = !start_menu_open;
                        } else if (ms->x >= 13 && ms->x < 19) {
                            active_win = WIN_HOME; start_menu_open = 0;
                        } else if (ms->x >= 19 && ms->x < 29) {
                            active_win = WIN_TERMINAL; start_menu_open = 0;
                        } else if (ms->x >= 29 && ms->x < 38) {
                            active_win = WIN_BROWSER; start_menu_open = 0;
                        } else if (ms->x >= 38 && ms->x < 45) {
                            active_win = WIN_GAMES; start_menu_open = 0;
                        } else if (ms->x >= 45 && ms->x < 52) {
                            active_win = WIN_FILES; start_menu_open = 0; file_refresh_list(); file_update_preview();
                        } else if (ms->x >= 52 && ms->x < 56) {
                            active_win = WIN_AI; start_menu_open = 0;
                        } else if (ms->x >= 56 && ms->x < 65) {
                            active_win = WIN_TELEMETRY; start_menu_open = 0;
                        } else if (ms->x >= 65 && ms->x < 73) {
                            audio_chime();
                        } else if (ms->x >= 73) {
                            running = 0;
                        }
                    }
                    // Check Start Menu Clicks
                    else if (start_menu_open) {
                        if (ms->x >= 1 && ms->x <= 37 && ms->y >= 10 && ms->y <= 21) {
                            if (ms->y == 10) { active_win = WIN_HOME; start_menu_open = 0; }
                            else if (ms->y == 11) { active_win = WIN_TERMINAL; start_menu_open = 0; }
                            else if (ms->y == 12) { active_win = WIN_BROWSER; start_menu_open = 0; }
                            else if (ms->y == 13) { active_win = WIN_GAMES; start_menu_open = 0; }
                            else if (ms->y == 14) { active_win = WIN_FILES; start_menu_open = 0; file_refresh_list(); file_update_preview(); }
                            else if (ms->y == 15) { active_win = WIN_AI; start_menu_open = 0; }
                            else if (ms->y == 16) { active_win = WIN_TELEMETRY; start_menu_open = 0; }
                            else if (ms->y == 18) { audio_chime(); start_menu_open = 0; }
                            else if (ms->y == 19) { acpi_poweroff(); }
                            else if (ms->y == 20) { acpi_reboot(); }
                            else if (ms->y == 21) { running = 0; }
                        } else {
                            start_menu_open = 0;
                        }
                    }
                    // Check Home Dashboard Application Card Clicks
                    else if (active_win == WIN_HOME) {
                        if (ms->y >= 7 && ms->y <= 10) {
                            if (ms->x >= 3 && ms->x <= 26) {
                                active_win = WIN_TERMINAL;
                            } else if (ms->x >= 28 && ms->x <= 51) {
                                active_win = WIN_BROWSER;
                            } else if (ms->x >= 53 && ms->x <= 76) {
                                active_win = WIN_GAMES;
                            }
                        } else if (ms->y >= 12 && ms->y <= 15) {
                            if (ms->x >= 3 && ms->x <= 26) {
                                active_win = WIN_AI;
                            } else if (ms->x >= 28 && ms->x <= 51) {
                                active_win = WIN_FILES;
                                file_refresh_list();
                                file_update_preview();
                            } else if (ms->x >= 53 && ms->x <= 76) {
                                active_win = WIN_TELEMETRY;
                            }
                        }
                    }
                    // Check Retro Games Hub Card Clicks
                    else if (active_win == WIN_GAMES) {
                        if (ms->y >= 7 && ms->y <= 10 && ms->x >= 3 && ms->x <= 76) {
                            snake_game_run();
                            desktop_render();
                        } else if (ms->y >= 12 && ms->y <= 15 && ms->x >= 3 && ms->x <= 76) {
                            snakes_ladders_run();
                            desktop_render();
                        } else if (ms->y >= 17 && ms->y <= 20 && ms->x >= 3 && ms->x <= 76) {
                            space_impact_run();
                            desktop_render();
                        }
                    }
                    // Check Browser Navigation & Input Clicks
                    else if (active_win == WIN_BROWSER) {
                        int cur_w = is_maximized ? 77 : 48;

                        // Row 5: Address Bar & [GO] button
                        if (ms->y == 5) {
                            if (ms->x >= cur_w - 7 && ms->x <= cur_w - 2) {
                                // Clicked [GO]
                                browser_navigate(browser_url_input);
                                const browser_page_t *p = browser_get_current_page();
                                gui_strncpy(browser_url_input, p->url, sizeof(browser_url_input));
                                browser_url_len = gui_strlen(browser_url_input);
                                audio_chime();
                            } else if (ms->x >= 3 && ms->x < cur_w - 8) {
                                // Clicked Address Bar: clear default to allow fast typing
                                if (gui_strncmp(browser_url_input, "http://eranos.local", 19) == 0) {
                                    browser_url_input[0] = '\0';
                                    browser_url_len = 0;
                                }
                            }
                        }
                        // Row 6: Navigation Buttons Toolbar
                        else if (ms->y == 6) {
                            if (ms->x >= 3 && ms->x <= 8) {
                                // [Back]
                                if (browser_can_go_back()) {
                                    browser_go_back();
                                    const browser_page_t *p = browser_get_current_page();
                                    gui_strncpy(browser_url_input, p->url, sizeof(browser_url_input));
                                    browser_url_len = gui_strlen(browser_url_input);
                                    audio_chime();
                                }
                            } else if (ms->x >= 10 && ms->x <= 15) {
                                // [Home]
                                browser_navigate("http://eranos.local/");
                                gui_strcpy(browser_url_input, "http://eranos.local/");
                                browser_url_len = gui_strlen(browser_url_input);
                            } else if (ms->x >= 17 && ms->x <= 23) {
                                // [About]
                                browser_navigate("http://eranos.local/about.html");
                                gui_strcpy(browser_url_input, "http://eranos.local/about.html");
                                browser_url_len = gui_strlen(browser_url_input);
                            } else if (ms->x >= 25 && ms->x <= 30) {
                                // [Help]
                                browser_navigate("http://eranos.local/help.html");
                                gui_strcpy(browser_url_input, "http://eranos.local/help.html");
                                browser_url_len = gui_strlen(browser_url_input);
                            } else if (ms->x >= 32 && ms->x <= 39) {
                                // [Search]
                                browser_navigate("search");
                                gui_strcpy(browser_url_input, "search");
                                browser_url_len = gui_strlen(browser_url_input);
                            } else if (ms->x >= 41 && ms->x <= 47) {
                                // [Games]
                                active_win = WIN_GAMES;
                            }
                        }
                        // Rows 8-21: Clickable Hyperlinks in Page Viewport
                        else if (ms->y >= 8 && ms->y <= 21 && ms->x >= 2 && ms->x < cur_w - 2) {
                            int line_idx = ms->y - 8;
                            if (browser_click_line(line_idx)) {
                                const browser_page_t *p = browser_get_current_page();
                                gui_strncpy(browser_url_input, p->url, sizeof(browser_url_input));
                                browser_url_len = gui_strlen(browser_url_input);
                                audio_chime();
                            }
                        }
                    }
                    // Check Window Controls (Close [■] or Title bar)
                    else if (ms->y == 2) {
                        if (ms->x >= 3 && ms->x <= 5) {
                            active_win = (active_win + 1) % WIN_COUNT;
                            if (active_win == WIN_FILES) {
                                file_refresh_list();
                                file_update_preview();
                            }
                        } else if (ms->x >= 73 && ms->x <= 76) {
                            running = 0;
                        }
                    }
                    // Check File row selection in Files window
                    else if (active_win == WIN_FILES && ms->y >= 6 && ms->y <= 18 && ms->x >= 3 && ms->x <= 36) {
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
                if (c == '1') { active_win = WIN_HOME; start_menu_open = 0; }
                else if (c == '2') { active_win = WIN_TERMINAL; start_menu_open = 0; }
                else if (c == '3') { active_win = WIN_BROWSER; start_menu_open = 0; }
                else if (c == '4') { active_win = WIN_GAMES; start_menu_open = 0; }
                else if (c == '5') { active_win = WIN_FILES; start_menu_open = 0; file_refresh_list(); file_update_preview(); }
                else if (c == '6') { active_win = WIN_AI; start_menu_open = 0; }
                else if (c == '7') { active_win = WIN_TELEMETRY; start_menu_open = 0; }
                else if (c == '8') { audio_chime(); start_menu_open = 0; }
                else if (c == '9') { acpi_poweroff(); }
                else if (c == '0') { acpi_reboot(); }
                else if (c == 'x' || c == 'X' || c == 27) { start_menu_open = 0; }
            } else {
                if (c == 27) { // ESC key
                    running = 0;
                } else if (c == '\t') { // TAB key cycles active window
                    active_win = (active_win + 1) % WIN_COUNT;
                    if (active_win == WIN_FILES) {
                        file_refresh_list();
                        file_update_preview();
                    }
                } else if (active_win == WIN_HOME) {
                    if (c == '\n') {
                        if (home_input_len > 0) {
                            active_win = WIN_TERMINAL;
                            term_execute_cmd(home_input);
                            home_input_len = 0;
                            home_input[0] = '\0';
                        } else {
                            active_win = WIN_TERMINAL;
                        }
                    } else if (c == '\b') {
                        if (home_input_len > 0) {
                            home_input_len--;
                            home_input[home_input_len] = '\0';
                        }
                    } else if (home_input_len == 0 && c == '1') {
                        active_win = WIN_TERMINAL;
                    } else if (home_input_len == 0 && c == '2') {
                        active_win = WIN_BROWSER;
                    } else if (home_input_len == 0 && c == '3') {
                        active_win = WIN_GAMES;
                    } else if (home_input_len == 0 && c == '4') {
                        active_win = WIN_AI;
                    } else if (home_input_len == 0 && c == '5') {
                        active_win = WIN_FILES;
                        file_refresh_list();
                        file_update_preview();
                    } else if (home_input_len == 0 && c == '6') {
                        active_win = WIN_TELEMETRY;
                    } else if (home_input_len == 0 && (c == 's' || c == 'S')) {
                        start_menu_open = !start_menu_open;
                    } else if (c >= 32 && c <= 126) {
                        if (home_input_len < 60) {
                            home_input[home_input_len++] = c;
                            home_input[home_input_len] = '\0';
                        }
                    }
                } else if (active_win == WIN_GAMES) {
                    if (c == '1') {
                        snake_game_run();
                        desktop_render();
                    } else if (c == '2') {
                        snakes_ladders_run();
                        desktop_render();
                    } else if (c == '3') {
                        space_impact_run();
                        desktop_render();
                    } else if (c == 'h' || c == 'H') {
                        active_win = WIN_HOME;
                    } else if (c == 'b' || c == 'B') {
                        active_win = WIN_BROWSER;
                    } else if (c == 't' || c == 'T') {
                        active_win = WIN_TERMINAL;
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
                } else if (active_win == WIN_BROWSER) {
                    if (c == '\n') {
                        browser_navigate(browser_url_input);
                        const browser_page_t *p = browser_get_current_page();
                        gui_strncpy(browser_url_input, p->url, sizeof(browser_url_input));
                        browser_url_len = gui_strlen(browser_url_input);
                    } else if (c == '\b') {
                        if (browser_url_len > 0) {
                            browser_url_len--;
                            browser_url_input[browser_url_len] = '\0';
                        }
                    } else if (c >= 32 && c <= 126) {
                        if (browser_url_len < 60) {
                            browser_url_input[browser_url_len++] = c;
                            browser_url_input[browser_url_len] = '\0';
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
                    } else if (c == '0' || c == 'h' || c == 'H') {
                        active_win = WIN_HOME;
                    } else if (c == '1') {
                        active_win = WIN_TERMINAL;
                    } else if (c == '2') {
                        active_win = WIN_BROWSER;
                    } else if (c == '3') {
                        active_win = WIN_GAMES;
                    } else if (c == '4') {
                        active_win = WIN_AI;
                    } else if (c == '6') {
                        active_win = WIN_TELEMETRY;
                    } else if (c == 'b' || c == 'B') {
                        audio_chime();
                    }
                } else if (active_win == WIN_TELEMETRY) {
                    if (c == '0' || c == 'h' || c == 'H') {
                        active_win = WIN_HOME;
                    } else if (c == '1') {
                        active_win = WIN_TERMINAL;
                    } else if (c == '2') {
                        active_win = WIN_BROWSER;
                    } else if (c == '3') {
                        active_win = WIN_GAMES;
                    } else if (c == '4') {
                        active_win = WIN_AI;
                    } else if (c == '5') {
                        active_win = WIN_FILES;
                        file_refresh_list();
                        file_update_preview();
                    } else if (c == 'b' || c == 'B') {
                        audio_chime();
                    } else if (c == 's' || c == 'S') {
                        start_menu_open = !start_menu_open;
                    }
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
