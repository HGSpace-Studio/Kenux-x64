#include "kanvas_start_menu.h"
#include "kapi.h"
#include <string.h>

static uint32_t kui_col32_inline(kui_color_t c)
{
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

static void fill_rect_fb(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fw) w = fw - x;
    if (y + h > fh) h = fh - y;
    if (w <= 0 || h <= 0) return;
    for (int row = y; row < y + h; row++) {
        uint32_t* p = (uint32_t*)((uint8_t*)fb + row * stride);
        for (int col = x; col < x + w; col++) p[col] = color;
    }
}

kanvas_start_menu_t* kanvas_start_menu_create(int x, int y)
{
    kanvas_start_menu_t* sm = (kanvas_start_menu_t*)kapi_kmalloc(sizeof(kanvas_start_menu_t));
    if (!sm) return NULL;
    memset(sm, 0, sizeof(kanvas_start_menu_t));
    sm->visible = false;
    sm->x = x; sm->y = y;
    sm->bg_color = (kui_color_t){40, 40, 40, 245};
    sm->fg_color = (kui_color_t){230, 230, 230, 255};
    sm->hover_color = (kui_color_t){60, 60, 60, 255};
    sm->accent_color = (kui_color_t){98, 0, 238, 255};
    sm->search_bg = (kui_color_t){50, 50, 50, 255};
    sm->search_fg = (kui_color_t){230, 230, 230, 255};
    sm->divider_color = (kui_color_t){70, 70, 70, 255};
    sm->power_off_color = (kui_color_t){224, 64, 64, 255};
    sm->power_restart_color = (kui_color_t){255, 180, 0, 255};
    sm->power_sleep_color = (kui_color_t){98, 0, 238, 255};
    sm->search_text[0] = '\0';
    sm->cursor_pos = 0;
    sm->all_count = 0;
    sm->filtered_count = 0;
    sm->selected_index = 0;
    sm->scroll_offset = 0;
    sm->max_visible = (KANVAS_START_H - KANVAS_START_SEARCH_H - 60) / KANVAS_START_ITEM_H;
    sm->search_focused = false;
    return sm;
}

void kanvas_start_menu_destroy(kanvas_start_menu_t* sm)
{
    if (!sm) return;
    kapi_kfree(sm);
}

