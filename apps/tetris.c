#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vga.h>

#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define PREVIEW_SIZE 4

typedef struct {
    int x, y;
} Point;

typedef struct {
    int x, y;
    Point blocks[4];
    int color;
} Tetromino;

typedef enum {
    SHAPE_I, SHAPE_O, SHAPE_T, SHAPE_S,
    SHAPE_Z, SHAPE_J, SHAPE_L, SHAPE_COUNT
} ShapeType;

static int board[BOARD_HEIGHT][BOARD_WIDTH];
static Tetromino current_piece;
static Tetromino next_piece;
static int score;
static int level;
static int lines_cleared;
static int game_over;
static int drop_timer;
static int drop_interval;

static const int shapes[SHAPE_COUNT][4][2] = {
    {{0,0}, {1,0}, {2,0}, {3,0}},
    {{0,0}, {1,0}, {0,1}, {1,1}},
    {{1,0}, {0,1}, {1,1}, {2,1}},
    {{0,1}, {1,1}, {1,0}, {2,0}},
    {{0,0}, {1,0}, {1,1}, {2,1}},
    {{0,1}, {1,1}, {2,1}, {2,0}},
    {{0,0}, {0,1}, {1,1}, {2,1}}
};

static void tetris_init(void) {
    memset(board, 0, sizeof(board));
    score = 0;
    level = 1;
    lines_cleared = 0;
    game_over = 0;
    drop_timer = 0;
    drop_interval = 500;
    
    srand(time(NULL));
}

static Tetromino create_tetromino(ShapeType shape) {
    Tetromino t;
    t.color = (shape % 7) + 1;
    
    for (int i = 0; i < 4; i++) {
        t.blocks[i].x = shapes[shape][i][0];
        t.blocks[i].y = shapes[shape][i][1];
    }
    
    return t;
}

static void spawn_piece(void) {
    current_piece = next_piece;
    current_piece.x = BOARD_WIDTH / 2 - 1;
    current_piece.y = 0;
    
    ShapeType next_shape = rand() % SHAPE_COUNT;
    next_piece = create_tetromino(next_shape);
    
    for (int i = 0; i < 4; i++) {
        int bx = current_piece.x + current_piece.blocks[i].x;
        int by = current_piece.y + current_piece.blocks[i].y;
        
        if (by >= 0 && board[by][bx] != 0) {
            game_over = 1;
        }
    }
}

static int is_valid_position(Tetromino* piece, int offset_x, int offset_y) {
    for (int i = 0; i < 4; i++) {
        int new_x = piece->x + piece->blocks[i].x + offset_x;
        int new_y = piece->y + piece->blocks[i].y + offset_y;
        
        if (new_x < 0 || new_x >= BOARD_WIDTH || new_y >= BOARD_HEIGHT) {
            return 0;
        }
        
        if (new_y >= 0 && board[new_y][new_x] != 0) {
            return 0;
        }
    }
    return 1;
}

static void rotate_piece(void) {
    if (current_piece.color == 2) return;
    
    Tetromino temp = current_piece;
    for (int i = 0; i < 4; i++) {
        int old_x = temp.blocks[i].x - 1;
        int old_y = temp.blocks[i].y - 1;
        temp.blocks[i].x = old_y + 1;
        temp.blocks[i].y = -old_x + 1;
    }
    
    if (is_valid_position(&temp, 0, 0)) {
        current_piece = temp;
    } else if (is_valid_position(&temp, -1, 0)) {
        current_piece = temp;
        current_piece.x--;
    } else if (is_valid_position(&temp, 1, 0)) {
        current_piece = temp;
        current_piece.x++;
    } else if (is_valid_position(&temp, -2, 0)) {
        current_piece = temp;
        current_piece.x -= 2;
    } else if (is_valid_position(&temp, 2, 0)) {
        current_piece = temp;
        current_piece.x += 2;
    }
}

static void lock_piece(void) {
    for (int i = 0; i < 4; i++) {
        int by = current_piece.y + current_piece.blocks[i].y;
        int bx = current_piece.x + current_piece.blocks[i].x;
        
        if (by >= 0) {
            board[by][bx] = current_piece.color;
        }
    }
}

static void clear_lines(void) {
    int cleared = 0;
    
    for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (board[y][x] == 0) {
                full = 0;
                break;
            }
        }
        
        if (full) {
            cleared++;
            for (int ny = y; ny > 0; ny--) {
                for (int nx = 0; nx < BOARD_WIDTH; nx++) {
                    board[ny][nx] = board[ny-1][nx];
                }
            }
            for (int nx = 0; nx < BOARD_WIDTH; nx++) {
                board[0][nx] = 0;
            }
            y++;
        }
    }
    
    if (cleared > 0) {
        int points[] = {0, 100, 300, 500, 800};
        score += points[cleared] * level;
        lines_cleared += cleared;
        
        if (lines_cleared / 10 >= level && level < 15) {
            level++;
            drop_interval -= 30;
            if (drop_interval < 50) drop_interval = 50;
        }
    }
}

