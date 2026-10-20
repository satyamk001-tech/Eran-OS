#include "desktop.h"
#include "../drivers/gfx.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/audio.h"
#include "../mm/pmm.h"
#include "../process/process.h"

static desktop_window_t windows[2];
static int active_win_idx = 0;

void desktop_init(void) {
    gfx_init();

    windows[0].id = 1;
    windows[0].title = " System Telemetry & HAL ";
    windows[0].x = 3;
    windows[0].y = 2;
    windows[0].width = 36;
    windows[0].height = 18;
    windows[0].is_active = 1;
    windows[0].is_minimized = 0;

    windows[1].id = 2;
    windows[1].title = " Eran AI Productivity Core ";
    windows[1].x = 41;
    windows[1].y = 2;
    windows[1].width = 36;
    windows[1].height = 18;
    windows[1].is_active = 0;
    windows[1].is_minimized = 0;
}

void desktop_render(void) {
    // 1. Draw Desktop Wallpaper Background (Dark Blue with subtle dots)
    gfx_clear(VGA_COLOR_BLUE);
    for (int y = 1; y < CANVAS_HEIGHT - 1; y += 2) {
        for (int x = 2; x < CANVAS_WIDTH - 2; x += 4) {
            gfx_put_char(x, y, 0xFA, VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLUE); // 0xFA is middle dot
        }
    }

    // 2. Top Status Bar
    gfx_fill_rect(0, 0, CANVAS_WIDTH, 1, ' ', VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(2, 0, "ERAN OS 32-BIT DESKTOP ENVIRONMENT", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_draw_string(52, 0, "Security: RING 0/3 ISOLATED", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);

    // 3. Render Windows
    for (int i = 0; i < 2; i++) {
        desktop_window_t *w = &windows[i];
        uint8_t border_fg = (i == active_win_idx) ? VGA_COLOR_LIGHT_CYAN : VGA_COLOR_WHITE;
        uint8_t win_bg = VGA_COLOR_BLACK;

        gfx_draw_box(w->x, w->y, w->width, w->height, border_fg, win_bg, w->title);

        if (i == 0) {
            // Content for System Telemetry Window
            gfx_draw_string(w->x + 2, w->y + 2, "Architecture : x86 32-bit PM", VGA_COLOR_WHITE, win_bg);
            gfx_draw_string(w->x + 2, w->y + 4, "Physical RAM : 32 MB Total", VGA_COLOR_LIGHT_GREEN, win_bg);
            gfx_draw_string(w->x + 2, w->y + 6, "PMM Status   : 4KB Frame Alloc", VGA_COLOR_LIGHT_GREY, win_bg);
            gfx_draw_string(w->x + 2, w->y + 8, "Heap Manager : 4 MB BoundaryTag", VGA_COLOR_LIGHT_GREY, win_bg);
            gfx_draw_string(w->x + 2, w->y + 10, "Multitasking : Round-Robin Active", VGA_COLOR_LIGHT_CYAN, win_bg);
            gfx_draw_string(w->x + 2, w->y + 12, "Storage      : ATA IDE 0x1F0", VGA_COLOR_LIGHT_GREY, win_bg);
            gfx_draw_string(w->x + 2, w->y + 14, "Filesystem   : RamFS Mounted /", VGA_COLOR_LIGHT_GREY, win_bg);
            gfx_draw_string(w->x + 2, w->y + 16, "Network      : Intel e1000 1Gbe", VGA_COLOR_LIGHT_BROWN, win_bg);
        } else {
            // Content for AI Assistant Window
            gfx_draw_string(w->x + 2, w->y + 2, "AI Engine    : Native C NLP Core", VGA_COLOR_LIGHT_CYAN, win_bg);
            gfx_draw_string(w->x + 2, w->y + 4, "Execution    : Ring 3 Unprivileged", VGA_COLOR_LIGHT_GREEN, win_bg);
            gfx_draw_string(w->x + 2, w->y + 6, "Security Gate: INT 0x80 Syscalls", VGA_COLOR_WHITE, win_bg);
            gfx_draw_string(w->x + 2, w->y + 8, "Cloud Status : 100% OFFLINE", VGA_COLOR_LIGHT_GREEN, win_bg);
            gfx_draw_string(w->x + 2, w->y + 10, "Planner      : Schema-Validated", VGA_COLOR_LIGHT_GREY, win_bg);
            gfx_draw_string(w->x + 2, w->y + 12, "Path Sandbox : Traversal Blocked", VGA_COLOR_LIGHT_GREY, win_bg);
            gfx_draw_string(w->x + 2, w->y + 14, "Audio Speech : Sound Generator", VGA_COLOR_LIGHT_BROWN, win_bg);
            gfx_draw_string(w->x + 2, w->y + 16, "Status       : Ready for commands", VGA_COLOR_WHITE, win_bg);
        }
    }

    // 4. Bottom Taskbar
    gfx_fill_rect(0, CANVAS_HEIGHT - 1, CANVAS_WIDTH, 1, ' ', VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    gfx_fill_rect(0, CANVAS_HEIGHT - 1, 10, 1, ' ', VGA_COLOR_WHITE, VGA_COLOR_CYAN);
    gfx_draw_string(1, CANVAS_HEIGHT - 1, " START ", VGA_COLOR_BLACK, VGA_COLOR_CYAN);

    // Taskbar Tabs
    gfx_draw_string(12, CANVAS_HEIGHT - 1, "[1: Telemetry]",
                    (active_win_idx == 0) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win_idx == 0) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(28, CANVAS_HEIGHT - 1, "[2: AI Core]",
                    (active_win_idx == 1) ? VGA_COLOR_WHITE : VGA_COLOR_DARK_GREY,
                    (active_win_idx == 1) ? VGA_COLOR_BLUE : VGA_COLOR_LIGHT_GREY);

    gfx_draw_string(42, CANVAS_HEIGHT - 1, "Keys: [TAB] Win [B] Sound [Q] Exit", VGA_COLOR_DARK_GREY, VGA_COLOR_LIGHT_GREY);

    // 5. Blit Backbuffer to VGA text buffer
    gfx_flip_to_screen();
}

void desktop_interactive(void) {
    desktop_init();
    desktop_render();

    // Event loop for desktop UI
    int running = 1;
    while (running) {
        if (keyboard_has_char()) {
            char c = keyboard_getchar();
            if (c == 'q' || c == 'Q' || c == 27) { // 27 = ESC
                running = 0;
            } else if (c == '\t' || c == ' ') {
                active_win_idx = 1 - active_win_idx;
                desktop_render();
            } else if (c == 'b' || c == 'B') {
                audio_chime();
            }
        }
        __asm__ volatile ("hlt");
    }

    // Restore text console
    vga_init();
    vga_clear();
}
