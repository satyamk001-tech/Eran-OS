#include "snakes_ladders.h"
#include "../drivers/gfx.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/serial.h"
#include "../drivers/timer.h"
#include "../drivers/audio.h"

#define BOARD_X 2
#define BOARD_Y 2
#define BOARD_W 42
#define BOARD_H 21

static uint32_t sl_rng = 543216789;
static uint32_t sl_rand(void) {
    sl_rng ^= sl_rng << 13;
    sl_rng ^= sl_rng >> 17;
    sl_rng ^= sl_rng << 5;
    return sl_rng;
}

// Ladders: Start -> End
static int check_ladder(int sq) {
    switch (sq) {
        case 4:  return 14;
        case 9:  return 31;
        case 20: return 38;
        case 28: return 84;
        case 40: return 59;
        case 51: return 67;
        case 63: return 81;
        case 71: return 91;
        default: return sq;
    }
}

// Snakes: Head -> Tail
static int check_snake(int sq) {
    switch (sq) {
        case 17: return 7;
        case 54: return 34;
        case 62: return 19;
        case 64: return 60;
        case 87: return 24;
        case 93: return 73;
        case 95: return 75;
        case 99: return 78;
        default: return sq;
    }
}

static void sl_strcpy(char *dest, const char *src) {
    if (!dest || !src) return;
    while ((*dest++ = *src++));
}

