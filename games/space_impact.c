#include "space_impact.h"
#include "../drivers/gfx.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/serial.h"
#include "../drivers/timer.h"
#include "../drivers/audio.h"

// ---------------------------------------------------------------------------
// Screen Geometry & Playfield Bounds
// ---------------------------------------------------------------------------
#define SCREEN_W        80
#define SCREEN_H        25

#define PLAYFIELD_X     1
#define PLAYFIELD_Y     3
#define PLAYFIELD_W     78
#define PLAYFIELD_H     19

#define MAX_STARS       32
#define MAX_PLAYER_SHOTS 10
#define MAX_ROCKETS     4
#define MAX_ENEMY_SHOTS 10
#define MAX_ENEMIES     8
#define MAX_PARTICLES   24

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------
typedef struct {
    int x;
    int y;
    int speed; // 1 = slow, 2 = medium, 3 = fast
    char ch;
} star_t;

typedef struct {
    int active;
    int x;
    int y;
    int is_rocket; // 0 = standard blaster, 1 = heavy rocket
} shot_t;

typedef struct {
    int active;
    int x;
    int y;
} enemy_shot_t;

typedef struct {
    int active;
    int x;
    int y;
    int base_y;       // for sine-wave oscillation
    int sine_phase;   // phase step
    int type;         // 0=Manta, 1=Beetle, 2=Cruiser, 3=Asteroid, 4=Boss
    int hp;
    int max_hp;
    int fire_timer;
} enemy_t;

typedef struct {
    int active;
    int x;
    int y;
    int life;
    char ch;
    uint8_t fg;
} particle_t;

// Persistent session high score
static int space_impact_high_score = 5000;

// Random number generator
static uint32_t si_seed = 987654321;
static uint32_t si_rand(void) {
    si_seed ^= si_seed << 13;
    si_seed ^= si_seed >> 17;
    si_seed ^= si_seed << 5;
    return si_seed;
}