void kanvas_start_menu_paint(kanvas_start_menu_t* sm, uint32_t* fb, int stride, int fw, int fh)
{
    if (!sm || !sm->visible || !fb) return;
    uint32_t bg = kui_col32_inline(sm->bg_color);
    uint32_t fg = kui_col32_inline(sm->fg_color);
    uint32_t hover = kui_col32_inline(sm->hover_color);
    uint32_t accent = kui_col32_inline(sm->accent_color);
    uint32_t search_bg = kui_col32_inline(sm->search_bg);
    uint32_t search_fg = kui_col32_inline(sm->search_fg);
    uint32_t divider = kui_col32_inline(sm->divider_color);
    kui_draw_rounded_rect(fb, stride, fw, fh, sm->x, sm->y, KANVAS_START_W, KANVAS_START_H, KANVAS_START_RADIUS, bg);
    int search_x = sm->x + KANVAS_START_PAD;
    int search_y = sm->y + KANVAS_START_PAD;
    int search_w = KANVAS_START_W - KANVAS_START_PAD * 2;
    kui_draw_rounded_rect(fb, stride, fw, fh, search_x, search_y, search_w, KANVAS_START_SEARCH_H, 12, search_bg);
    if (sm->search_text[0]) {
        kui_draw_text(fb, stride, fw, fh, search_x + 12, search_y + (KANVAS_START_SEARCH_H - 14) / 2, sm->search_text, search_fg, 14, 0);
    } else {
        kui_draw_text(fb, stride, fw, fh, search_x + 12, search_y + (KANVAS_START_SEARCH_H - 14) / 2, "Search apps...", 0x808080FF, 14, 0);
    }
    if (sm->search_focused) {
        int cursor_x = search_x + 12 + (int)(sm->cursor_pos * 8);
        int cursor_y = search_y + 6;
        fill_rect_fb(fb, stride, fw, fh, cursor_x, cursor_y, 2, KANVAS_START_SEARCH_H - 12, accent);
    }
    int list_y = search_y + KANVAS_START_SEARCH_H + KANVAS_START_PAD;
    fill_rect_fb(fb, stride, fw, fh, sm->x + KANVAS_START_PAD, list_y - 4, KANVAS_START_W - KANVAS_START_PAD * 2, 1, divider);
    int visible_start = sm->scroll_offset;
    int visible_end = visible_start + sm->max_visible;
    if (visible_end > sm->filtered_count) visible_end = sm->filtered_count;
    for (int i = visible_start; i < visible_end; i++) {
        kanvas_start_item_t* item = sm->filtered[i];
        int item_y = list_y + (i - visible_start) * KANVAS_START_ITEM_H;
        if (i == sm->selected_index) {
            kui_draw_rounded_rect(fb, stride, fw, fh, sm->x + KANVAS_START_PAD, item_y, KANVAS_START_W - KANVAS_START_PAD * 2, KANVAS_START_ITEM_H, 6, hover);
        }
        kui_draw_icon(fb, stride, fw, fh, sm->x + KANVAS_START_PAD + 8, item_y + (KANVAS_START_ITEM_H - KANVAS_START_ICON_SZ) / 2, item->icon_id, KANVAS_START_ICON_SZ, fg);
        kui_draw_text(fb, stride, fw, fh, sm->x + KANVAS_START_PAD + 8 + KANVAS_START_ICON_SZ + 8, item_y + (KANVAS_START_ITEM_H - 14) / 2, item->name, fg, 14, 0);
    }
    int power_y = sm->y + KANVAS_START_H - 48;
    fill_rect_fb(fb, stride, fw, fh, sm->x + KANVAS_START_PAD, power_y - 4, KANVAS_START_W - KANVAS_START_PAD * 2, 1, divider);
    uint32_t power_off = kui_col32_inline(sm->power_off_color);
    uint32_t power_restart = kui_col32_inline(sm->power_restart_color);
    uint32_t power_sleep = kui_col32_inline(sm->power_sleep_color);
    int btn_w = 80, btn_h = 32, btn_r = 10;
    int btn_y = power_y + 4;
    int btn_x = sm->x + KANVAS_START_PAD;
    kui_draw_rounded_rect(fb, stride, fw, fh, btn_x, btn_y, btn_w, btn_h, btn_r, power_sleep);
    kui_draw_text(fb, stride, fw, fh, btn_x + 16, btn_y + 8, "Sleep", 0xFFFFFFFF, 12, 0);
    btn_x += btn_w + 8;
    kui_draw_rounded_rect(fb, stride, fw, fh, btn_x, btn_y, btn_w, btn_h, btn_r, power_restart);
    kui_draw_text(fb, stride, fw, fh, btn_x + 10, btn_y + 8, "Restart", 0xFFFFFFFF, 12, 0);
    btn_x += btn_w + 8;
    kui_draw_rounded_rect(fb, stride, fw, fh, btn_x, btn_y, btn_w, btn_h, btn_r, power_off);
    kui_draw_text(fb, stride, fw, fh, btn_x + 18, btn_y + 8, "Off", 0xFFFFFFFF, 12, 0);
}

void kanvas_start_menu_handle_mouse(kanvas_start_menu_t* sm, int mx, int my, bool left_down, bool left_up)
{
    if (!sm || !sm->visible) return;
    (void)left_up;
    int search_x = sm->x + KANVAS_START_PAD;
    int search_y = sm->y + KANVAS_START_PAD;
    int search_w = KANVAS_START_W - KANVAS_START_PAD * 2;
    if (mx >= search_x && mx < search_x + search_w && my >= search_y && my < search_y + KANVAS_START_SEARCH_H) {
        sm->search_focused = true;
        if (left_down) {
            sm->cursor_pos = (mx - search_x - 12) / 8;
            if (sm->cursor_pos < 0) sm->cursor_pos = 0;
            int max_pos = (int)kapi_strlen(sm->search_text);
            if (sm->cursor_pos > max_pos) sm->cursor_pos = max_pos;
        }
        return;
    }
    int list_y = search_y + KANVAS_START_SEARCH_H + KANVAS_START_PAD;
    if (my >= list_y && my < list_y + sm->max_visible * KANVAS_START_ITEM_H) {
        int idx = (my - list_y) / KANVAS_START_ITEM_H + sm->scroll_offset;
        if (idx >= 0 && idx < sm->filtered_count) {
            sm->selected_index = idx;
            if (left_down && sm->filtered[idx]) {
                sm->visible = false;
            }
        }
    }
    int power_y = sm->y + KANVAS_START_H - 48;
    int btn_y = power_y + 4;
    int btn_w = 80, btn_h = 32;
    int btn_x = sm->x + KANVAS_START_PAD;
    if (left_down && my >= btn_y && my < btn_y + btn_h) {
        if (mx >= btn_x && mx < btn_x + btn_w) {
            sm->visible = false;
        } else if (mx >= btn_x + btn_w + 8 && mx < btn_x + btn_w * 2 + 8) {
            sm->visible = false;
        } else if (mx >= btn_x + btn_w * 2 + 16 && mx < btn_x + btn_w * 3 + 16) {
            sm->visible = false;
        }
    }
}

