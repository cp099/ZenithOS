#include "libc.h"

#define VIEW_MIN_X 30
#define VIEW_MAX_X 90
#define VIEW_MIN_Y 2
#define VIEW_MAX_Y 42

#define MAX_LASERS 6
#define MAX_BOMBS 8
#define ALIEN_ROWS 3
#define ALIEN_COLS 6

static unsigned int next_random = 1337;

int rand() {
    next_random = next_random * 1103515245 + 12345;
    return (unsigned int)(next_random / 65536) % 32768;
}

void srand(unsigned int seed) {
    next_random = seed;
}

struct Laser {
    int active;
    int x, y;
    int prev_x, prev_y;
};

struct Bomb {
    int active;
    int x, y;
    int prev_x, prev_y;
};

struct Alien {
    int active;
    int x, y;
    int prev_x, prev_y;
    int home_x, home_y;
    int type;
    int diving;
    int dive_step;
};

struct Player {
    int x;
    int prev_x;
    int lives;
    int score;
    int wave;
};

static struct Player player;
static struct Laser lasers[MAX_LASERS];
static struct Bomb bombs[MAX_BOMBS];
static struct Alien aliens[ALIEN_ROWS][ALIEN_COLS];

static int formation_offset = 0;
static int formation_dir = 1;
static int formation_timer = 0;

void erase_player() {
    set_cursor(player.prev_x, VIEW_MAX_Y - 1);
    puts("   ");
    set_cursor(player.prev_x, VIEW_MAX_Y);
    puts("   ");
}

void draw_player() {
    set_cursor(player.x, VIEW_MAX_Y - 1);
    puts(" /\\");
    set_cursor(player.x, VIEW_MAX_Y);
    puts("|oo|");
}

void spawn_swarm() {
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            aliens[r][c].active = 1;
            aliens[r][c].type = r;
            aliens[r][c].diving = 0;
            aliens[r][c].dive_step = 0;
            aliens[r][c].home_y = VIEW_MIN_Y + 3 + r * 3;
            aliens[r][c].home_x = VIEW_MIN_X + 10 + c * 7;
            aliens[r][c].x = aliens[r][c].home_x;
            aliens[r][c].y = aliens[r][c].home_y;
            aliens[r][c].prev_x = aliens[r][c].x;
            aliens[r][c].prev_y = aliens[r][c].y;
        }
    }
}

int count_active_aliens() {
    int count = 0;
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            if (aliens[r][c].active) count++;
        }
    }
    return count;
}

void update_formation() {
    formation_timer++;
    if (formation_timer >= 12) {
        formation_timer = 0;
        formation_offset += formation_dir;
        if (formation_offset >= 4 || formation_offset <= -4) {
            formation_dir = -formation_dir;
        }
    }
}

