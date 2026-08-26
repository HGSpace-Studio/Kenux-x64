/* ============================================================
 * textedit.c - Text Editor Application
 *
 * A simple multi-line text editor with line numbers, cursor
 * positioning, auto-scroll, and a status bar showing
 * line/column/character counts.
 * ============================================================ */

#include "textedit.h"
#include "widget.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "color.h"
#include "icon.h"
#include "msf.h"
#include "kenux_render.h"
#include "window_manager.h"
#include <arch/keyboard.h>

/* ---------- Constants ---------- */

#define TEXTEDIT_MAX_LINES   100
#define TEXTEDIT_LINE_WIDTH  80

/* Layout (relative to content rect) */
#define TE_MENU_BAR_H        18
#define TE_LINE_NUM_W        30
#define TE_LINE_HEIGHT       16
#define TE_EDIT_X            TE_LINE_NUM_W

/* Colors */
#define TE_MENU_BG           RGB(0xF0, 0xF0, 0xF0)
#define TE_MENU_TEXT         RGB(0x33, 0x33, 0x33)
#define TE_MENU_SEP          RGB(0xCC, 0xCC, 0xCC)
#define TE_LINENUM_BG        RGB(0xF0, 0xF0, 0xF0)
#define TE_LINENUM_TEXT      RGB(0x88, 0x88, 0x88)
#define TE_EDIT_BG           RGB(0xFF, 0xFF, 0xFF)
#define TE_EDIT_TEXT         RGB(0x00, 0x00, 0x00)
#define TE_CURSOR_COLOR      RGB(0x00, 0x00, 0x00)
#define TE_EDIT_BORDER       RGB(0xCC, 0xCC, 0xCC)

/* ---------- State ---------- */

typedef struct {
    char lines[TEXTEDIT_MAX_LINES][TEXTEDIT_LINE_WIDTH];
    uint32_t line_count;
    uint32_t cursor_line;
    uint32_t cursor_col;
    uint32_t scroll_offset;
    bool modified;
} textedit_state_t;

static window_t* textedit_win = NULL;
static textedit_state_t te_state;

/* ---------- Forward declarations ---------- */

static void te_draw_content(void);

/* ---------- String helpers ---------- */

static void te_str_copy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void te_str_cat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static uint32_t te_str_len(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

static void te_int_to_str(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int32_t j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* ---------- Text manipulation ---------- */

/* Insert a printable character at the cursor position */
static void te_insert_char(char c) {
    if (te_state.cursor_line >= TEXTEDIT_MAX_LINES) return;

    char* line = te_state.lines[te_state.cursor_line];
    uint32_t len = te_str_len(line);

    if (len >= TEXTEDIT_LINE_WIDTH - 1) return;

    /* Shift characters right from cursor position */
    for (int32_t i = (int32_t)len; i >= (int32_t)te_state.cursor_col; i--) {
        line[i + 1] = line[i];
    }
    line[te_state.cursor_col] = c;
    te_state.cursor_col++;
    te_state.modified = true;
}

/* Insert a newline: split the current line at the cursor position */
static void te_handle_enter(void) {
    if (te_state.cursor_line + 1 >= TEXTEDIT_MAX_LINES) return;
    if (te_state.line_count >= TEXTEDIT_MAX_LINES) return;

    /* Move all lines after cursor_line down by one */
    for (int32_t i = (int32_t)te_state.line_count; i > (int32_t)te_state.cursor_line; i--) {
        te_str_copy(te_state.lines[i], te_state.lines[i - 1]);
    }

    /* Split current line at cursor column */
    char* current = te_state.lines[te_state.cursor_line];
    char* next = te_state.lines[te_state.cursor_line + 1];

    /* Copy text after cursor to the new line */
    uint32_t j = 0;
    uint32_t col = te_state.cursor_col;
    while (current[col] && j < TEXTEDIT_LINE_WIDTH - 1) {
        next[j] = current[col];
        j++;
        col++;
    }
    next[j] = '\0';

    /* Truncate current line at cursor */
    current[te_state.cursor_col] = '\0';

    te_state.cursor_line++;
    te_state.cursor_col = 0;
    te_state.line_count++;
    te_state.modified = true;
}

/* Delete the character before the cursor */
static void te_handle_backspace(void) {
    if (te_state.cursor_col > 0) {
        /* Delete character within current line */
        char* line = te_state.lines[te_state.cursor_line];
        uint32_t len = te_str_len(line);

        for (uint32_t i = te_state.cursor_col - 1; i < len; i++) {
            line[i] = line[i + 1];
        }
        te_state.cursor_col--;
        te_state.modified = true;
    } else if (te_state.cursor_line > 0) {
        /* Merge current line with previous line */
        char* prev = te_state.lines[te_state.cursor_line - 1];
        char* curr = te_state.lines[te_state.cursor_line];

        uint32_t prev_len = te_str_len(prev);
        uint32_t curr_len = te_str_len(curr);

        /* Append current line to previous, if it fits */
        uint32_t copy_count = curr_len;
        if (prev_len + curr_len >= TEXTEDIT_LINE_WIDTH - 1) {
            copy_count = TEXTEDIT_LINE_WIDTH - 1 - prev_len;
        }

        for (uint32_t i = 0; i < copy_count; i++) {
            prev[prev_len + i] = curr[i];
        }
        prev[prev_len + copy_count] = '\0';

        /* Set cursor to the merge point */
        te_state.cursor_col = prev_len;
        te_state.cursor_line--;

        /* Move all lines after current up by one */
        for (uint32_t i = te_state.cursor_line + 1; i < te_state.line_count; i++) {
            te_str_copy(te_state.lines[i], te_state.lines[i + 1]);
        }

        /* Clear the last line */
        te_state.lines[te_state.line_count - 1][0] = '\0';
        te_state.line_count--;
        te_state.modified = true;
    }
}

/* Count total characters across all lines */
static uint32_t te_get_char_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < te_state.line_count; i++) {
        count += te_str_len(te_state.lines[i]);
        /* Count newline characters (except for the last line) */
        if (i < te_state.line_count - 1) {
            count++;
        }
    }
    return count;
}