static void sl_int_to_str(int val, char *buf) {
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

static void play_ladder_sound(void) {
    audio_play_tone(523, 50); // C5
    audio_play_tone(659, 50); // E5
    audio_play_tone(784, 50); // G5
    audio_play_tone(1046, 90); // C6
}

static void play_snake_sound(void) {
    audio_play_tone(440, 60);
    audio_play_tone(330, 60);
    audio_play_tone(220, 100);
}

static void play_win_sound(void) {
    audio_play_tone(523, 70);
    audio_play_tone(659, 70);
    audio_play_tone(784, 70);
    audio_play_tone(1046, 180);
}

void snakes_ladders_run(void) {
    gfx_init();
    sl_rng = timer_get_ticks() + 98765;

    int player_pos = 1;
    int ai_pos = 1;
    int is_player_turn = 1;
    int last_dice = 0;
    int game_won = 0; // 1 = player won, 2 = AI won
    char log_msg1[40] = "Press [SPACE] to roll dice!";
    char log_msg2[40] = "First to square 100 wins!";

    int running = 1;
    while (running) {
        // --- Render Board & UI ---
        gfx_clear(VGA_COLOR_BLACK);

        // Draw Board Outer Frame
        gfx_draw_double_box(BOARD_X, BOARD_Y, BOARD_W, BOARD_H, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK, " SNAKES & LADDERS (100) ");

        // Draw 10x10 Grid
        // Rows from top (y=0: 100..91) down to bottom (y=9: 1..10)
        for (int r = 0; r < 10; r++) {
            int row_num = 9 - r; // 9 down to 0
            for (int c = 0; c < 10; c++) {
                int col_num = (row_num % 2 == 1) ? (9 - c) : c;
                int sq = row_num * 10 + col_num + 1; // 1 to 100

                int cell_x = BOARD_X + 1 + c * 4;
                int cell_y = BOARD_Y + 1 + r * 2;

                uint8_t fg = VGA_COLOR_DARK_GREY;
                uint8_t bg = VGA_COLOR_BLACK;
                char label[4] = "  ";

                // Check Special Board Properties
                if (check_ladder(sq) > sq) {
                    fg = VGA_COLOR_LIGHT_GREEN;
                    label[0] = 'L';
                } else if (check_snake(sq) < sq) {
                    fg = VGA_COLOR_LIGHT_RED;
                    label[0] = 'S';
                }

                // Check Pieces
                if (sq == player_pos && sq == ai_pos) {
                    fg = VGA_COLOR_WHITE;
                    bg = VGA_COLOR_MAGENTA;
                    label[0] = 'P';
                    label[1] = 'A';
                } else if (sq == player_pos) {
                    fg = VGA_COLOR_BLACK;
                    bg = VGA_COLOR_CYAN;
                    label[0] = 'P';
                    label[1] = '!';
                } else if (sq == ai_pos) {
                    fg = VGA_COLOR_BLACK;
                    bg = VGA_COLOR_YELLOW;
                    label[0] = 'A';
                    label[1] = 'I';
                }

                gfx_draw_string(cell_x, cell_y, label, fg, bg);
            }
        }

        // Draw Right Panel: Game Telemetry & Dice
        int rx = BOARD_X + BOARD_W + 2;
        int ry = BOARD_Y;
        int rw = 32;
        int rh = BOARD_H;

        gfx_draw_box(rx, ry, rw, rh, VGA_COLOR_YELLOW, VGA_COLOR_BLACK, " GAME PANEL ");

        char num_buf[16];
        gfx_draw_string(rx + 2, ry + 2, "PLAYER [P]: Square ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        sl_int_to_str(player_pos, num_buf);
        gfx_draw_string(rx + 21, ry + 2, num_buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        gfx_draw_string(rx + 2, ry + 4, "KERNEL [AI]: Square ", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        sl_int_to_str(ai_pos, num_buf);
        gfx_draw_string(rx + 22, ry + 4, num_buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        // Dice Graphic
        gfx_draw_box(rx + 9, ry + 6, 13, 5, VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY, " DICE ");
        if (last_dice > 0) {
            char dstr[4] = "[ ]";
            dstr[1] = (char)('0' + last_dice);
            gfx_draw_string(rx + 14, ry + 8, dstr, VGA_COLOR_YELLOW, VGA_COLOR_DARK_GREY);
        } else {
            gfx_draw_string(rx + 13, ry + 8, "[ ? ]", VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
        }

        // Action Status
        gfx_draw_string(rx + 2, ry + 12, "EVENT LOG:", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        gfx_draw_string(rx + 2, ry + 14, log_msg1, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        gfx_draw_string(rx + 2, ry + 15, log_msg2, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

        // Turn indicator
        if (!game_won) {
            if (is_player_turn) {
                gfx_draw_string(rx + 2, ry + 17, ">> YOUR TURN: Press [SPACE] <<", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            } else {
                gfx_draw_string(rx + 2, ry + 17, "... AI is rolling dice ...", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            }
        } else {
            if (game_won == 1) {
                gfx_draw_string(rx + 2, ry + 17, "🎉 CONGRATS! YOU WON! 🎉", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            } else {
                gfx_draw_string(rx + 2, ry + 17, "💀 KERNEL AI WON! 💀", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            }
            gfx_draw_string(rx + 2, ry + 18, "[SPACE] Play Again | [Q] Exit", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }

        gfx_draw_string(rx + 2, ry + 19, "[Q] Return to Shell / OS", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

        gfx_flip_to_screen();

        // --- Handle Turns & AI ---
        if (game_won) {
            // Wait for restart or quit
            char c = 0;
            if (keyboard_has_char()) c = keyboard_getc();
            else if (serial_has_rx()) c = serial_getc();

            if (c == 'q' || c == 'Q' || c == 27) {
                running = 0;
            } else if (c == ' ' || c == '\n') {
                player_pos = 1;
                ai_pos = 1;
                game_won = 0;
                is_player_turn = 1;
                last_dice = 0;
                for (int i = 0; i < 40; i++) { log_msg1[i] = '\0'; log_msg2[i] = '\0'; }
                sl_int_to_str(1, num_buf);
                // Reset msgs
                sl_strcpy(log_msg1, "Game restarted! Press SPACE");
            }
            sleep_ms(30);
            continue;
        }

        if (is_player_turn) {
            // Wait for Player Key
            char c = 0;
            if (keyboard_has_char()) c = keyboard_getc();
            else if (serial_has_rx()) c = serial_getc();

            if (c == 'q' || c == 'Q' || c == 27) {
                running = 0;
            } else if (c == ' ' || c == '\n' || c == 'r' || c == 'R') {
                // Roll dice
                audio_play_tone(800, 20);
                last_dice = 1 + (sl_rand() % 6);

                player_pos += last_dice;
                if (player_pos >= 100) {
                    player_pos = 100;
                    game_won = 1;
                    play_win_sound();
                } else {
                    // Check Ladder or Snake
                    int after_ladder = check_ladder(player_pos);
                    int after_snake = check_snake(player_pos);

                    if (after_ladder > player_pos) {
                        player_pos = after_ladder;
                        play_ladder_sound();
                        sl_strcpy(log_msg1, "LADDER! You climbed up!");
                    } else if (after_snake < player_pos) {
                        player_pos = after_snake;
                        play_snake_sound();
                        sl_strcpy(log_msg1, "SNAKE! Slid down!");
                    } else {
                        audio_play_tone(600, 30);
                        sl_strcpy(log_msg1, "You advanced smoothly.");
                    }
                }

                is_player_turn = 0; // Hand over to AI
            }
            sleep_ms(40);
        } else {
            // AI Turn (takes ~0.6 sec pause for realistic feel)
            sleep_ms(400);

            audio_play_tone(700, 20);
            last_dice = 1 + (sl_rand() % 6);

            ai_pos += last_dice;
            if (ai_pos >= 100) {
                ai_pos = 100;
                game_won = 2;
                audio_play_tone(220, 200);
            } else {
                int after_ladder = check_ladder(ai_pos);
                int after_snake = check_snake(ai_pos);

                if (after_ladder > ai_pos) {
                    ai_pos = after_ladder;
                    play_ladder_sound();
                    sl_strcpy(log_msg2, "AI hit a Ladder!");
                } else if (after_snake < ai_pos) {
                    ai_pos = after_snake;
                    play_snake_sound();
                    sl_strcpy(log_msg2, "AI got bitten by Snake!");
                } else {
                    sl_strcpy(log_msg2, "AI advanced.");
                }
            }

            is_player_turn = 1;
            sleep_ms(300);
        }
    }

    // Restore screen
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("[SNAKES & LADDERS] Exited back to Eran OS Shell.\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}