void update_aliens() {
    update_formation();

    int dive_chance = 80 - (player.wave * 10);
    if (dive_chance < 15) dive_chance = 15;

    int bomb_chance_dive = 25 - (player.wave * 3);
    if (bomb_chance_dive < 6) bomb_chance_dive = 6;

    int bomb_chance_stand = 500 - (player.wave * 50);
    if (bomb_chance_stand < 100) bomb_chance_stand = 100;

    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            struct Alien* al = &aliens[r][c];
            if (!al->active) continue;

            al->prev_x = al->x;
            al->prev_y = al->y;

            if (al->diving) {
                int move_bomber = 1;
                if (player.wave == 1) {
                    if (al->dive_step % 2 == 1) move_bomber = 0;
                }

                if (move_bomber) {
                    al->dive_step++;
                    al->y++;
                    
                    int target_x = player.x + 1;
                    if (al->dive_step < 35) {
                        int dx = target_x - al->x;
                        if (dx > 0) al->x++;
                        else if (dx < 0) al->x--;
                    }
                } else {
                    al->dive_step++;
                }

                if (al->y >= VIEW_MAX_Y) {
                    al->diving = 0;
                    al->dive_step = 0;
                    al->x = al->home_x + formation_offset;
                    al->y = al->home_y;
                }

                if (rand() % bomb_chance_dive == 0) {
                    for (int i = 0; i < MAX_BOMBS; i++) {
                        if (!bombs[i].active) {
                            bombs[i].active = 1;
                            bombs[i].x = al->x + 1;
                            bombs[i].y = al->y + 1;
                            bombs[i].prev_x = bombs[i].x;
                            bombs[i].prev_y = bombs[i].y;
                            break;
                        }
                    }
                }
            } else {
                al->x = al->home_x + formation_offset;
                al->y = al->home_y;
                
                if (rand() % bomb_chance_stand == 0) {
                    for (int i = 0; i < MAX_BOMBS; i++) {
                        if (!bombs[i].active) {
                            bombs[i].active = 1;
                            bombs[i].x = al->x + 1;
                            bombs[i].y = al->y + 1;
                            bombs[i].prev_x = bombs[i].x;
                            bombs[i].prev_y = bombs[i].y;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (rand() % dive_chance == 0) {
        int attempts = 15;
        while (attempts > 0) {
            int r = rand() % ALIEN_ROWS;
            int c = rand() % ALIEN_COLS;
            if (aliens[r][c].active && !aliens[r][c].diving) {
                aliens[r][c].diving = 1;
                aliens[r][c].dive_step = 0;
                break;
            }
            attempts--;
        }
    }
}

void update_lasers() {
    for (int i = 0; i < MAX_LASERS; i++) {
        if (!lasers[i].active) continue;

        lasers[i].prev_x = lasers[i].x;
        lasers[i].prev_y = lasers[i].y;
        lasers[i].y--;

        if (lasers[i].y < VIEW_MIN_Y) {
            lasers[i].active = 0;
        }
    }
}

void update_bombs() {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) continue;

        bombs[i].prev_x = bombs[i].x;
        bombs[i].prev_y = bombs[i].y;
        bombs[i].y++;

        if (bombs[i].y > VIEW_MAX_Y) {
            bombs[i].active = 0;
        }
    }
}

void check_collisions() {
    for (int l = 0; l < MAX_LASERS; l++) {
        if (!lasers[l].active) continue;

        for (int r = 0; r < ALIEN_ROWS; r++) {
            for (int c = 0; c < ALIEN_COLS; c++) {
                struct Alien* al = &aliens[r][c];
                if (!al->active) continue;

                if (lasers[l].y == al->y && lasers[l].x >= al->x && lasers[l].x <= al->x + 2) {
                    al->active = 0;
                    lasers[l].active = 0;
                    
                    set_cursor(al->x, al->y);
                    puts("   ");
                    
                    if (al->type == 0) player.score += 100;
                    else if (al->type == 1) player.score += 80;
                    else player.score += 50;
                    
                    break;
                }
            }
            if (!lasers[l].active) break;
        }
    }

    int px_start = player.x;
    int px_end = player.x + 3;

    for (int b = 0; b < MAX_BOMBS; b++) {
        if (!bombs[b].active) continue;

        if (bombs[b].y == VIEW_MAX_Y - 1 && bombs[b].x >= px_start && bombs[b].x <= px_end) {
            bombs[b].active = 0;
            player.lives--;
            erase_player();
            player.x = (VIEW_MIN_X + VIEW_MAX_X) / 2;
            player.prev_x = player.x;
            draw_player();
            return;
        }
    }

    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            struct Alien* al = &aliens[r][c];
            if (!al->active || !al->diving) continue;

            if (al->y >= VIEW_MAX_Y - 1 && al->x + 2 >= px_start && al->x <= px_end) {
                al->active = 0;
                set_cursor(al->x, al->y);
                puts("   ");
                player.lives--;
                erase_player();
                player.x = (VIEW_MIN_X + VIEW_MAX_X) / 2;
                player.prev_x = player.x;
                draw_player();
                return;
            }
        }
    }
}

void erase_entities() {
    erase_player();

    for (int i = 0; i < MAX_LASERS; i++) {
        if (lasers[i].active) {
            set_cursor(lasers[i].prev_x, lasers[i].prev_y);
            putchar(' ');
        }
    }

    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) {
            set_cursor(bombs[i].prev_x, bombs[i].prev_y);
            putchar(' ');
        }
    }

    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            struct Alien* al = &aliens[r][c];
            if (al->active) {
                set_cursor(al->prev_x, al->prev_y);
                puts("   ");
            }
        }
    }
}

void draw_entities() {
    for (int i = 0; i < MAX_LASERS; i++) {
        if (lasers[i].active) {
            set_cursor(lasers[i].x, lasers[i].y);
            putchar('|');
        }
    }

    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) {
            set_cursor(bombs[i].x, bombs[i].y);
            putchar('*');
        }
    }

    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            struct Alien* al = &aliens[r][c];
            if (al->active) {
                set_cursor(al->x, al->y);
                if (al->diving) {
                    if (al->type == 0) puts("<M>");
                    else if (al->type == 1) puts("<v>");
                    else puts("<o>");
                } else {
                    if (al->type == 0) puts(" M ");
                    else if (al->type == 1) puts(" v ");
                    else puts(" o ");
                }
            }
        }
    }

    draw_player();
}

void draw_headers() {
    set_cursor(VIEW_MIN_X, 0);
    puts(" SCORE: ");
    puts(itoa(player.score, 10));
    puts("      WAVE: ");
    puts(itoa(player.wave, 10));
    puts("      LIVES: ");
    for (int i = 0; i < player.lives; i++) {
        puts("^ ");
    }
    puts("   ");
}

void draw_boundaries() {
    for (int y = VIEW_MIN_Y; y <= VIEW_MAX_Y; y++) {
        set_cursor(VIEW_MIN_X - 1, y);
        putchar('|');
        set_cursor(VIEW_MAX_X + 3, y);
        putchar('|');
    }
}