/* ---------- Auto-scroll ---------- */

static void te_auto_scroll(void) {
    if (!textedit_win) return;

    uint32_t cx, cy, cw, ch;
    window_get_content_rect(textedit_win, &cx, &cy, &cw, &ch);

    uint32_t edit_h = ch - TE_MENU_BAR_H;
    uint32_t visible_lines = edit_h / TE_LINE_HEIGHT;
    if (visible_lines == 0) visible_lines = 1;

    /* Scroll down if cursor is below visible area */
    if (te_state.cursor_line >= te_state.scroll_offset + visible_lines) {
        te_state.scroll_offset = te_state.cursor_line - visible_lines + 1;
    }

    /* Scroll up if cursor is above visible area */
    if (te_state.cursor_line < te_state.scroll_offset) {
        te_state.scroll_offset = te_state.cursor_line;
    }
}

/* ---------- Status bar update ---------- */

static void te_update_statusbar(void) {
    if (!textedit_win) return;

    char status[64];
    te_str_copy(status, "Line: ");
    char num[16];
    te_int_to_str(te_state.cursor_line + 1, num);
    te_str_cat(status, num);

    te_str_cat(status, "  Col: ");
    te_int_to_str(te_state.cursor_col + 1, num);
    te_str_cat(status, num);

    te_str_cat(status, "  Chars: ");
    te_int_to_str(te_get_char_count(), num);
    te_str_cat(status, num);

    if (te_state.modified) {
        te_str_cat(status, "  *");
    }

    window_set_statusbar(textedit_win, status);
}

/* ---------- Drawing ---------- */

static void te_draw_menu_bar(uint32_t cx, uint32_t cy, uint32_t cw) {
    /* Menu bar background */
    fb_fill_rect(cx, cy, cw, TE_MENU_BAR_H, TE_MENU_BG);

    /* Separator below menu bar */
    gfx_draw_hline(cx, cy + TE_MENU_BAR_H, cw, TE_MENU_SEP);

    /* Menu items */
    uint32_t x = cx + 8;
    uint32_t y = cy + 2;

    font_draw_text(x, y, "File", TE_MENU_TEXT);
    x += font_text_width("File") + 20;

    font_draw_text(x, y, "Edit", TE_MENU_TEXT);
    x += font_text_width("Edit") + 20;

    font_draw_text(x, y, "View", TE_MENU_TEXT);
}

