#include "space_impact.h"
#include "../drivers/gfx.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/serial.h"
#include "../drivers/timer.h"
#include "../drivers/audio.h"

#define PLAYFIELD_X 1
#define PLAYFIELD_Y 2
#define PLAYFIELD_W 78
#define PLAYFIELD_H 21

#define MAX_PLAYER_LASERS 8
#define MAX_ENEMY_LASERS  8
#define MAX_ENEMIES       8
#define MAX_PARTICLES     16

typedef struct {
    int active;
    int x;
    int y;
} laser_t;

typedef struct {
    int active;
    int x;
    int y;
    int type; // 0=Scout, 1=Fighter, 2=Cruiser, 3=Asteroid, 4=Boss
    int hp;
    int max_hp;
    int fire_cooldown;
} enemy_t;

typedef struct {
    int active;
    int x;
    int y;
    int life;
    char ch;
    uint8_t color;
} particle_t;

static uint32_t si_rng = 432198765;
static uint32_t si_rand(void) {
    si_rng ^= si_rng << 13;
    si_rng ^= si_rng >> 17;
    si_rng ^= si_rng << 5;
    return si_rng;
}

static void si_int_to_str(int val, char *buf) {
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

void space_impact_run(void) {
    gfx_init();
    si_rng = timer_get_ticks() + 13579;

    // Player State
    int px = 4;
    int py = 12;
    int hp = 100;
    int lives = 3;
    int bombs = 2;
    int score = 0;
    int wave = 1;
    int enemies_defeated = 0;

    laser_t p_lasers[MAX_PLAYER_LASERS];
    for (int i = 0; i < MAX_PLAYER_LASERS; i++) p_lasers[i].active = 0;

    laser_t e_lasers[MAX_ENEMY_LASERS];
    for (int i = 0; i < MAX_ENEMY_LASERS; i++) e_lasers[i].active = 0;

    enemy_t enemies[MAX_ENEMIES];
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;

    particle_t particles[MAX_PARTICLES];
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = 0;

    int running = 1;
    int game_over = 0;
    int victory = 0;
    int boss_spawned = 0;

    while (running) {
        // --- Input Processing ---
        while (keyboard_has_char() || serial_has_rx()) {
            char c = 0;
            if (keyboard_has_char()) c = keyboard_getc();
            else if (serial_has_rx()) c = serial_getc();

            if (c == 'q' || c == 'Q' || c == 27) { // Q or ESC
                running = 0;
                break;
            } else if (game_over || victory) {
                if (c == ' ' || c == '\n' || c == 'r' || c == 'R') {
                    // Restart
                    px = 4;
                    py = 12;
                    hp = 100;
                    lives = 3;
                    bombs = 2;
                    score = 0;
                    wave = 1;
                    enemies_defeated = 0;
                    boss_spawned = 0;
                    game_over = 0;
                    victory = 0;
                    for (int i = 0; i < MAX_PLAYER_LASERS; i++) p_lasers[i].active = 0;
                    for (int i = 0; i < MAX_ENEMY_LASERS; i++) e_lasers[i].active = 0;
                    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;
                    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = 0;
                }
            } else {
                // Movement
                if ((c == 'w' || c == 'W' || c == 'i') && py > PLAYFIELD_Y + 1) py--;
                else if ((c == 's' || c == 'S' || c == 'k') && py < PLAYFIELD_Y + PLAYFIELD_H - 2) py++;
                else if ((c == 'a' || c == 'A' || c == 'j') && px > PLAYFIELD_X + 2) px -= 2;
                else if ((c == 'd' || c == 'D' || c == 'l') && px < PLAYFIELD_X + 30) px += 2;
                // Fire Laser
                else if (c == ' ' || c == '\n') {
                    for (int i = 0; i < MAX_PLAYER_LASERS; i++) {
                        if (!p_lasers[i].active) {
                            p_lasers[i].active = 1;
                            p_lasers[i].x = px + 4;
                            p_lasers[i].y = py;
                            audio_play_tone(1400, 15);
                            break;
                        }
                    }
                }
                // Smart Bomb
                else if ((c == 'b' || c == 'B') && bombs > 0) {
                    bombs--;
                    audio_play_tone(200, 100);
                    audio_play_tone(150, 150);
                    // Clear all regular enemies
                    for (int i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].active && enemies[i].type != 4) {
                            enemies[i].active = 0;
                            score += 150;
                            enemies_defeated++;
                        }
                    }
                    for (int i = 0; i < MAX_ENEMY_LASERS; i++) e_lasers[i].active = 0;
                }
            }
        }

        if (!running) break;

        // --- Game Logic Update ---
        if (!game_over && !victory) {
            // 1. Move Player Lasers
            for (int i = 0; i < MAX_PLAYER_LASERS; i++) {
                if (p_lasers[i].active) {
                    p_lasers[i].x += 3;
                    if (p_lasers[i].x >= PLAYFIELD_X + PLAYFIELD_W - 2) {
                        p_lasers[i].active = 0;
                    }
                }
            }

            // 2. Move Enemy Lasers
            for (int i = 0; i < MAX_ENEMY_LASERS; i++) {
                if (e_lasers[i].active) {
                    e_lasers[i].x -= 2;
                    if (e_lasers[i].x <= PLAYFIELD_X + 1) {
                        e_lasers[i].active = 0;
                    }
                    // Check collision with Player
                    if (e_lasers[i].active &&
                        e_lasers[i].x >= px && e_lasers[i].x <= px + 3 &&
                        e_lasers[i].y == py) {
                        e_lasers[i].active = 0;
                        hp -= 15;
                        audio_play_tone(280, 50);
                        if (hp <= 0) {
                            lives--;
                            if (lives > 0) {
                                hp = 100;
                            } else {
                                game_over = 1;
                                audio_play_tone(180, 250);
                            }
                        }
                    }
                }
            }

            // 3. Spawn Enemies
            if (wave < 3 && (si_rand() % 10) < 3) {
                int free_idx = -1;
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (!enemies[i].active) { free_idx = i; break; }
                }
                if (free_idx >= 0) {
                    enemies[free_idx].active = 1;
                    enemies[free_idx].x = PLAYFIELD_X + PLAYFIELD_W - 5;
                    enemies[free_idx].y = PLAYFIELD_Y + 2 + (si_rand() % (PLAYFIELD_H - 4));
                    int t = si_rand() % 4;
                    enemies[free_idx].type = t;
                    enemies[free_idx].hp = (t == 3) ? 4 : (t == 2) ? 3 : (t == 1) ? 2 : 1;
                    enemies[free_idx].max_hp = enemies[free_idx].hp;
                    enemies[free_idx].fire_cooldown = 10 + (si_rand() % 20);
                }
            } else if (wave >= 3 && !boss_spawned) {
                // Spawn Titan Boss
                int free_idx = 0;
                enemies[free_idx].active = 1;
                enemies[free_idx].x = PLAYFIELD_X + PLAYFIELD_W - 16;
                enemies[free_idx].y = PLAYFIELD_Y + PLAYFIELD_H / 2;
                enemies[free_idx].type = 4; // Boss
                enemies[free_idx].hp = 30;
                enemies[free_idx].max_hp = 30;
                enemies[free_idx].fire_cooldown = 8;
                boss_spawned = 1;
                audio_play_tone(440, 100);
                audio_play_tone(880, 100);
            }

            // 4. Update Enemies
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].active) {
                    // Move
                    if (enemies[i].type == 4) {
                        // Boss moves vertically
                        if (si_rand() % 2 == 0) {
                            if (enemies[i].y > PLAYFIELD_Y + 2) enemies[i].y--;
                        } else {
                            if (enemies[i].y < PLAYFIELD_Y + PLAYFIELD_H - 4) enemies[i].y++;
                        }
                    } else {
                        enemies[i].x -= 1;
                        if (enemies[i].x <= PLAYFIELD_X + 1) {
                            enemies[i].active = 0;
                        }
                    }

                    // Enemy Shoot
                    if (enemies[i].type == 2 || enemies[i].type == 4) {
                        enemies[i].fire_cooldown--;
                        if (enemies[i].fire_cooldown <= 0) {
                            enemies[i].fire_cooldown = (enemies[i].type == 4) ? 6 : 18;
                            for (int el = 0; el < MAX_ENEMY_LASERS; el++) {
                                if (!e_lasers[el].active) {
                                    e_lasers[el].active = 1;
                                    e_lasers[el].x = enemies[i].x - 1;
                                    e_lasers[el].y = enemies[i].y;
                                    break;
                                }
                            }
                        }
                    }

                    // Check Collision with Player Lasers
                    for (int l = 0; l < MAX_PLAYER_LASERS; l++) {
                        if (p_lasers[l].active && enemies[i].active) {
                            int hit = 0;
                            if (enemies[i].type == 4) {
                                // Boss hitbox
                                if (p_lasers[l].x >= enemies[i].x && p_lasers[l].x <= enemies[i].x + 14 &&
                                    p_lasers[l].y >= enemies[i].y - 1 && p_lasers[l].y <= enemies[i].y + 2) {
                                    hit = 1;
                                }
                            } else {
                                if (p_lasers[l].x >= enemies[i].x && p_lasers[l].x <= enemies[i].x + 3 &&
                                    p_lasers[l].y == enemies[i].y) {
                                    hit = 1;
                                }
                            }

                            if (hit) {
                                p_lasers[l].active = 0;
                                enemies[i].hp--;
                                audio_play_tone(1100, 15);

                                if (enemies[i].hp <= 0) {
                                    enemies[i].active = 0;
                                    audio_play_tone(160, 40);

                                    // Spawn particles
                                    for (int pt = 0; pt < MAX_PARTICLES; pt++) {
                                        if (!particles[pt].active) {
                                            particles[pt].active = 1;
                                            particles[pt].x = enemies[i].x + (pt % 3);
                                            particles[pt].y = enemies[i].y + (pt % 2);
                                            particles[pt].life = 4;
                                            particles[pt].ch = (pt % 2 == 0) ? '*' : '#';
                                            particles[pt].color = VGA_COLOR_LIGHT_RED;
                                            break;
                                        }
                                    }

                                    if (enemies[i].type == 4) {
                                        // Boss Defeated!
                                        score += 2500;
                                        victory = 1;
                                        audio_play_tone(523, 80);
                                        audio_play_tone(659, 80);
                                        audio_play_tone(784, 80);
                                        audio_play_tone(1046, 200);
                                    } else {
                                        score += (enemies[i].type == 2) ? 350 : (enemies[i].type == 1) ? 200 : 100;
                                        enemies_defeated++;
                                        if (enemies_defeated >= 12 && wave == 1) { wave = 2; hp = 100; }
                                        else if (enemies_defeated >= 24 && wave == 2) { wave = 3; hp = 100; }
                                    }
                                }
                            }
                        }
                    }

                    // Check Collision with Player Ship
                    if (enemies[i].active &&
                        px + 3 >= enemies[i].x && px <= enemies[i].x + 3 &&
                        py == enemies[i].y) {
                        enemies[i].active = 0;
                        hp -= 25;
                        audio_play_tone(220, 80);
                        if (hp <= 0) {
                            lives--;
                            if (lives > 0) hp = 100;
                            else { game_over = 1; audio_play_tone(180, 250); }
                        }
                    }
                }
            }

            // 5. Update Particles
            for (int i = 0; i < MAX_PARTICLES; i++) {
                if (particles[i].active) {
                    particles[i].life--;
                    if (particles[i].life <= 0) particles[i].active = 0;
                }
            }
        }

        // --- Render Frame ---
        gfx_clear(VGA_COLOR_BLACK);

        // Top Status HUD
        gfx_draw_box(PLAYFIELD_X, 0, PLAYFIELD_W, 3, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK, " SPACE IMPACT v1.0 ");

        char num_buf[16];
        gfx_draw_string(PLAYFIELD_X + 2, 1, "HP:", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        int bar_len = hp / 10;
        if (bar_len < 0) bar_len = 0;
        if (bar_len > 10) bar_len = 10;
        char hp_bar[12] = "          ";
        for (int b = 0; b < bar_len; b++) hp_bar[b] = '=';
        gfx_draw_string(PLAYFIELD_X + 6, 1, hp_bar, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_DARK_GREY);

        gfx_draw_string(PLAYFIELD_X + 18, 1, "LIVES:", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        si_int_to_str(lives, num_buf);
        gfx_draw_string(PLAYFIELD_X + 25, 1, num_buf, VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

        gfx_draw_string(PLAYFIELD_X + 29, 1, "BOMBS:", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        si_int_to_str(bombs, num_buf);
        gfx_draw_string(PLAYFIELD_X + 36, 1, num_buf, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

        gfx_draw_string(PLAYFIELD_X + 40, 1, "SCORE:", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        si_int_to_str(score, num_buf);
        gfx_draw_string(PLAYFIELD_X + 47, 1, num_buf, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

        gfx_draw_string(PLAYFIELD_X + 56, 1, "WAVE:", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        si_int_to_str(wave, num_buf);
        gfx_draw_string(PLAYFIELD_X + 62, 1, num_buf, (wave == 3) ? VGA_COLOR_LIGHT_RED : VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        // Draw Playfield Outer Frame
        gfx_draw_double_box(PLAYFIELD_X, PLAYFIELD_Y, PLAYFIELD_W, PLAYFIELD_H, VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK, 0);

        // Draw Player Ship: Sprite `}=►`
        if (!game_over) {
            gfx_draw_string(px, py, "}=>", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        }

        // Draw Player Lasers
        for (int i = 0; i < MAX_PLAYER_LASERS; i++) {
            if (p_lasers[i].active) {
                gfx_draw_string(p_lasers[i].x, p_lasers[i].y, "-=", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            }
        }

        // Draw Enemy Lasers
        for (int i = 0; i < MAX_ENEMY_LASERS; i++) {
            if (e_lasers[i].active) {
                gfx_put_char(e_lasers[i].x, e_lasers[i].y, '*', VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            }
        }

        // Draw Enemies
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                if (enemies[i].type == 0) {
                    gfx_draw_string(enemies[i].x, enemies[i].y, "<#", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                } else if (enemies[i].type == 1) {
                    gfx_draw_string(enemies[i].x, enemies[i].y, "<<=", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                } else if (enemies[i].type == 2) {
                    gfx_draw_string(enemies[i].x, enemies[i].y, "{<X}", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
                } else if (enemies[i].type == 3) {
                    gfx_draw_string(enemies[i].x, enemies[i].y, "(O)", VGA_COLOR_BROWN, VGA_COLOR_BLACK);
                } else if (enemies[i].type == 4) {
                    // Boss Sprite
                    gfx_draw_string(enemies[i].x, enemies[i].y - 1, "[<< TITAN >>]", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                    gfx_draw_string(enemies[i].x, enemies[i].y,     "<= OMEGA-X =>", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                    gfx_draw_string(enemies[i].x, enemies[i].y + 1, "[<< DREAD >>]", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                }
            }
        }

        // Draw Explosion Particles
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].active) {
                gfx_put_char(particles[i].x, particles[i].y, particles[i].ch, particles[i].color, VGA_COLOR_BLACK);
            }
        }

        // Draw Overlays (Game Over / Victory)
        if (game_over) {
            int gbx = PLAYFIELD_X + 22;
            int gby = PLAYFIELD_Y + 6;
            gfx_draw_double_box(gbx, gby, 34, 7, VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK, " GAME OVER ");
            gfx_draw_string(gbx + 4, gby + 2, "Your ship was destroyed!", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            gfx_draw_string(gbx + 4, gby + 3, "Final Score: ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            si_int_to_str(score, num_buf);
            gfx_draw_string(gbx + 17, gby + 3, num_buf, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            gfx_draw_string(gbx + 4, gby + 5, "[SPACE] Play Again | [Q] Exit", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        } else if (victory) {
            int gbx = PLAYFIELD_X + 20;
            int gby = PLAYFIELD_Y + 6;
            gfx_draw_double_box(gbx, gby, 38, 7, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK, " MISSION COMPLETE! ");
            gfx_draw_string(gbx + 4, gby + 2, "Alien Dreadnought Obliterated!", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            gfx_draw_string(gbx + 4, gby + 3, "Hero of Eran Galaxy! Score: ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            si_int_to_str(score, num_buf);
            gfx_draw_string(gbx + 32, gby + 3, num_buf, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            gfx_draw_string(gbx + 4, gby + 5, "[SPACE] Play Again | [Q] Exit", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        }

        gfx_draw_string(PLAYFIELD_X + 2, PLAYFIELD_Y + PLAYFIELD_H - 1,
                        "[W/S/A/D: Move | SPACE: Laser | B: Bomb | Q: Exit]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

        gfx_flip_to_screen();
        sleep_ms(65);
    }

    // Screen reset
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("[SPACE IMPACT] Exited back to Shell. Final Score: %d\n", score);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}