static void move_piece(int dx, int dy) {
    if (is_valid_position(&current_piece, dx, dy)) {
        current_piece.x += dx;
        current_piece.y += dy;
    } else if (dy > 0) {
        lock_piece();
        clear_lines();
        spawn_piece();
    }
}

static void hard_drop(void) {
    while (is_valid_position(&current_piece, 0, 1)) {
        current_piece.y++;
        score += 2;
    }
    lock_piece();
    clear_lines();
    spawn_piece();
}

static void draw_block(int x, int y, int color, char c) {
    static const char block_chars[] = " IOZSTJL";
    vga_setcolor(color, 0);
    vga_print_char(x * 2, y + 3, c ? c : block_chars[color]);
    vga_print_char(x * 2 + 1, y + 3, c ? c : block_chars[color]);
}

static void draw_board(void) {
    vga_clear();
    
    vga_setcolor(0x0E, 0);
    vga_print("=== Kenux TETRIS ===\n\n");
    
    vga_setcolor(0x07, 0);
    char info[128];
    sprintf(info, "Score: %-8d Level: %d\n", score, level);
    vga_print(info);
    sprintf(info, "Lines: %-7d Next:\n", lines_cleared);
    vga_print(info);
    
    for (int py = 0; py < PREVIEW_SIZE; py++) {
        vga_print("  ");
        for (int px = 0; px < PREVIEW_SIZE; px++) {
            int found = 0;
            for (int i = 0; i < 4; i++) {
                if (next_piece.blocks[i].x == px && next_piece.blocks[i].y == py) {
                    draw_block(px + 12, py, next_piece.color, 0);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                vga_print("  ");
            }
        }
        vga_print("\n");
    }
    
    vga_print("\n");
    
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        vga_print(" ");
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (board[y][x] != 0) {
                draw_block(x, y, board[y][x], 0);
            } else {
                int is_piece = 0;
                for (int i = 0; i < 4; i++) {
                    if (current_piece.x + current_piece.blocks[i].x == x &&
                        current_piece.y + current_piece.blocks[i].y == y) {
                        draw_block(x, y, current_piece.color, 0);
                        is_piece = 1;
                        break;
                    }
                }
                if (!is_piece) {
                    vga_print("..");
                }
            }
        }
        vga_print("\n");
    }
    
    vga_print(" ");
    for (int x = 0; x < BOARD_WIDTH; x++) {
        vga_print("--");
    }
    vga_print("\n");
    
    vga_setcolor(0x0A, 0);
    vga_print("\nControls:\n");
    vga_print("< > Move | ^ Rotate | Space Drop | P Pause | Q Quit\n");
}

static void show_gameover(void) {
    vga_clear();
    vga_setcolor(0x0C, 0);
    vga_print("\n\n\t*** GAME OVER ***\n\n");
    vga_setcolor(0x0F, 0);
    vga_print("\tFinal Score: ");
    char buf[32];
    sprintf(buf, "%d\n", score);
    vga_print(buf);
    vga_print("\tLevel: ");
    sprintf(buf, "%d\n", level);
    vga_print(buf);
    vga_print("\tLines Cleared: ");
    sprintf(buf, "%d\n", lines_cleared);
    vga_print(buf);
    vga_print("\n\tPress any key to continue...\n");
    
    char dummy;
    vga_getc(&dummy);
}

void tetris_run(void) {
    tetris_init();
    
    ShapeType first_shape = rand() % SHAPE_COUNT;
    next_piece = create_tetromino(first_shape);
    spawn_piece();
    
    clock_t last_drop = clock();
    int paused = 0;
    
    while (!game_over) {
        if (kbhit()) {
            char key;
            vga_getc(&key);
            
            if (paused) {
                if (key == 'p' || key == 'P') paused = 0;
                continue;
            }
            
            switch (key) {
                case 'a': case 'A': case 75:
                    move_piece(-1, 0);
                    break;
                case 'd': case 'D': case 77:
                    move_piece(1, 0);
                    break;
                case 's': case 'S':
                    move_piece(0, 1);
                    score += 1;
                    break;
                case 'w': case 'W':
                    rotate_piece();
                    break;
                case 80:
                    move_piece(0, 1);
                    score += 1;
                    break;
                case 72:
                    rotate_piece();
                    break;
                case ' ':
                    hard_drop();
                    break;
                case 'p': case 'P':
                    paused = 1;
                    vga_setcolor(0x0E, 0);
                    vga_print("\n\t** PAUSED **\n");
                    break;
                case 'q': case 'Q':
                    game_over = 1;
                    break;
            }
        }
        
        if (!paused) {
            clock_t now = clock();
            double elapsed = ((double)(now - last_drop)) / CLOCKS_PER_SEC * 1000;
            
            if (elapsed >= drop_interval) {
                move_piece(0, 1);
                last_drop = now;
            }
            
            draw_board();
        }
    }
    
    show_gameover();
}

int main(int argc, char** argv) {
    vga_print("Starting Kenux Tetris...\n");
    tetris_run();
    return 0;
}