static void te_draw_line_numbers(uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch) {
    uint32_t ln_x = cx;
    uint32_t ln_y = cy + TE_MENU_BAR_H;
    uint32_t ln_h = ch - TE_MENU_BAR_H;

    /* Line number column background */
    fb_fill_rect(ln_x, ln_y, TE_LINE_NUM_W, ln_h, TE_LINENUM_BG);

    /* Separator between line numbers and edit area */
    gfx_draw_vline(ln_x + TE_LINE_NUM_W, ln_y, ln_h, TE_EDIT_BORDER);

    /* Draw line numbers */
    uint32_t visible_lines = ln_h / TE_LINE_HEIGHT;
    for (uint32_t i = 0; i < visible_lines; i++) {
        uint32_t line_idx = te_state.scroll_offset + i;
        if (line_idx >= te_state.line_count) break;

        char num_str[8];
        te_int_to_str(line_idx + 1, num_str);
        uint32_t num_w = font_text_width(num_str);

        /* Right-align the line number in the 30px column */
        uint32_t text_x = ln_x + TE_LINE_NUM_W - num_w - 4;
        uint32_t text_y = ln_y + i * TE_LINE_HEIGHT;

        font_draw_text(text_x, text_y, num_str, TE_LINENUM_TEXT);
    }
}

static void te_draw_edit_area(uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch) {
    uint32_t edit_x = cx + TE_EDIT_X;
    uint32_t edit_y = cy + TE_MENU_BAR_H;
    uint32_t edit_w = cw - TE_EDIT_X;
    uint32_t edit_h = ch - TE_MENU_BAR_H;

    /* Edit area background */
    fb_fill_rect(edit_x, edit_y, edit_w, edit_h, TE_EDIT_BG);

    /* Draw text lines */
    uint32_t visible_lines = edit_h / TE_LINE_HEIGHT;
    for (uint32_t i = 0; i < visible_lines; i++) {
        uint32_t line_idx = te_state.scroll_offset + i;
        if (line_idx >= te_state.line_count) break;

        uint32_t text_y = edit_y + i * TE_LINE_HEIGHT;
        font_draw_text(edit_x + 4, text_y, te_state.lines[line_idx], TE_EDIT_TEXT);
    }

    /* Draw cursor (blinking vertical line) */
    {
        uint32_t cursor_y_offset = te_state.cursor_line - te_state.scroll_offset;
        if (cursor_y_offset < visible_lines) {
            uint32_t cursor_x = edit_x + 4 + te_state.cursor_col * FONT_WIDTH;
            uint32_t cursor_y = edit_y + cursor_y_offset * TE_LINE_HEIGHT;

            /* Draw cursor as a vertical line */
            gfx_draw_vline(cursor_x, cursor_y, TE_LINE_HEIGHT, TE_CURSOR_COLOR);
            gfx_draw_vline(cursor_x + 1, cursor_y, TE_LINE_HEIGHT, TE_CURSOR_COLOR);
        }
    }
}

static void te_draw_content(void) {
    if (!textedit_win) return;

    uint32_t cx, cy, cw, ch;
    window_get_content_rect(textedit_win, &cx, &cy, &cw, &ch);

    /* Menu bar */
    te_draw_menu_bar(cx, cy, cw);

    /* Line numbers */
    te_draw_line_numbers(cx, cy, cw, ch);

    /* Edit area */
    te_draw_edit_area(cx, cy, cw, ch);

    /* Update status bar */
    te_update_statusbar();
}

/* ---------- Public API ---------- */

window_t* textedit_create(void) {
    if (textedit_win) return textedit_win;

    /* Initialize state: start with one empty line */
    for (uint32_t i = 0; i < TEXTEDIT_MAX_LINES; i++) {
        te_state.lines[i][0] = '\0';
    }
    te_state.line_count = 1;
    te_state.cursor_line = 0;
    te_state.cursor_col = 0;
    te_state.scroll_offset = 0;
    te_state.modified = false;

    /* Create window 520x360 */
    textedit_win = window_create(220, 60, 520, 360, "Text Editor");
    textedit_win->titlebar_color = msf_settings.titlebar_active;
    textedit_win->visible = false;
    window_set_statusbar(textedit_win, "Line: 1  Col: 1  Chars: 0");

    wm_add_window(textedit_win);

    return textedit_win;
}

void textedit_on_show(void) {
    if (textedit_win && textedit_win->visible) {
        te_auto_scroll();
        te_draw_content();
    }
}

void textedit_handle_key(uint16_t key_code, uint16_t key_char) {
    if (!textedit_win || !textedit_win->visible) return;

    if (key_code == KEY_BACKSPACE) {
        /* Backspace: delete character before cursor */
        te_handle_backspace();
    } else if (key_code == KEY_ENTER) {
        /* Enter: insert newline */
        te_handle_enter();
    } else if (key_char >= 32 && key_char <= 126) {
        /* Printable ASCII character */
        te_insert_char((char)key_char);
    } else {
        /* Ignore non-printable keys */
        return;
    }

    /* Auto-scroll to keep cursor visible */
    te_auto_scroll();

    /* Redraw content */
    te_draw_content();
    wm_paint();
}
