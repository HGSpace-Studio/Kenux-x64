#include "kanvas_terminal.h"
#include "kapi.h"
#include <string.h>

static uint32_t t_col32(kui_color_t c) { return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; }

static void t_fill_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > fw) w = fw - x; if (y + h > fh) h = fh - y;
    if (w <= 0 || h <= 0) return;
    for (int r = y; r < y + h; r++) { uint32_t* p = (uint32_t*)((uint8_t*)fb + r * stride); for (int c = x; c < x + w; c++) p[c] = color; }
}

kanvas_terminal_t* kanvas_terminal_create(void)
{
    kanvas_terminal_t* t = (kanvas_terminal_t*)kapi_kmalloc(sizeof(kanvas_terminal_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(kanvas_terminal_t));
    t->visible = false;
    t->w = KTERM_WIN_W; t->h = KTERM_WIN_H;
    t->tab_count = 1; t->active_tab = 0;
    t->font_w = 8; t->font_h = 16;
    t->cols = (KTERM_WIN_W - KTERM_SCROLLBAR_W) / t->font_w;
    t->rows = (KTERM_WIN_H - KTERM_TAB_BAR_H) / t->font_h;
    t->show_scrollbar = true;
    t->bg_color = (kui_color_t){20, 20, 20, 255};
    t->fg_color = (kui_color_t){230, 230, 230, 255};
    t->accent_color = (kui_color_t){98, 0, 238, 255};
    t->selection_color = (kui_color_t){98, 0, 238, 64};
    uint32_t ansi[] = {
        0x1A1A1AFF, 0xE04040FF, 0x40C040FF, 0xC0C040FF,
        0x4040E0FF, 0xC040C0FF, 0x40C0C0FF, 0xC0C0C0FF,
        0x606060FF, 0xFF6060FF, 0x60FF60FF, 0xFFFF60FF,
        0x6060FFFF, 0xFF60FFFF, 0x60FFFFFF, 0xFFFFFFFF
    };
    memcpy(t->ansi_colors, ansi, sizeof(ansi));
    kterm_tab_t* tab = &t->tabs[0];
    memcpy(tab->cwd, "/home", 5);
    memcpy(tab->title, "Terminal", 8);
    tab->cursor_visible = true;
    tab->default_fg = KTERM_COLOR_WHITE;
    tab->default_bg = KTERM_COLOR_BLACK;
    return t;
}

void kanvas_terminal_destroy(kanvas_terminal_t* term)
{
    if (!term) return;
    kapi_kfree(term);
}

void kanvas_terminal_paint(kanvas_terminal_t* term, uint32_t* fb, int stride, int fw, int fh)
{
    if (!term || !term->visible || !fb) return;
    uint32_t bg = t_col32(term->bg_color);
    uint32_t fg = t_col32(term->fg_color);
    uint32_t accent = t_col32(term->accent_color);
    t_fill_rect(fb, stride, fw, fh, term->x, term->y, term->w, term->h, bg);
    int tab_y = term->y;
    t_fill_rect(fb, stride, fw, fh, term->x, tab_y, term->w, KTERM_TAB_BAR_H, 0x2A2A2AFF);
    int tab_x = term->x + 4;
    for (int i = 0; i < term->tab_count; i++) {
        int tw = 100;
        uint32_t tbg = i == term->active_tab ? 0x3A3A3AFF : 0x2A2A2AFF;
        t_fill_rect(fb, stride, fw, fh, tab_x, tab_y + 2, tw, KTERM_TAB_BAR_H - 4, tbg);
        kui_draw_text(fb, stride, fw, fh, tab_x + 8, tab_y + 8, term->tabs[i].title, i == term->active_tab ? fg : 0x808080FF, 12, 0);
        tab_x += tw + 2;
    }
    kterm_tab_t* tab = &term->tabs[term->active_tab];
    int content_y = term->y + KTERM_TAB_BAR_H;
    int max_lines = term->rows;
    int start_line = tab->line_count - max_lines - tab->scroll_offset;
    if (start_line < 0) start_line = 0;
    for (int i = 0; i < max_lines; i++) {
        int line_idx = start_line + i;
        if (line_idx >= tab->line_count) break;
        kterm_line_t* line = &tab->lines[line_idx];
        int line_x = term->x + 4;
        int line_y = content_y + i * term->font_h;
        for (int c = 0; c < line->length && c < term->cols; c++) {
            kterm_cell_t* cell = &line->cells[c];
            uint32_t cell_fg = cell->fg < 16 ? term->ansi_colors[cell->fg] : fg;
            uint32_t cell_bg = cell->bg < 16 ? term->ansi_colors[cell->bg] : bg;
            if (cell->reverse) { uint32_t tmp = cell_fg; cell_fg = cell_bg; cell_bg = tmp; }
            if (cell_bg != bg) t_fill_rect(fb, stride, fw, fh, line_x + c * term->font_w, line_y, term->font_w, term->font_h, cell_bg);
            if (cell->chars[0]) kui_draw_text(fb, stride, fw, fh, line_x + c * term->font_w, line_y, cell->chars, cell_fg, term->font_h, 0);
        }
    }
    if (tab->cursor_visible) {
        int cur_x = term->x + 4 + tab->cursor_col * term->font_w;
        int cur_y = content_y + (tab->cursor_row - start_line) * term->font_h;
        if (cur_y >= content_y && cur_y < content_y + max_lines * term->font_h)
            t_fill_rect(fb, stride, fw, fh, cur_x, cur_y, 2, term->font_h, accent);
    }
    if (term->show_scrollbar) {
        int sb_x = term->x + term->w - KTERM_SCROLLBAR_W;
        t_fill_rect(fb, stride, fw, fh, sb_x, content_y, KTERM_SCROLLBAR_W, term->h - KTERM_TAB_BAR_H, 0x2A2A2AFF);
        if (tab->line_count > max_lines) {
            int thumb_h = (int)((float)max_lines / (float)tab->line_count * (term->h - KTERM_TAB_BAR_H));
            int thumb_y = content_y + (int)((float)tab->scroll_offset / (float)(tab->line_count - max_lines) * (term->h - KTERM_TAB_BAR_H - thumb_h));
            t_fill_rect(fb, stride, fw, fh, sb_x, thumb_y, KTERM_SCROLLBAR_W, thumb_h, 0x606060FF);
        }
    }
}

void kanvas_terminal_update(kanvas_terminal_t* term, uint64_t now_ms) { (void)term; (void)now_ms; }
void kanvas_terminal_handle_key(kanvas_terminal_t* term, int key, bool down, uint32_t mods) { (void)term; (void)key; (void)down; (void)mods; }
void kanvas_terminal_handle_char(kanvas_terminal_t* term, uint32_t ch) { (void)term; (void)ch; }
void kanvas_terminal_handle_mouse(kanvas_terminal_t* term, int mx, int my, bool left, bool right) { (void)term; (void)mx; (void)my; (void)left; (void)right; }

void kanvas_terminal_write(kanvas_terminal_t* term, const char* data, int len)
{
    if (!term || !data) return;
    kterm_tab_t* tab = &term->tabs[term->active_tab];
    for (int i = 0; i < len; i++) {
        if (data[i] == '\n') {
            tab->cursor_row++;
            tab->cursor_col = 0;
            if (tab->cursor_row >= KTERM_MAX_LINES) tab->cursor_row = KTERM_MAX_LINES - 1;
            if (tab->line_count < KTERM_MAX_LINES) tab->line_count++;
        } else if (data[i] == '\r') {
            tab->cursor_col = 0;
        } else {
            if (tab->line_count == 0) tab->line_count = 1;
            if (tab->cursor_row < KTERM_MAX_LINES && tab->cursor_col < KTERM_MAX_COLS) {
                kterm_cell_t* cell = &tab->lines[tab->cursor_row].cells[tab->cursor_col];
                cell->chars[0] = data[i]; cell->chars[1] = '\0';
                cell->fg = (uint8_t)tab->default_fg;
                cell->bg = (uint8_t)tab->default_bg;
                if (tab->lines[tab->cursor_row].length <= tab->cursor_col)
                    tab->lines[tab->cursor_row].length = tab->cursor_col + 1;
                tab->cursor_col++;
            }
        }
    }
}

void kanvas_terminal_new_tab(kanvas_terminal_t* term)
{
    if (!term || term->tab_count >= 8) return;
    int idx = term->tab_count++;
    memset(&term->tabs[idx], 0, sizeof(kterm_tab_t));
    memcpy(term->tabs[idx].cwd, "/home", 5);
    memcpy(term->tabs[idx].title, "Terminal", 8);
    term->tabs[idx].cursor_visible = true;
    term->tabs[idx].default_fg = KTERM_COLOR_WHITE;
    term->tabs[idx].default_bg = KTERM_COLOR_BLACK;
    term->active_tab = idx;
}

void kanvas_terminal_close_tab(kanvas_terminal_t* term, int idx)
{
    if (!term || idx < 0 || idx >= term->tab_count || term->tab_count <= 1) return;
    for (int i = idx; i < term->tab_count - 1; i++) term->tabs[i] = term->tabs[i + 1];
    term->tab_count--;
    if (term->active_tab >= term->tab_count) term->active_tab = term->tab_count - 1;
}

void kanvas_terminal_resize(kanvas_terminal_t* term, int w, int h)
{
    if (!term) return;
    term->w = w; term->h = h;
    term->cols = (w - KTERM_SCROLLBAR_W) / term->font_w;
    term->rows = (h - KTERM_TAB_BAR_H) / term->font_h;
}