void show_title_screen() {
    clear();
    set_cursor(13, 5);
    puts("V   V  OOO  III DDD     BBB  L      A  SSS TTT EEE RRR");
    set_cursor(13, 6);
    puts("V   V O   O  I  D  D    B  B L     A A S    T  E   R  R");
    set_cursor(13, 7);
    puts("V   V O   O  I  D  D    BBB  L     AAA SSS  T  EEE RRR");
    set_cursor(13, 8);
    puts(" V V  O   O  I  D  D    B  B L     A A   S  T  E   R R");
    set_cursor(13, 9);
    puts("  V    OOO  III DDD     BBB  LLLL  A A SSS  T  EEE R  R");

    set_cursor(18, 13);
    puts("=============================================\n");
    set_cursor(18, 15);
    puts("          ZENITH ARCADE SPACE SHOOTER        \n");
    set_cursor(18, 18);
    puts("=============================================\n");

    set_cursor(25, 21);
    puts("Controls:\n");
    set_cursor(25, 23);
    puts("  A      - Slide Ship Left\n");
    set_cursor(25, 24);
    puts("  D      - Slide Ship Right\n");
    set_cursor(25, 25);
    puts("  Space  - Fire Laser\n");
    set_cursor(25, 26);
    puts("  Q      - Quit Game\n\n");

    set_cursor(23, 29);
    puts(":: Press any key to pilot ship ::");

    swap_buffers();
    getchar();
    clear();
}

int main(void) {
    srand(uptime());

    while (1) {
        show_title_screen();
        draw_boundaries();

        player.score = 0;
        player.lives = 3;
        player.wave = 1;
        player.x = (VIEW_MIN_X + VIEW_MAX_X) / 2;
        player.prev_x = player.x;

        for (int i = 0; i < MAX_LASERS; i++) lasers[i].active = 0;
        for (int i = 0; i < MAX_BOMBS; i++) bombs[i].active = 0;

        spawn_swarm();

        int running = 1;

        while (running && player.lives > 0) {
            draw_headers();

            if (count_active_aliens() == 0) {
                set_cursor(VIEW_MIN_X + 15, (VIEW_MIN_Y + VIEW_MAX_Y) / 2);
                puts(":: WAVE CLEARED! ::");
                sleep(150);
                
                set_cursor(VIEW_MIN_X + 10, (VIEW_MIN_Y + VIEW_MAX_Y) / 2);
                puts("                               ");

                player.wave++;
                for (int i = 0; i < MAX_LASERS; i++) lasers[i].active = 0;
                for (int i = 0; i < MAX_BOMBS; i++) bombs[i].active = 0;
                clear();
                draw_boundaries();
                spawn_swarm();
                continue;
            }

            char key = getchar_nonblock();

            player.prev_x = player.x;

            if (key == 'a' || key == 'A') {
                if (player.x > VIEW_MIN_X) player.x -= 2;
            } else if (key == 'd' || key == 'D') {
                if (player.x < VIEW_MAX_X) player.x += 2;
            } else if (key == 'q' || key == 'Q') {
                running = 0;
                break;
            } else if (key == ' ') {
                for (int i = 0; i < MAX_LASERS; i++) {
                    if (!lasers[i].active) {
                        lasers[i].active = 1;
                        lasers[i].x = player.x + 1;
                        lasers[i].y = VIEW_MAX_Y - 2;
                        lasers[i].prev_x = lasers[i].x;
                        lasers[i].prev_y = lasers[i].y;
                        break;
                    }
                }
            }

            erase_entities();
            update_aliens();
            update_lasers();
            update_bombs();
            check_collisions();
            draw_entities();
            swap_buffers();

            int sleep_ticks = 7 - (player.wave - 1);
            if (sleep_ticks < 3) sleep_ticks = 3;
            sleep(sleep_ticks);
        }

        clear();
        set_cursor(18, 12);
        puts("=============================================\n");
        set_cursor(18, 14);
        puts("               G A M E   O V E R             \n");
        set_cursor(18, 16);
        puts("            Final Score: ");
        puts(itoa(player.score, 10));
        puts("\n");
        set_cursor(18, 18);
        puts("=============================================\n");
        set_cursor(18, 20);
        puts("      Press 'P' to Play Again              \n");
        set_cursor(18, 21);
        puts("      Press 'Q' to Quit to Shell           \n");
        set_cursor(18, 23);
        puts("=============================================\n");
        
        swap_buffers();
        sleep(50);
        getchar_nonblock();
        
        int choice = 0;
        while (choice == 0) {
            char key = getchar();
            if (key == 'p' || key == 'P') {
                choice = 1;
            } else if (key == 'q' || key == 'Q') {
                choice = 2;
            }
        }

        if (choice == 2) {
            break;
        }
        clear();
    }

    clear();
    puts("zenith_blaster: closed. Returning to shell...\n");
    exit();
    return 0;
}