void kanvas_start_menu_handle_key(kanvas_start_menu_t* sm, int key, bool down, uint32_t mods)
{
    if (!sm || !sm->visible || !sm->search_focused || !down) return;
    (void)mods;
    if (key == 8) {
        if (sm->cursor_pos > 0) {
            for (int i = sm->cursor_pos - 1; i < 127; i++) sm->search_text[i] = sm->search_text[i + 1];
            sm->cursor_pos--;
            kanvas_start_menu_filter(sm);
        }
    } else if (key == 13) {
        if (sm->selected_index >= 0 && sm->selected_index < sm->filtered_count && sm->filtered[sm->selected_index]) {
            sm->visible = false;
        }
    } else if (key == 27) {
        sm->visible = false;
    } else if (key == 38) {
        if (sm->selected_index > 0) sm->selected_index--;
    } else if (key == 40) {
        if (sm->selected_index < sm->filtered_count - 1) sm->selected_index++;
    }
}

void kanvas_start_menu_handle_char(kanvas_start_menu_t* sm, uint32_t ch)
{
    if (!sm || !sm->visible || !sm->search_focused) return;
    if (ch < 32 || ch > 126) return;
    if (sm->cursor_pos >= 127) return;
    for (int i = 127; i > sm->cursor_pos; i--) sm->search_text[i] = sm->search_text[i - 1];
    sm->search_text[sm->cursor_pos] = (char)ch;
    sm->cursor_pos++;
    sm->search_text[127] = '\0';
    kanvas_start_menu_filter(sm);
}

int kanvas_start_menu_add_item(kanvas_start_menu_t* sm, const char* name, const char* exec,
                                int icon_id, kanvas_start_section_t section, void* app_data)
{
    if (!sm || sm->all_count >= KANVAS_START_MAX_ALL) return -1;
    int idx = sm->all_count++;
    kanvas_start_item_t* item = &sm->all_items[idx];
    memset(item, 0, sizeof(kanvas_start_item_t));
    if (name) {
        size_t len = kapi_strlen(name);
        if (len >= 64) len = 63;
        memcpy(item->name, name, len);
    }
    if (exec) {
        size_t len = kapi_strlen(exec);
        if (len >= 128) len = 127;
        memcpy(item->exec, exec, len);
    }
    item->icon_id = icon_id;
    item->section = section;
    item->app_data = app_data;
    return idx;
}

void kanvas_start_menu_filter(kanvas_start_menu_t* sm)
{
    if (!sm) return;
    sm->filtered_count = 0;
    sm->selected_index = 0;
    sm->scroll_offset = 0;
    bool has_search = sm->search_text[0] != '\0';
    for (int i = 0; i < sm->all_count && sm->filtered_count < KANVAS_START_MAX_RESULTS; i++) {
        kanvas_start_item_t* item = &sm->all_items[i];
        if (has_search) {
            bool match = false;
            size_t name_len = kapi_strlen(item->name);
            size_t search_len = kapi_strlen(sm->search_text);
            for (size_t j = 0; j + search_len <= name_len; j++) {
                bool sub_match = true;
                for (size_t k = 0; k < search_len; k++) {
                    char nc = item->name[j + k];
                    char sc = sm->search_text[k];
                    if (nc >= 'A' && nc <= 'Z') nc = (char)(nc + 32);
                    if (sc >= 'A' && sc <= 'Z') sc = (char)(sc + 32);
                    if (nc != sc) { sub_match = false; break; }
                }
                if (sub_match) { match = true; break; }
            }
            if (!match) continue;
        }
        sm->filtered[sm->filtered_count++] = item;
    }
}

void kanvas_start_menu_show(kanvas_start_menu_t* sm)
{
    if (!sm) return;
    sm->visible = true;
    sm->search_focused = true;
    sm->search_text[0] = '\0';
    sm->cursor_pos = 0;
    sm->selected_index = 0;
    sm->scroll_offset = 0;
    kanvas_start_menu_filter(sm);
}

void kanvas_start_menu_hide(kanvas_start_menu_t* sm)
{
    if (!sm) return;
    sm->visible = false;
    sm->search_focused = false;
}