static void si_int_to_str(int val, char *buf, int min_digits) {
    char temp[12];
    int i = 0;
    if (val == 0) {
        temp[i++] = '0';
    } else {
        while (val > 0) {
            temp[i++] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    while (i < min_digits && i < 10) {
        temp[i++] = '0';
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

// ---------------------------------------------------------------------------
// Play Retro Nokia Monophonic Fanfare
// ---------------------------------------------------------------------------
static void play_nokia_intro(void) {
    audio_play_tone(659, 90);  // E5
    audio_play_tone(587, 90);  // D5
    audio_play_tone(370, 110); // F#4
    audio_play_tone(415, 110); // G#4
    audio_play_tone(554, 90);  // C#5
    audio_play_tone(494, 90);  // B4
    audio_play_tone(294, 110); // D4
    audio_play_tone(330, 150); // E4
}

// ---------------------------------------------------------------------------
// Main Game Engine
// ---------------------------------------------------------------------------
void space_impact_run(void) {
    gfx_init();
    si_seed = timer_get_ticks() + 876543;

    // Palette mode: 0 = Authentic Nokia 3310 Green LCD, 1 = Arcade Color VGA
    int palette_mode = 0;

    // Player State
    int px = 4;
    int py = 12;
    int hp = 100;
    int lives = 3;
    int bombs = 2;
    int rockets = 4;
    int score = 0;
    int level = 1;
    int enemies_killed = 0;
    int thruster_frame = 0;

    shot_t p_shots[MAX_PLAYER_SHOTS];
    for (int i = 0; i < MAX_PLAYER_SHOTS; i++) p_shots[i].active = 0;

    shot_t p_rockets[MAX_ROCKETS];
    for (int i = 0; i < MAX_ROCKETS; i++) p_rockets[i].active = 0;

    enemy_shot_t e_shots[MAX_ENEMY_SHOTS];
    for (int i = 0; i < MAX_ENEMY_SHOTS; i++) e_shots[i].active = 0;

    enemy_t enemies[MAX_ENEMIES];
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;

    particle_t particles[MAX_PARTICLES];
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = 0;

    // Parallax Starfield initialization
    star_t stars[MAX_STARS];
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = PLAYFIELD_X + (si_rand() % (PLAYFIELD_W - 2));
        stars[i].y = PLAYFIELD_Y + 1 + (si_rand() % (PLAYFIELD_H - 2));
        int sp = (i % 3) + 1;
        stars[i].speed = sp;
        stars[i].ch = (sp == 3) ? '*' : (sp == 2) ? '+' : '.';
    }

    int running = 1;
    int game_over = 0;
    int victory = 0;
    int boss_spawned = 0;
    int frame_count = 0;

    play_nokia_intro();

    while (running) {
        frame_count++;
        thruster_frame = !thruster_frame;

        // --- 1. Starfield Scrolling ---
        for (int i = 0; i < MAX_STARS; i++) {
            if (stars[i].speed == 3 || (stars[i].speed == 2 && (frame_count % 2 == 0)) || (stars[i].speed == 1 && (frame_count % 3 == 0))) {
                stars[i].x--;
                if (stars[i].x <= PLAYFIELD_X) {
                    stars[i].x = PLAYFIELD_X + PLAYFIELD_W - 2;
                    stars[i].y = PLAYFIELD_Y + 1 + (si_rand() % (PLAYFIELD_H - 2));
                }
            }
        }

        // --- 2. Handle Inputs ---
        while (keyboard_has_char() || serial_has_rx()) {
            char c = 0;
            if (keyboard_has_char()) c = keyboard_getc();
            else if (serial_has_rx()) c = serial_getc();

            if (c == 'q' || c == 'Q' || c == 27) { // Q or ESC quits
                running = 0;
                break;
            } else if (c == 'c' || c == 'C') { // Toggle Nokia LCD / Arcade VGA Palette
                palette_mode = !palette_mode;
                audio_play_tone(900, 30);
            } else if (game_over || victory) {
                if (c == ' ' || c == '\n' || c == 'r' || c == 'R' || c == '5') {
                    // Reset game
                    px = 4;
                    py = 12;
                    hp = 100;
                    lives = 3;
                    bombs = 2;
                    rockets = 4;
                    score = 0;
                    level = 1;
                    enemies_killed = 0;
                    boss_spawned = 0;
                    game_over = 0;
                    victory = 0;
                    for (int i = 0; i < MAX_PLAYER_SHOTS; i++) p_shots[i].active = 0;
                    for (int i = 0; i < MAX_ROCKETS; i++) p_rockets[i].active = 0;
                    for (int i = 0; i < MAX_ENEMY_SHOTS; i++) e_shots[i].active = 0;
                    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;
                    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = 0;
                    play_nokia_intro();
                }
            } else {
                // Movement: Supports both Nokia 3310 Keypad (2, 8, 4, 6) and PC keys (W, S, A, D / Arrow equivalents)
                if ((c == '2' || c == 'w' || c == 'W' || c == 'i') && py > PLAYFIELD_Y + 1) {
                    py--;
                } else if ((c == '8' || c == 's' || c == 'S' || c == 'k') && py < PLAYFIELD_Y + PLAYFIELD_H - 2) {
                    py++;
                } else if ((c == '4' || c == 'a' || c == 'A' || c == 'j') && px > PLAYFIELD_X + 2) {
                    px -= 2;
                } else if ((c == '6' || c == 'd' || c == 'D' || c == 'l') && px < PLAYFIELD_X + 28) {
                    px += 2;
                }
                // Primary Laser Blaster: Key 5, SPACE, or ENTER
                else if (c == '5' || c == ' ' || c == '\n') {
                    for (int i = 0; i < MAX_PLAYER_SHOTS; i++) {
                        if (!p_shots[i].active) {
                            p_shots[i].active = 1;
                            p_shots[i].x = px + 6;
                            p_shots[i].y = py;
                            p_shots[i].is_rocket = 0;
                            audio_play_tone(1500, 15);
                            break;
                        }
                    }
                }
                // Secondary Weapon: Heavy Piercing Rocket (Key 1, R, or M)
                else if ((c == '1' || c == 'r' || c == 'R' || c == 'm' || c == 'M') && rockets > 0) {
                    for (int i = 0; i < MAX_ROCKETS; i++) {
                        if (!p_rockets[i].active) {
                            p_rockets[i].active = 1;
                            p_rockets[i].x = px + 6;
                            p_rockets[i].y = py;
                            p_rockets[i].is_rocket = 1;
                            rockets--;
                            audio_play_tone(700, 35);
                            break;
                        }
                    }
                }
                // Smart Bomb: Full Screen Shockwave (Key 7, B, or 0)
                else if ((c == '7' || c == 'b' || c == 'B' || c == '0') && bombs > 0) {
                    bombs--;
                    audio_play_tone(140, 120);
                    audio_play_tone(90, 160);
                    // Clear all normal enemies & enemy bullets
                    for (int i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].active) {
                            if (enemies[i].type == 4) {
                                // Boss takes heavy damage
                                enemies[i].hp -= 12;
                            } else {
                                enemies[i].active = 0;
                                score += 200;
                                enemies_killed++;
                            }
                        }
                    }
                    for (int i = 0; i < MAX_ENEMY_SHOTS; i++) e_shots[i].active = 0;

                    // Spawn screen shockwave particles
                    for (int pt = 0; pt < MAX_PARTICLES; pt++) {
                        particles[pt].active = 1;
                        particles[pt].x = PLAYFIELD_X + 4 + (si_rand() % (PLAYFIELD_W - 8));
                        particles[pt].y = PLAYFIELD_Y + 1 + (si_rand() % (PLAYFIELD_H - 2));
                        particles[pt].life = 6;
                        particles[pt].ch = '#';
                        particles[pt].fg = (palette_mode == 0) ? VGA_COLOR_BLACK : VGA_COLOR_YELLOW;
                    }
                }
            }
        }

        if (!running) break;

        // --- 3. Game Logic Update ---
        if (!game_over && !victory) {
            // Update Player Blaster Lasers
            for (int i = 0; i < MAX_PLAYER_SHOTS; i++) {
                if (p_shots[i].active) {
                    p_shots[i].x += 3;
                    if (p_shots[i].x >= PLAYFIELD_X + PLAYFIELD_W - 2) {
                        p_shots[i].active = 0;
                    }
                }
            }

            // Update Heavy Rockets
            for (int i = 0; i < MAX_ROCKETS; i++) {
                if (p_rockets[i].active) {
                    p_rockets[i].x += 2;
                    if (p_rockets[i].x >= PLAYFIELD_X + PLAYFIELD_W - 2) {
                        p_rockets[i].active = 0;
                    }
                }
            }

            // Update Enemy Shots
            for (int i = 0; i < MAX_ENEMY_SHOTS; i++) {
                if (e_shots[i].active) {
                    e_shots[i].x -= 2;
                    if (e_shots[i].x <= PLAYFIELD_X + 1) {
                        e_shots[i].active = 0;
                    }
                    // Collision with Player Ship
                    if (e_shots[i].active &&
                        e_shots[i].x >= px && e_shots[i].x <= px + 5 &&
                        e_shots[i].y == py) {
                        e_shots[i].active = 0;
                        hp -= 15;
                        audio_play_tone(240, 50);
                        if (hp <= 0) {
                            lives--;
                            if (lives > 0) {
                                hp = 100;
                            } else {
                                game_over = 1;
                                audio_play_tone(150, 300);
                            }
                        }
                    }
                }
            }

            // Spawn Enemies
            if (level < 3 && (si_rand() % 10) < 3) {
                int free_idx = -1;
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (!enemies[i].active) { free_idx = i; break; }
                }
                if (free_idx >= 0) {
                    enemies[free_idx].active = 1;
                    enemies[free_idx].x = PLAYFIELD_X + PLAYFIELD_W - 6;
                    enemies[free_idx].base_y = PLAYFIELD_Y + 2 + (si_rand() % (PLAYFIELD_H - 4));
                    enemies[free_idx].y = enemies[free_idx].base_y;
                    enemies[free_idx].sine_phase = si_rand() % 8;

                    int t = (level == 1) ? (si_rand() % 3) : (si_rand() % 4);
                    enemies[free_idx].type = t;
                    enemies[free_idx].hp = (t == 3) ? 5 : (t == 2) ? 4 : (t == 1) ? 2 : 1;
                    enemies[free_idx].max_hp = enemies[free_idx].hp;
                    enemies[free_idx].fire_timer = 12 + (si_rand() % 18);
                }
            } else if (level >= 3 && !boss_spawned) {
                // Spawn Original Alien Megabeast Boss!
                int free_idx = 0;
                enemies[free_idx].active = 1;
                enemies[free_idx].x = PLAYFIELD_X + PLAYFIELD_W - 20;
                enemies[free_idx].base_y = PLAYFIELD_Y + PLAYFIELD_H / 2;
                enemies[free_idx].y = enemies[free_idx].base_y;
                enemies[free_idx].sine_phase = 0;
                enemies[free_idx].type = 4; // Boss
                enemies[free_idx].hp = 40;
                enemies[free_idx].max_hp = 40;
                enemies[free_idx].fire_timer = 8;
                boss_spawned = 1;
                audio_play_tone(350, 150);
                audio_play_tone(700, 150);
            }

            // Update Enemies
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].active) {
                    // Movement
                    if (enemies[i].type == 0) {
                        // Manta: Sine wave undulating flight
                        enemies[i].x -= 1;
                        enemies[i].sine_phase = (enemies[i].sine_phase + 1) % 12;
                        int offset = (enemies[i].sine_phase < 6) ? (enemies[i].sine_phase - 2) : (10 - enemies[i].sine_phase - 2);
                        enemies[i].y = enemies[i].base_y + offset;
                        if (enemies[i].y < PLAYFIELD_Y + 1) enemies[i].y = PLAYFIELD_Y + 1;
                        if (enemies[i].y > PLAYFIELD_Y + PLAYFIELD_H - 2) enemies[i].y = PLAYFIELD_Y + PLAYFIELD_H - 2;
                    } else if (enemies[i].type == 4) {
                        // Boss: Patrols vertically and tracks slightly
                        if (frame_count % 3 == 0) {
                            if (enemies[i].y < py && enemies[i].y < PLAYFIELD_Y + PLAYFIELD_H - 4) {
                                enemies[i].y++;
                            } else if (enemies[i].y > py && enemies[i].y > PLAYFIELD_Y + 2) {
                                enemies[i].y--;
                            }
                        }
                    } else {
                        // Beetle, Cruiser, Asteroid move steadily left
                        enemies[i].x -= 1;
                    }

                    if (enemies[i].x <= PLAYFIELD_X + 1) {
                        enemies[i].active = 0;
                    }

                    // Enemy Shooting (Beetles, Cruisers & Boss)
                    if (enemies[i].type == 1 || enemies[i].type == 2 || enemies[i].type == 4) {
                        enemies[i].fire_timer--;
                        if (enemies[i].fire_timer <= 0) {
                            enemies[i].fire_timer = (enemies[i].type == 4) ? 7 : (enemies[i].type == 2) ? 16 : 24;
                            for (int el = 0; el < MAX_ENEMY_SHOTS; el++) {
                                if (!e_shots[el].active) {
                                    e_shots[el].active = 1;
                                    e_shots[el].x = enemies[i].x - 1;
                                    e_shots[el].y = enemies[i].y;
                                    break;
                                }
                            }
                        }
                    }

                    // Collision with Player Shots (Blasters)
                    for (int l = 0; l < MAX_PLAYER_SHOTS; l++) {
                        if (p_shots[l].active && enemies[i].active) {
                            int hit = 0;
                            if (enemies[i].type == 4) {
                                // Boss Hitbox (18 cols x 3 rows)
                                if (p_shots[l].x >= enemies[i].x && p_shots[l].x <= enemies[i].x + 18 &&
                                    p_shots[l].y >= enemies[i].y - 1 && p_shots[l].y <= enemies[i].y + 1) {
                                    hit = 1;
                                }
                            } else {
                                if (p_shots[l].x >= enemies[i].x && p_shots[l].x <= enemies[i].x + 3 &&
                                    p_shots[l].y == enemies[i].y) {
                                    hit = 1;
                                }
                            }

                            if (hit) {
                                p_shots[l].active = 0;
                                enemies[i].hp--;
                                audio_play_tone(1100, 15);

                                if (enemies[i].hp <= 0) {
                                    enemies[i].active = 0;
                                    audio_play_tone(200, 40);

                                    // Spawn particles
                                    for (int pt = 0; pt < 4; pt++) {
                                        for (int p_idx = 0; p_idx < MAX_PARTICLES; p_idx++) {
                                            if (!particles[p_idx].active) {
                                                particles[p_idx].active = 1;
                                                particles[p_idx].x = enemies[i].x + (pt % 3);
                                                particles[p_idx].y = enemies[i].y + (pt % 2);
                                                particles[p_idx].life = 4;
                                                particles[p_idx].ch = (pt % 2 == 0) ? '*' : '#';
                                                particles[p_idx].fg = (palette_mode == 0) ? VGA_COLOR_BLACK : VGA_COLOR_LIGHT_RED;
                                                break;
                                            }
                                        }
                                    }

                                    if (enemies[i].type == 4) {
                                        // Boss Defeated!
                                        score += 5000;
                                        victory = 1;
                                        audio_play_tone(523, 100);
                                        audio_play_tone(659, 100);
                                        audio_play_tone(784, 100);
                                        audio_play_tone(1046, 250);
                                    } else {
                                        score += (enemies[i].type == 3) ? 300 : (enemies[i].type == 2) ? 400 : (enemies[i].type == 1) ? 200 : 100;
                                        enemies_killed++;
                                        // Bonus pickups occasionally
                                        if (enemies_killed % 7 == 0 && rockets < 6) rockets++;
                                        if (enemies_killed % 15 == 0 && bombs < 4) bombs++;

                                        if (enemies_killed >= 12 && level == 1) { level = 2; hp = 100; }
                                        else if (enemies_killed >= 24 && level == 2) { level = 3; hp = 100; }
                                    }
                                }
                            }
                        }
                    }

                    // Collision with Player Rockets (Heavy Torpedoes)
                    for (int r = 0; r < MAX_ROCKETS; r++) {
                        if (p_rockets[r].active && enemies[i].active) {
                            int hit = 0;
                            if (enemies[i].type == 4) {
                                if (p_rockets[r].x >= enemies[i].x && p_rockets[r].x <= enemies[i].x + 18 &&
                                    p_rockets[r].y >= enemies[i].y - 1 && p_rockets[r].y <= enemies[i].y + 1) {
                                    hit = 1;
                                }
                            } else {
                                if (p_rockets[r].x >= enemies[i].x && p_rockets[r].x <= enemies[i].x + 3 &&
                                    p_rockets[r].y == enemies[i].y) {
                                    hit = 1;
                                }
                            }

                            if (hit) {
                                p_rockets[r].active = 0;
                                enemies[i].hp -= 4; // Rockets deal 4x damage!
                                audio_play_tone(400, 30);
                                if (enemies[i].hp <= 0) {
                                    enemies[i].active = 0;
                                    score += 250;
                                    enemies_killed++;
                                }
                            }
                        }
                    }

                    // Collision with Player Ship Hull
                    if (enemies[i].active &&
                        px + 5 >= enemies[i].x && px <= enemies[i].x + 3 &&
                        py == enemies[i].y) {
                        enemies[i].active = 0;
                        hp -= 25;
                        audio_play_tone(190, 80);
                        if (hp <= 0) {
                            lives--;
                            if (lives > 0) hp = 100;
                            else { game_over = 1; audio_play_tone(150, 300); }
                        }
                    }
                }
            }

            // Update Particles
            for (int i = 0; i < MAX_PARTICLES; i++) {
                if (particles[i].active) {
                    particles[i].life--;
                    if (particles[i].life <= 0) particles[i].active = 0;
                }
            }

            if (score > space_impact_high_score) {
                space_impact_high_score = score;
            }
        }

        // --- 4. Render Frame ---
        uint8_t bg = (palette_mode == 0) ? VGA_COLOR_LIGHT_GREEN : VGA_COLOR_BLACK;
        uint8_t fg_text = (palette_mode == 0) ? VGA_COLOR_BLACK : VGA_COLOR_WHITE;
        uint8_t fg_ship = (palette_mode == 0) ? VGA_COLOR_BLACK : VGA_COLOR_LIGHT_CYAN;
        uint8_t fg_laser = (palette_mode == 0) ? VGA_COLOR_BLACK : VGA_COLOR_YELLOW;
        uint8_t fg_rocket = (palette_mode == 0) ? VGA_COLOR_BLACK : VGA_COLOR_LIGHT_MAGENTA;
        uint8_t fg_enemy = (palette_mode == 0) ? VGA_COLOR_BLACK : VGA_COLOR_LIGHT_RED;
        uint8_t fg_star = (palette_mode == 0) ? VGA_COLOR_GREEN : VGA_COLOR_DARK_GREY;

        gfx_clear(bg);

        // Header: Authentic Nokia 3310 Status Banner
        char buf[16];
        gfx_draw_string(PLAYFIELD_X + 1, 0, "|||| [NOKIA 3310] ", fg_text, bg);
        gfx_draw_string(PLAYFIELD_X + 22, 0, "* SPACE IMPACT *", fg_text, bg);
        gfx_draw_string(PLAYFIELD_X + 58, 0, "BATT [====]", fg_text, bg);

        // Score & Status Line
        gfx_draw_string(PLAYFIELD_X + 1, 1, "SCORE:", fg_text, bg);
        si_int_to_str(score, buf, 6);
        gfx_draw_string(PLAYFIELD_X + 8, 1, buf, fg_text, bg);

        gfx_draw_string(PLAYFIELD_X + 16, 1, "HIGH:", fg_text, bg);
        si_int_to_str(space_impact_high_score, buf, 6);
        gfx_draw_string(PLAYFIELD_X + 22, 1, buf, fg_text, bg);

        gfx_draw_string(PLAYFIELD_X + 30, 1, "HP:", fg_text, bg);
        int bar_len = hp / 10;
        char hp_bar[12] = "          ";
        for (int b = 0; b < bar_len && b < 10; b++) hp_bar[b] = '=';
        gfx_draw_string(PLAYFIELD_X + 34, 1, hp_bar, fg_text, bg);

        gfx_draw_string(PLAYFIELD_X + 46, 1, "SHIPS:", fg_text, bg);
        for (int l = 0; l < lives; l++) {
            gfx_draw_string(PLAYFIELD_X + 53 + (l * 3), 1, "=>", fg_text, bg);
        }

        gfx_draw_string(PLAYFIELD_X + 64, 1, "BOMB:", fg_text, bg);
        si_int_to_str(bombs, buf, 1);
        gfx_draw_string(PLAYFIELD_X + 70, 1, buf, fg_text, bg);

        gfx_draw_string(PLAYFIELD_X + 73, 1, "RCK:", fg_text, bg);
        si_int_to_str(rockets, buf, 1);
        gfx_draw_string(PLAYFIELD_X + 77, 1, buf, fg_text, bg);

        // Border of LCD Screen
        gfx_draw_double_box(PLAYFIELD_X, PLAYFIELD_Y - 1, PLAYFIELD_W, PLAYFIELD_H + 2, fg_text, bg, 0);

        // Render Scrolling Stars
        for (int i = 0; i < MAX_STARS; i++) {
            gfx_put_char(stars[i].x, stars[i].y, stars[i].ch, fg_star, bg);
        }

        // Render Player Ship (Authentic Nokia Delta Fighter with Flickering Engine Flame)
        if (!game_over) {
            char thruster = thruster_frame ? '~' : '-';
            gfx_put_char(px, py, thruster, fg_laser, bg);
            gfx_draw_string(px + 1, py, "}>==>", fg_ship, bg);
        }

        // Render Player Blaster Lasers
        for (int i = 0; i < MAX_PLAYER_SHOTS; i++) {
            if (p_shots[i].active) {
                gfx_draw_string(p_shots[i].x, p_shots[i].y, "= =", fg_laser, bg);
            }
        }

        // Render Player Rockets
        for (int i = 0; i < MAX_ROCKETS; i++) {
            if (p_rockets[i].active) {
                gfx_draw_string(p_rockets[i].x, p_rockets[i].y, "==>>", fg_rocket, bg);
            }
        }

        // Render Enemy Plasma Shots
        for (int i = 0; i < MAX_ENEMY_SHOTS; i++) {
            if (e_shots[i].active) {
                gfx_put_char(e_shots[i].x, e_shots[i].y, '*', fg_enemy, bg);
            }
        }

        // Render Enemies
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                if (enemies[i].type == 0) {
                    // Manta Flier
                    gfx_draw_string(enemies[i].x, enemies[i].y, "<#>", fg_enemy, bg);
                } else if (enemies[i].type == 1) {
                    // Beetle Drone
                    gfx_draw_string(enemies[i].x, enemies[i].y, "<<O", fg_enemy, bg);
                } else if (enemies[i].type == 2) {
                    // Armored Cruiser
                    gfx_draw_string(enemies[i].x, enemies[i].y, "[<<=X]", fg_enemy, bg);
                } else if (enemies[i].type == 3) {
                    // Spiked Asteroid / Mine
                    gfx_draw_string(enemies[i].x, enemies[i].y, "(*)", fg_enemy, bg);
                } else if (enemies[i].type == 4) {
                    // Giant Alien Megabeast / Titan Dreadnought
                    gfx_draw_string(enemies[i].x, enemies[i].y - 1, "/<<---[TITAN]--->>\\", fg_enemy, bg);
                    gfx_draw_string(enemies[i].x, enemies[i].y,     "{<= (X) CORE (X) =>}", fg_laser, bg);
                    gfx_draw_string(enemies[i].x, enemies[i].y + 1, "\\<<---[TITAN]--->>/", fg_enemy, bg);

                    // Boss HP bar right above the battlefield
                    gfx_draw_string(PLAYFIELD_X + 20, PLAYFIELD_Y, "[BOSS HP: ", fg_text, bg);
                    int b_bar = enemies[i].hp / 2;
                    char b_hp_str[24] = "                    ";
                    for (int b = 0; b < b_bar && b < 20; b++) b_hp_str[b] = '=';
                    gfx_draw_string(PLAYFIELD_X + 30, PLAYFIELD_Y, b_hp_str, fg_enemy, bg);
                    gfx_draw_string(PLAYFIELD_X + 51, PLAYFIELD_Y, "]", fg_text, bg);
                }
            }
        }

        // Render Explosion Particles
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].active) {
                gfx_put_char(particles[i].x, particles[i].y, particles[i].ch, particles[i].fg, bg);
            }
        }

        // Overlays: Game Over / Victory
        if (game_over) {
            int gbx = PLAYFIELD_X + 18;
            int gby = PLAYFIELD_Y + 5;
            gfx_draw_double_box(gbx, gby, 42, 8, fg_text, bg, " NOKIA GAME OVER ");
            gfx_draw_string(gbx + 4, gby + 2, "Your ship was destroyed in deep space!", fg_text, bg);
            gfx_draw_string(gbx + 4, gby + 3, "Final Score: ", fg_text, bg);
            si_int_to_str(score, buf, 6);
            gfx_draw_string(gbx + 17, gby + 3, buf, fg_text, bg);
            gfx_draw_string(gbx + 4, gby + 5, "[5/SPACE] Play Again | [Q] Exit to Shell", fg_text, bg);
        } else if (victory) {
            int gbx = PLAYFIELD_X + 16;
            int gby = PLAYFIELD_Y + 5;
            gfx_draw_double_box(gbx, gby, 46, 8, fg_text, bg, " MISSION COMPLETE! ");
            gfx_draw_string(gbx + 4, gby + 2, "Alien Megabeast Obliterated! Earth is Safe!", fg_text, bg);
            gfx_draw_string(gbx + 4, gby + 3, "Galaxy Legend! Final Score: ", fg_text, bg);
            si_int_to_str(score, buf, 6);
            gfx_draw_string(gbx + 32, gby + 3, buf, fg_text, bg);
            gfx_draw_string(gbx + 4, gby + 5, "[5/SPACE] Play Again | [Q] Exit to Shell", fg_text, bg);
        }

        // Bottom Authentic Controls Bar
        gfx_draw_string(PLAYFIELD_X + 1, PLAYFIELD_Y + PLAYFIELD_H + 1,
                        "[2/8/4/6 or W/A/S/D: Move | 5/SPC: Laser | 1/R: Rocket | 7/B: Bomb | C: Color | Q: Exit]",
                        fg_text, bg);

        gfx_flip_to_screen();
        sleep_ms(60);
    }

    // Clean exit
    vga_init();
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("\n[NOKIA SPACE IMPACT] Game Exited. Final Score: %d | High Score: %d\n", score, space_impact_high_score);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}
