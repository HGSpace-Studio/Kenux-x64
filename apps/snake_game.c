#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vga.h>

#define BOARD_WIDTH 40
#define BOARD_HEIGHT 20
#define MAX_SNAKE_LENGTH 800

typedef struct {
    int x, y;
} Position;

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef struct {
    Position body[MAX_SNAKE_LENGTH];
    int length;
    Direction direction;
    Direction next_direction;
    int score;
    int game_over;
    int speed;
    Position food;
    int level;
    int total_eaten;
} SnakeGame;

static SnakeGame game;

static void snake_spawn_food(void);

static void snake_init(void) {
    game.length = 3;
    game.direction = DIR_RIGHT;
    game.next_direction = DIR_RIGHT;
    game.score = 0;
    game.game_over = 0;
    game.speed = 150;
    game.level = 1;
    game.total_eaten = 0;

    for (int i = 0; i < game.length; i++) {
        game.body[i].x = 5 - i;
        game.body[i].y = BOARD_HEIGHT / 2;
    }

    snake_spawn_food();
}

static void snake_spawn_food(void) {
    int valid = 0;
    while (!valid) {
        valid = 1;
        game.food.x = rand() % (BOARD_WIDTH - 2) + 1;
        game.food.y = rand() % (BOARD_HEIGHT - 2) + 1;

        for (int i = 0; i < game.length; i++) {
            if (game.body[i].x == game.food.x && game.body[i].y == game.food.y) {
                valid = 0;
                break;
            }
        }
    }
}

static void snake_draw_board(void) {
    vga_clear();
    
    vga_setcolor(0x0E, 0x00);
    vga_print("=== Kenux Snake Game ===\n");
    vga_print("Score: ");
    char score_buf[16];
    sprintf(score_buf, "%d", game.score);
    vga_print(score_buf);
    vga_print(" | Level: ");
    sprintf(score_buf, "%d", game.level);
    vga_print(score_buf);
    vga_print("\n\n");

    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (x == 0 || x == BOARD_WIDTH - 1 || y == 0 || y == BOARD_HEIGHT - 1) {
                vga_print("#");
            } else if (x == game.food.x && y == game.food.y) {
                vga_print("@");
            } else {
                int is_snake = 0;
                for (int i = 0; i < game.length; i++) {
                    if (game.body[i].x == x && game.body[i].y == y) {
                        if (i == 0)
                            vga_print("O");
                        else
                            vga_print("o");
                        is_snake = 1;
                        break;
                    }
                }
                if (!is_snake) {
                    vga_print(" ");
                }
            }
        }
        vga_print("\n");
    }

    vga_print("\nControls: W/A/S/D or Arrow Keys | Q: Quit | P: Pause\n");
}

static void snake_update(void) {
    if (game.game_over) return;

    game.direction = game.next_direction;

    Position new_head = game.body[0];
    switch (game.direction) {
        case DIR_UP:    new_head.y--; break;
        case DIR_DOWN:  new_head.y++; break;
        case DIR_LEFT:  new_head.x--; break;
        case DIR_RIGHT: new_head.x++; break;
    }

    if (new_head.x <= 0 || new_head.x >= BOARD_WIDTH - 1 ||
        new_head.y <= 0 || new_head.y >= BOARD_HEIGHT - 1) {
        game.game_over = 1;
        return;
    }

    for (int i = 0; i < game.length; i++) {
        if (new_head.x == game.body[i].x && new_head.y == game.body[i].y) {
            game.game_over = 1;
            return;
        }
    }

    for (int i = game.length; i > 0; i--) {
        game.body[i] = game.body[i - 1];
    }
    game.body[0] = new_head;

    if (new_head.x == game.food.x && new_head.y == game.food.y) {
        game.score += 10 * game.level;
        game.total_eaten++;
        
        if (game.length < MAX_SNAKE_LENGTH) {
            game.length++;
        }

        if (game.total_eaten % 5 == 0 && game.speed > 50) {
            game.level++;
            game.speed -= 10;
        }

        snake_spawn_food();
    }
}

static void snake_handle_input(char c) {
    switch (c) {
        case 'w': case 'W': case 72:
            if (game.direction != DIR_DOWN) game.next_direction = DIR_UP;
            break;
        case 's': case 'S': case 80:
            if (game.direction != DIR_UP) game.next_direction = DIR_DOWN;
            break;
        case 'a': case 'A': case 75:
            if (game.direction != DIR_RIGHT) game.next_direction = DIR_LEFT;
            break;
        case 'd': case 'D': case 77:
            if (game.direction != DIR_LEFT) game.next_direction = DIR_RIGHT;
            break;
        case 'q': case 'Q':
            game.game_over = 1;
            break;
    }
}

static void snake_show_gameover(void) {
    vga_clear();
    vga_setcolor(0x0C, 0x00);
    vga_print("\n\n\tGAME OVER!\n\n");
    vga_setcolor(0x0F, 0x00);
    vga_print("\tFinal Score: ");
    char buf[32];
    sprintf(buf, "%d\n", game.score);
    vga_print(buf);
    vga_print("\tLevel Reached: ");
    sprintf(buf, "%d\n", game.level);
    vga_print(buf);
    vga_print("\tTotal Food Eaten: ");
    sprintf(buf, "%d\n", game.total_eaten);
    vga_print(buf);
    vga_print("\n\n\tPress any key to continue...\n");

    char dummy;
    vga_getc(&dummy);
}

void snake_run(void) {
    srand(time(NULL));
    snake_init();

    char input;
    clock_t last_update = clock();

    while (!game.game_over) {
        if (kbhit()) {
            vga_getc(&input);
            snake_handle_input(input);
        }

        clock_t now = clock();
        double elapsed = ((double)(now - last_update)) / CLOCKS_PER_SEC * 1000;

        if (elapsed >= game.speed) {
            snake_update();
            last_update = now;
        }

        snake_draw_board();
    }

    snake_show_gameover();
}

int main(int argc, char** argv) {
    vga_print("Starting Kenux Snake Game...\n");
    snake_run();
    return 0;
}