#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vga.h>

#define SIZE 4

static int board[SIZE][SIZE];
static int score;
static int best_score;
static int game_over;
static int won;

static void init_2048(void) {
    memset(board, 0, sizeof(board));
    score = 0;
    game_over = 0;
    won = 0;
    
    srand(time(NULL));
    
    for (int i = 0; i < 2; i++) {
        int x, y;
        do {
            x = rand() % SIZE;
            y = rand() % SIZE;
        } while (board[y][x] != 0);
        
        board[y][x] = (rand() % 10 == 0) ? 4 : 2;
    }
}

static void add_tile(void) {
    int empty[SIZE * SIZE];
    int empty_count = 0;
    
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            if (board[y][x] == 0) {
                empty[empty_count++] = y * SIZE + x;
            }
        }
    }
    
    if (empty_count > 0) {
        int pos = empty[rand() % empty_count];
        board[pos / SIZE][pos % SIZE] = (rand() % 10 == 0) ? 4 : 2;
    }
}

static int can_move(void) {
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            if (board[y][x] == 0) return 1;
            
            if (x < SIZE - 1 && board[y][x] == board[y][x+1]) return 1;
            if (y < SIZE - 1 && board[y][x] == board[y+1][x]) return 1;
        }
    }
    return 0;
}

static int slide_left(void) {
    int moved = 0;
    
    for (int y = 0; y < SIZE; y++) {
        int merged[SIZE] = {0};
        
        for (int x = 1; x < SIZE; x++) {
            if (board[y][x] == 0) continue;
            
            int new_x = x;
            while (new_x > 0 && board[y][new_x - 1] == 0) {
                new_x--;
            }
            
            if (new_x > 0 && board[y][new_x - 1] == board[y][x] && !merged[new_x - 1]) {
                board[y][new_x - 1] *= 2;
                score += board[y][new_x - 1];
                board[y][x] = 0;
                merged[new_x - 1] = 1;
                moved = 1;
                
                if (board[y][new_x - 1] == 2048 && !won) {
                    won = 1;
                }
            } else if (new_x != x) {
                board[y][new_x] = board[y][x];
                board[y][x] = 0;
                moved = 1;
            }
        }
    }
    
    return moved;
}

static void rotate_board(void) {
    int temp[SIZE][SIZE];
    
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            temp[x][SIZE - 1 - y] = board[y][x];
        }
    }
    
    memcpy(board, temp, sizeof(board));
}

static int move(int direction) {
    int moved = 0;
    
    for (int r = 0; r < direction; r++) {
        rotate_board();
    }
    
    moved = slide_left();
    
    for (int r = 0; r < (4 - direction) % 4; r++) {
        rotate_board();
    }
    
    return moved;
}

static void get_color_for_value(int val, int* fg, int* bg) {
    switch (val) {
        case 0:    *fg = 7;  *bg = 0;  break;
        case 2:    *fg = 0;  *bg = 6;  break;
        case 4:    *fg = 0;  *bg = 14; break;
        case 8:    *fg = 0;  *bg = 10; break;
        case 16:   *fg = 0;  *bg = 11; break;
        case 32:   *fg = 0;  *bg = 13; break;
        case 64:   *fg = 0;  *bg = 12; break;
        case 128:  *fg = 0;  *bg = 9;  break;
        case 256:  *fg = 0;  *bg = 5;  break;
        case 512:  *fg = 7;  *bg = 4;  break;
        case 1024: *fg = 7;  *bg = 2;  break;
        case 2048:*fg = 15; *bg = 14; break;
        default:   *fg = 15; *bg = 1;  break;
    }
}

static void draw_board(void) {
    vga_clear();
    
    vga_setcolor(0x0E, 0);
    vga_print("=== Kenux 2048 ===\n\n");
    
    vga_setcolor(0x07, 0);
    char info[64];
    sprintf(info, "Score: %-10d Best: %d\n", score, best_score);
    vga_print(info);
    vga_print("\n");
    
    vga_print("+------+------+------+------+\n");
    
    for (int y = 0; y < SIZE; y++) {
        vga_print("|");
        
        for (int x = 0; x < SIZE; x++) {
            int fg, bg;
            get_color_for_value(board[y][x], &fg, &bg);
            vga_setcolor(fg, bg);
            
            if (board[y][x] == 0) {
                vga_print("      |");
            } else {
                char cell[8];
                sprintf(cell, "%5d |", board[y][x]);
                vga_print(cell);
            }
        }
        
        vga_print("\n");
        vga_print("+------+------+------+------+\n");
    }
    
    if (won) {
        vga_setcolor(0x0A, 0);
        vga_print("\n*** YOU WIN! *** (Press any key to continue)\n");
    } else if (game_over) {
        vga_setcolor(0x0C, 0);
        vga_print("\n*** GAME OVER ***\n");
    }
    
    vga_setcolor(0x0A, 0);
    vga_print("\nControls: Arrow Keys or WASD to move | R: Restart | Q: Quit\n");
}

static void show_final_screen(const char* message) {
    vga_clear();
    vga_setcolor(0x0E, 0);
    vga_print(message);
    vga_setcolor(0x0F, 0);
    vga_print("\n\n\tFinal Score: ");
    char buf[32];
    sprintf(buf, "%d\n", score);
    vga_print(buf);
    vga_print("\tBest Score: ");
    sprintf(buf, "%d\n", best_score);
    vga_print(buf);
    vga_print("\n\tPress R to play again or Q to quit...\n");
}

void game2048_run(void) {
    init_2048();
    
    int continue_after_win = 0;
    
    while (1) {
        draw_board();
        
        if (won && !continue_after_win) {
            char dummy;
            vga_getc(&dummy);
            continue_after_win = 1;
        }
        
        if (kbhit()) {
            char key;
            vga_getc(&key);
            
            int dir = -1;
            switch (key) {
                case 75: case 'a': case 'A':
                    dir = 3;
                    break;
                case 77: case 'd': case 'D':
                    dir = 1;
                    break;
                case 72: case 'w': case 'W':
                    dir = 0;
                    break;
                case 80: case 's': case 'S':
                    dir = 2;
                    break;
                case 'r': case 'R':
                    if (score > best_score) best_score = score;
                    init_2048();
                    continue_after_win = 0;
                    break;
                case 'q': case 'Q':
                    if (score > best_score) best_score = score;
                    return;
            }
            
            if (dir >= 0 && !game_over) {
                if (move(dir)) {
                    add_tile();
                    if (!can_move()) {
                        game_over = 1;
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    vga_print("Starting Kenux 2048...\n");
    game2048_run();
    return 0;
}