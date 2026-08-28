#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vga.h>

#define GRID_SIZE 16
#define MINE_COUNT 40

typedef enum {
    CELL_HIDDEN,
    CELL_REVEALED,
    CELL_FLAGGED
} CellState;

typedef struct {
    int is_mine;
    int adjacent_mines;
    CellState state;
} Cell;

static Cell grid[GRID_SIZE][GRID_SIZE];
static int cursor_x, cursor_y;
static int game_over;
static int game_won;
static int revealed_count;
static int flag_count;
static int first_click;

static void minesweeper_init(void) {
    memset(grid, 0, sizeof(grid));
    cursor_x = GRID_SIZE / 2;
    cursor_y = GRID_SIZE / 2;
    game_over = 0;
    game_won = 0;
    revealed_count = 0;
    flag_count = 0;
    first_click = 1;
}

static void place_mines(int exclude_x, int exclude_y) {
    int placed = 0;
    srand(time(NULL));
    
    while (placed < MINE_COUNT) {
        int x = rand() % GRID_SIZE;
        int y = rand() % GRID_SIZE;
        
        if (!grid[y][x].is_mine &&
            !(abs(x - exclude_x) <= 1 && abs(y - exclude_y) <= 1)) {
            grid[y][x].is_mine = 1;
            placed++;
        }
    }
    
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (!grid[y][x].is_mine) {
                int count = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < GRID_SIZE && 
                            ny >= 0 && ny < GRID_SIZE &&
                            grid[ny][nx].is_mine) {
                            count++;
                        }
                    }
                }
                grid[y][x].adjacent_mines = count;
            }
        }
    }
}

static void reveal_cell(int x, int y) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) return;
    if (grid[y][x].state != CELL_HIDDEN) return;
    
    grid[y][x].state = CELL_REVEALED;
    revealed_count++;
    
    if (grid[y][x].is_mine) {
        game_over = 1;
        for (int cy = 0; cy < GRID_SIZE; cy++) {
            for (int cx = 0; cx < GRID_SIZE; cx++) {
                if (grid[cy][cx].is_mine) {
                    grid[cy][cx].state = CELL_REVEALED;
                }
            }
        }
        return;
    }
    
    if (grid[y][x].adjacent_mines == 0) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                reveal_cell(x + dx, y + dy);
            }
        }
    }
    
    if (revealed_count == GRID_SIZE * GRID_SIZE - MINE_COUNT) {
        game_won = 1;
        game_over = 1;
    }
}

static void toggle_flag(int x, int y) {
    if (grid[y][x].state == CELL_HIDDEN) {
        grid[y][x].state = CELL_FLAGGED;
        flag_count++;
    } else if (grid[y][x].state == CELL_FLAGGED) {
        grid[y][x].state = CELL_HIDDEN;
        flag_count--;
    }
}

static void chord_reveal(int x, int y) {
    if (grid[y][x].state != CELL_REVEALED) return;
    
    int flags_around = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < GRID_SIZE && 
                ny >= 0 && ny < GRID_SIZE &&
                grid[ny][nx].state == CELL_FLAGGED) {
                flags_around++;
            }
        }
    }
    
    if (flags_around == grid[y][x].adjacent_mines) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < GRID_SIZE && 
                    ny >= 0 && ny < GRID_SIZE &&
                    grid[ny][nx].state == CELL_HIDDEN) {
                    reveal_cell(nx, ny);
                }
            }
        }
    }
}

static void draw_grid(void) {
    vga_clear();
    
    vga_setcolor(0x0E, 0);
    vga_print("=== Kenux MINESWEEPER ===\n\n");
    
    vga_setcolor(0x07, 0);
    char info[64];
    sprintf(info, "Mines: %d/%d   Flags: %d\n", MINE_COUNT, MINE_COUNT - flag_count, flag_count);
    vga_print(info);
    vga_print("\n  ");
    
    for (int x = 0; x < GRID_SIZE; x++) {
        char col_label[2];
        sprintf(col_label, "%X", x % 16);
        vga_print(col_label);
        vga_print(" ");
    }
    vga_print("\n");
    
    for (int y = 0; y < GRID_SIZE; y++) {
        char row_label[3];
        sprintf(row_label, "%X ", y % 16);
        vga_print(row_label);
        
        for (int x = 0; x < GRID_SIZE; x++) {
            int is_cursor = (x == cursor_x && y == cursor_y);
            
            if (grid[y][x].state == CELL_HIDDEN) {
                if (is_cursor) {
                    vga_setcolor(0x70, 0);
                } else {
                    vga_setcolor(0x17, 0);
                }
                vga_print("# ");
            } else if (grid[y][x].state == CELL_FLAGGED) {
                if (is_cursor) {
                    vga_setcolor(0x74, 0);
                } else {
                    vga_setcolor(0x47, 0);
                }
                vga_print("F ");
            } else {
                if (is_cursor) {
                    vga_setcolor(0x70, 0);
                } else {
                    vga_setcolor(0x07, 0);
                }
                
                if (grid[y][x].is_mine) {
                    vga_setcolor(0x0C, 0);
                    vga_print("* ");
                } else if (grid[y][x].adjacent_mines > 0) {
                    char num_str[2];
                    sprintf(num_str, "%d", grid[y][x].adjacent_mines);
                    
                    static const int num_colors[] = {0, 9, 0xA, 0xC, 0x4, 0xE, 5, 0, 7};
                    vga_setcolor(num_colors[grid[y][x].adjacent_mines], 0);
                    vga_print(num_str);
                    vga_print(" ");
                } else {
                    vga_print(". ");
                }
            }
        }
        vga_print("\n");
    }
    
    vga_setcolor(0x0A, 0);
    vga_print("\nControls:\n");
    vga_print("Arrows: Move | Enter/Space: Reveal | F: Flag | C: Chord | R: Restart | Q: Quit\n");
    
    if (game_over) {
        vga_setcolor(0x0E, 0);
        if (game_won) {
            vga_print("\n*** YOU WIN! ***\n");
        } else {
            vga_print("\n*** GAME OVER ***\n");
        }
        vga_print("Press R to restart or Q to quit\n");
    }
}

void minesweeper_run(void) {
    minesweeper_init();
    
    while (1) {
        draw_grid();
        
        if (kbhit()) {
            char key;
            vga_getc(&key);
            
            switch (key) {
                case 72: case 'w': case 'W':
                    if (cursor_y > 0) cursor_y--;
                    break;
                case 80: case 's': case 'S':
                    if (cursor_y < GRID_SIZE - 1) cursor_y++;
                    break;
                case 75: case 'a': case 'A':
                    if (cursor_x > 0) cursor_x--;
                    break;
                case 77: case 'd': case 'D':
                    if (cursor_x < GRID_SIZE - 1) cursor_x++;
                    break;
                case ' ': case '\r':
                    if (!game_over) {
                        if (first_click) {
                            place_mines(cursor_x, cursor_y);
                            first_click = 0;
                        }
                        reveal_cell(cursor_x, cursor_y);
                    }
                    break;
                case 'f': case 'F':
                    if (!game_over) {
                        toggle_flag(cursor_x, cursor_y);
                    }
                    break;
                case 'c': case 'C':
                    if (!game_over) {
                        chord_reveal(cursor_x, cursor_y);
                    }
                    break;
                case 'r': case 'R':
                    minesweeper_init();
                    break;
                case 'q': case 'Q':
                    return;
            }
        }
    }
}

int main(int argc, char** argv) {
    vga_print("Starting Kenux Minesweeper...\n");
    minesweeper_run();
    return 0;
}