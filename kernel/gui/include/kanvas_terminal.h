#ifndef KANVAS_TERMINAL_H
#define KANVAS_TERMINAL_H

#include "kapi_kanvasui.h"
#include "kapi_graphics2d.h"

#define KTERM_WIN_W            720
#define KTERM_WIN_H            480
#define KTERM_MAX_LINES        4096
#define KTERM_MAX_COLS         256
#define KTERM_MAX_HISTORY      512
#define KTERM_TAB_BAR_H        32
#define KTERM_SCROLLBAR_W      8
#define KTERM_RADIUS           10

typedef enum {
    KTERM_COLOR_BLACK = 0,
    KTERM_COLOR_RED,
    KTERM_COLOR_GREEN,
    KTERM_COLOR_YELLOW,
    KTERM_COLOR_BLUE,
    KTERM_COLOR_MAGENTA,
    KTERM_COLOR_CYAN,
    KTERM_COLOR_WHITE,
    KTERM_COLOR_BRIGHT_BLACK,
    KTERM_COLOR_BRIGHT_RED,
    KTERM_COLOR_BRIGHT_GREEN,
    KTERM_COLOR_BRIGHT_YELLOW,
    KTERM_COLOR_BRIGHT_BLUE,
    KTERM_COLOR_BRIGHT_MAGENTA,
    KTERM_COLOR_BRIGHT_CYAN,
    KTERM_COLOR_BRIGHT_WHITE
} kterm_color_t;

typedef struct {
    char chars[4];
    uint8_t fg;
    uint8_t bg;
    bool bold;
    bool italic;
    bool underline;
    bool reverse;
} kterm_cell_t;

typedef struct {
    kterm_cell_t cells[KTERM_MAX_COLS];
    int length;
} kterm_line_t;

typedef struct {
    int fd;
    int pid;
    char cwd[256];
    char title[64];
    kterm_line_t lines[KTERM_MAX_LINES];
    int line_count;
    int scroll_offset;
    int cursor_col;
    int cursor_row;
    bool cursor_visible;
    kterm_color_t default_fg;
    kterm_color_t default_bg;
    char history[KTERM_MAX_HISTORY][256];
    int history_count;
    int history_idx;
    char input_buf[256];
    int input_len;
    int input_pos;
} kterm_tab_t;

typedef struct {
    bool visible;
    int x, y, w, h;
    kterm_tab_t tabs[8];
    int tab_count;
    int active_tab;
    int font_w;
    int font_h;
    int cols;
    int rows;
    bool show_scrollbar;
    kui_color_t bg_color;
    kui_color_t fg_color;
    kui_color_t accent_color;
    kui_color_t selection_color;
    uint32_t ansi_colors[16];
} kanvas_terminal_t;

kanvas_terminal_t* kanvas_terminal_create(void);
void kanvas_terminal_destroy(kanvas_terminal_t* term);
void kanvas_terminal_paint(kanvas_terminal_t* term, uint32_t* fb, int stride, int fw, int fh);
void kanvas_terminal_update(kanvas_terminal_t* term, uint64_t now_ms);
void kanvas_terminal_handle_key(kanvas_terminal_t* term, int key, bool down, uint32_t mods);
void kanvas_terminal_handle_char(kanvas_terminal_t* term, uint32_t ch);
void kanvas_terminal_handle_mouse(kanvas_terminal_t* term, int mx, int my, bool left, bool right);
void kanvas_terminal_write(kanvas_terminal_t* term, const char* data, int len);
void kanvas_terminal_new_tab(kanvas_terminal_t* term);
void kanvas_terminal_close_tab(kanvas_terminal_t* term, int idx);
void kanvas_terminal_resize(kanvas_terminal_t* term, int w, int h);

#endif