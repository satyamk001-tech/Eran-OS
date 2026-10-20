#include "snake.h"
#include "../drivers/gfx.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/serial.h"
#include "../drivers/timer.h"
#include "../drivers/audio.h"

#define ARENA_X 3
#define ARENA_Y 2
#define ARENA_W 50
#define ARENA_H 21
#define MAX_SNAKE 256

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} snake_dir_t;

typedef struct {
    int x;
    int y;
} point_t;

static uint32_t snake_rng = 987654321;
static uint32_t snake_rand(void) {
    snake_rng ^= snake_rng << 13;
    snake_rng ^= snake_rng >> 17;
    snake_rng ^= snake_rng << 5;
    return snake_rng;
}

static void snake_int_to_str(int val, char *buf) {
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

void snake_game_run(void) {
    gfx_init();
    snake_rng = timer_get_ticks() + 12345;

    point_t body[MAX_SNAKE];
    int length = 4;
    int score = 0;
    int high_score = 0;
    snake_dir_t dir = DIR_RIGHT;
    snake_dir_t next_dir = DIR_RIGHT;

    // Start in center of arena
    int start_x = ARENA_X + ARENA_W / 2;
    int start_y = ARENA_Y + ARENA_H / 2;
    for (int i = 0; i < length; i++) {
        body[i].x = start_x - i;
        body[i].y = start_y;
    }

    // Spawn initial food
    int food_x = ARENA_X + 1 + (snake_rand() % (ARENA_W - 2));
    int food_y = ARENA_Y + 1 + (snake_rand() % (ARENA_H - 2));

    int running = 1;
    int game_over = 0;
    int paused = 0;
    uint32_t tick_delay = 90; // ms per frame

    while (running) {
        // --- Input Processing ---
        while (keyboard_has_char() || serial_has_rx()) {
            char c = 0;
            if (keyboard_has_char()) c = keyboard_getc();
            else if (serial_has_rx()) c = serial_getc();

            if (c == 'q' || c == 'Q' || c == 27) { // Q or ESC
                running = 0;
                break;
            } else if (c == 'p' || c == 'P') {
                paused = !paused;
            } else if (game_over) {
                if (c == ' ' || c == '\n' || c == 'r' || c == 'R') {
                    // Restart
                    length = 4;
                    score = 0;
                    dir = DIR_RIGHT;
                    next_dir = DIR_RIGHT;
                    game_over = 0;
                    tick_delay = 90;
                    for (int i = 0; i < length; i++) {
                        body[i].x = start_x - i;
                        body[i].y = start_y;
                    }
                    food_x = ARENA_X + 1 + (snake_rand() % (ARENA_W - 2));
                    food_y = ARENA_Y + 1 + (snake_rand() % (ARENA_H - 2));
                }
            } else {
                if ((c == 'w' || c == 'W' || c == 'i') && dir != DIR_DOWN) next_dir = DIR_UP;
                else if ((c == 's' || c == 'S' || c == 'k') && dir != DIR_UP) next_dir = DIR_DOWN;
                else if ((c == 'a' || c == 'A' || c == 'j') && dir != DIR_RIGHT) next_dir = DIR_LEFT;
                else if ((c == 'd' || c == 'D' || c == 'l') && dir != DIR_LEFT) next_dir = DIR_RIGHT;
            }
        }

        if (!running) break;

        // --- Game Logic Update ---
        if (!game_over && !paused) {
            dir = next_dir;

            // Calculate new head position
            int new_x = body[0].x;
            int new_y = body[0].y;

            if (dir == DIR_UP) new_y--;
            else if (dir == DIR_DOWN) new_y++;
            else if (dir == DIR_LEFT) new_x--;
            else if (dir == DIR_RIGHT) new_x++;

            // Check Wall Collisions
            if (new_x <= ARENA_X || new_x >= ARENA_X + ARENA_W - 1 ||
                new_y <= ARENA_Y || new_y >= ARENA_Y + ARENA_H - 1) {
                game_over = 1;
                audio_play_tone(220, 180);
            }

            // Check Self Collision
            if (!game_over) {
                for (int i = 0; i < length; i++) {
                    if (body[i].x == new_x && body[i].y == new_y) {
                        game_over = 1;
                        audio_play_tone(220, 180);
                        break;
                    }
                }
            }

            // Advance Snake
            if (!game_over) {
                // Check Food Collision
                int ate_food = (new_x == food_x && new_y == food_y);

                if (ate_food) {
                    score += 10;
                    if (score > high_score) high_score = score;
                    if (length < MAX_SNAKE - 1) length++;

                    // Sound effect
                    audio_play_tone(1100, 20);
                    audio_play_tone(1600, 25);

                    // Speed up slightly
                    if (tick_delay > 45) tick_delay -= 2;

                    // Respawn Food
                    int valid = 0;
                    while (!valid) {
                        food_x = ARENA_X + 1 + (snake_rand() % (ARENA_W - 2));
                        food_y = ARENA_Y + 1 + (snake_rand() % (ARENA_H - 2));
                        valid = 1;
                        for (int i = 0; i < length; i++) {
                            if (body[i].x == food_x && body[i].y == food_y) {
                                valid = 0;
                                break;
                            }
                        }
                    }
                }

                // Shift body
                for (int i = length - 1; i > 0; i--) {
                    body[i] = body[i - 1];
                }
                body[0].x = new_x;
                body[0].y = new_y;
            }
        }

        // --- Render Frame ---
        gfx_clear(VGA_COLOR_BLACK);

        // Draw Arena Double Box
        gfx_draw_double_box(ARENA_X, ARENA_Y, ARENA_W, ARENA_H, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK, " ERAN RETRO SNAKE ");

        // Draw Food (Apple)
        gfx_put_char(food_x, food_y, '*', VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

        // Draw Snake Body & Head
        for (int i = length - 1; i >= 0; i--) {
            if (i == 0) {
                gfx_put_char(body[i].x, body[i].y, '@', VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            } else {
                gfx_put_char(body[i].x, body[i].y, 'o', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            }
        }

        // Draw Side Info Panel
        int px = ARENA_X + ARENA_W + 2;
        int py = ARENA_Y;
        int pw = 23;
        int ph = ARENA_H;

        gfx_draw_box(px, py, pw, ph, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK, " STATS ");

        char num_buf[16];
        gfx_draw_string(px + 2, py + 2, "SCORE:", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        snake_int_to_str(score, num_buf);
        gfx_draw_string(px + 10, py + 2, num_buf, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

        gfx_draw_string(px + 2, py + 4, "HIGH :", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        snake_int_to_str(high_score, num_buf);
        gfx_draw_string(px + 10, py + 4, num_buf, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

        gfx_draw_string(px + 2, py + 6, "LEN  :", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        snake_int_to_str(length, num_buf);
        gfx_draw_string(px + 10, py + 6, num_buf, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

        gfx_draw_string(px + 2, py + 9, "CONTROLS:", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        gfx_draw_string(px + 2, py + 11, "W / Up   : Up", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        gfx_draw_string(px + 2, py + 12, "S / Down : Down", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        gfx_draw_string(px + 2, py + 13, "A / Left : Left", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        gfx_draw_string(px + 2, py + 14, "D / Right: Right", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        gfx_draw_string(px + 2, py + 16, "P : Pause", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        gfx_draw_string(px + 2, py + 17, "Q : Exit to OS", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

        // Status Messages
        if (paused) {
            gfx_draw_string(ARENA_X + 18, ARENA_Y + ARENA_H / 2, " [ GAME PAUSED ] ", VGA_COLOR_YELLOW, VGA_COLOR_BLUE);
        } else if (game_over) {
            int gbx = ARENA_X + 10;
            int gby = ARENA_Y + 7;
            gfx_draw_double_box(gbx, gby, 30, 7, VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, " GAME OVER ");
            gfx_draw_string(gbx + 4, gby + 2, "Final Score: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            snake_int_to_str(score, num_buf);
            gfx_draw_string(gbx + 17, gby + 2, num_buf, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            gfx_draw_string(gbx + 4, gby + 4, "[SPACE] Play Again", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            gfx_draw_string(gbx + 4, gby + 5, "[Q] Return to Shell", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        }

        gfx_flip_to_screen();
        sleep_ms(tick_delay);
    }

    // Clean up screen when exiting
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("[SNAKE] Game exited. Final Score: %d | High Score: %d\n", score, high_score);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}
