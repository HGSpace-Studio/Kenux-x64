#include "kanvas_desktop.h"
#include "kanvas_window.h"
#include "kanvas_taskbar.h"
#include "kanvas_start_menu.h"
#include "kapi.h"
#include <string.h>

static uint32_t kui_col32_inline(kui_color_t c)
{
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

static void fill_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t color)
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

static void fill_rounded_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, int r, uint32_t color)
{
    if (r <= 0) { fill_rect(fb, stride, fw, fh, x, y, w, h, color); return; }
    fill_rect(fb, stride, fw, fh, x + r, y, w - 2 * r, h, color);
    fill_rect(fb, stride, fw, fh, x, y + r, r, h - 2 * r, color);
    fill_rect(fb, stride, fw, fh, x + w - r, y + r, r, h - 2 * r, color);
    for (int dy = 0; dy < r; dy++) {
        int dx = (int)kapi_sqrtf((float)(r * r - dy * dy));
        int cx1 = x + r - dx, cx2 = x + w - r + dx;
        int ry1 = y + r - dy - 1, ry2 = y - r + h + dy;
        fill_rect(fb, stride, fw, fh, cx1, ry1, cx2 - cx1, 1, color);
        fill_rect(fb, stride, fw, fh, cx1, ry2, cx2 - cx1, 1, color);
    }
}

static void paint_wallpaper(kanvas_desktop_t* desk, uint32_t* fb, int stride, int fw, int fh)
{
    if (desk->wallpaper_data && desk->wallpaper_w > 0 && desk->wallpaper_h > 0) {
        for (int y = 0; y < fh; y++) {
            int sy = y % desk->wallpaper_h;
            uint32_t* src_row = desk->wallpaper_data + sy * desk->wallpaper_w;
            uint32_t* dst_row = (uint32_t*)((uint8_t*)fb + y * stride);
            for (int x = 0; x < fw; x++) {
                int sx = x % desk->wallpaper_w;
                dst_row[x] = src_row[sx];
            }
        }
    } else {
        fill_rect(fb, stride, fw, fh, 0, 0, fw, fh, desk->wallpaper_color);
    }
}

static void paint_desktop_icons(kanvas_desktop_t* desk, uint32_t* fb, int stride, int fw, int fh)
{
    uint32_t fg = kui_col32_inline(desk->theme.desktop_fg);
    for (int i = 0; i < desk->icon_count; i++) {
        kanvas_desktop_icon_t* icon = &desk->icons[i];
        int ix = icon->x, iy = icon->y;
        int icon_sz = 48;
        int icon_cx = ix + icon_sz / 2;
        int icon_cy = iy + icon_sz / 2;
        kui_draw_icon(fb, stride, fw, fh, icon_cx - 16, icon_cy - 16, icon->icon_id, 32, fg);
        int text_y = iy + icon_sz + 2;
        kui_draw_text(fb, stride, fw, fh, ix, text_y, icon->name, fg, 12, 0);
    }
}

static void paint_taskbar(kanvas_desktop_t* desk, uint32_t* fb, int stride, int fw, int fh)
{
    kanvas_taskbar_t* tb = &desk->taskbar;
    if (!tb->visible) return;
    uint32_t bg = kui_col32_inline(desk->theme.taskbar_bg);
    uint32_t fg = kui_col32_inline(desk->theme.taskbar_fg);
    uint32_t accent = kui_col32_inline(desk->theme.taskbar_accent);
    int tb_y = fh - KANVAS_TASKBAR_HEIGHT;
    fill_rect(fb, stride, fw, fh, 0, tb_y, fw, KANVAS_TASKBAR_HEIGHT, bg);
    int start_x = KANVAS_TASKBAR_PADDING;
    int start_y = tb_y + (KANVAS_TASKBAR_HEIGHT - 36) / 2;
    uint32_t start_col = tb->start_hovered ? accent : fg;
    fill_rounded_rect(fb, stride, fw, fh, start_x, start_y, 36, 36, 8, start_col);
    int item_x = start_x + 36 + KANVAS_TASKBAR_PADDING * 2;
    for (int i = 0; i < tb->item_count; i++) {
        kanvas_taskbar_item_t* item = &tb->items[i];
        int item_y = tb_y + (KANVAS_TASKBAR_HEIGHT - KANVAS_TASKBAR_ITEM_H) / 2;
        uint32_t item_bg = item->hovered ? kui_col32_inline(desk->theme.taskbar_accent) : 0;
        if (item_bg) fill_rounded_rect(fb, stride, fw, fh, item_x, item_y, KANVAS_TASKBAR_ITEM_W, KANVAS_TASKBAR_ITEM_H, 6, item_bg);
        if (item->active) {
            int indicator_y = tb_y + KANVAS_TASKBAR_HEIGHT - 3;
            fill_rect(fb, stride, fw, fh, item_x + 8, indicator_y, KANVAS_TASKBAR_ITEM_W - 16, 2, accent);
        }
        kui_draw_icon(fb, stride, fw, fh, item_x + (KANVAS_TASKBAR_ITEM_W - 24) / 2, item_y + (KANVAS_TASKBAR_ITEM_H - 24) / 2, item->icon_id, 24, fg);
        item_x += KANVAS_TASKBAR_ITEM_W + KANVAS_TASKBAR_PADDING;
    }
    int tray_x = fw - KANVAS_TASKBAR_PADDING;
    for (int i = tb->tray_count - 1; i >= 0; i--) {
        kanvas_tray_icon_t* tray = &tb->tray[i];
        tray_x -= 24;
        int tray_y = tb_y + (KANVAS_TASKBAR_HEIGHT - 24) / 2;
        kui_draw_icon(fb, stride, fw, fh, tray_x, tray_y, tray->icon_id, 20, fg);
        if (tray->has_notification) {
            fill_rect(fb, stride, fw, fh, tray_x + 16, tray_y, 8, 8, 0xFF4444FF);
        }
        tray_x -= KANVAS_TASKBAR_PADDING;
    }
    if (tb->clock_text[0]) {
        int clock_w = (int)(kapi_strlen(tb->clock_text) * 8);
        kui_draw_text(fb, stride, fw, fh, fw - clock_w - 16, tb_y + (KANVAS_TASKBAR_HEIGHT - 14) / 2, tb->clock_text, fg, 13, 0);
    }
}

static void paint_context_menu(kanvas_desktop_t* desk, uint32_t* fb, int stride, int fw, int fh)
{
    kanvas_context_menu_t* cm = &desk->context_menu;
    if (!cm->visible) return;
    uint32_t bg = kui_col32_inline(desk->theme.context_menu_bg);
    uint32_t fg = kui_col32_inline(desk->theme.context_menu_fg);
    uint32_t hover = kui_col32_inline(desk->theme.context_menu_hover);
    int r = desk->theme.corner_radius;
    fill_rounded_rect(fb, stride, fw, fh, cm->x, cm->y, cm->width, cm->height, r, bg);
    int y = cm->y + 4;
    for (int i = 0; i < cm->item_count; i++) {
        kanvas_menu_entry_t* item = &cm->items[i];
        int item_h = 28;
        if (item->separator) {
            fill_rect(fb, stride, fw, fh, cm->x + 8, y + item_h / 2, cm->width - 16, 1, 0x404040FF);
            y += item_h;
            continue;
        }
        if (i == cm->hovered_index)
            fill_rounded_rect(fb, stride, fw, fh, cm->x + 4, y, cm->width - 8, item_h, 4, hover);
        if (item->enabled)
            kui_draw_text(fb, stride, fw, fh, cm->x + 12, y + (item_h - 14) / 2, item->label, fg, 14, 0);
        else
            kui_draw_text(fb, stride, fw, fh, cm->x + 12, y + (item_h - 14) / 2, item->label, 0x808080FF, 14, 0);
        y += item_h;
    }
}

kanvas_desktop_t* kanvas_desktop_create(int screen_w, int screen_h, uint32_t* fb, int stride)
{
    kanvas_desktop_t* desk = (kanvas_desktop_t*)kapi_kmalloc(sizeof(kanvas_desktop_t));
    if (!desk) return NULL;
    memset(desk, 0, sizeof(kanvas_desktop_t));
    desk->ui = NULL;
    desk->state = KANVAS_DESKTOP_STATE_NORMAL;
    desk->screen_width = screen_w;
    desk->screen_height = screen_h;
    desk->wallpaper_color = 0x1A1A2EFF;
    desk->wallpaper_data = NULL;
    desk->needs_repaint = true;
    desk->taskbar.visible = true;
    desk->taskbar.width = screen_w;
    desk->taskbar.x = 0;
    desk->taskbar.y = screen_h - KANVAS_TASKBAR_HEIGHT;
    desk->taskbar.hovered_item = -1;
    desk->taskbar.hovered_tray = -1;
    desk->taskbar.start_hovered = false;
    desk->start_menu.visible = false;
    desk->start_menu.x = 0;
    desk->start_menu.y = screen_h - KANVAS_TASKBAR_HEIGHT - KANVAS_START_H;
    desk->context_menu.visible = false;
    desk->context_menu.hovered_index = -1;
    desk->drag_window_idx = -1;
    desk->resize_edge = 0;
    kanvas_desktop_apply_theme_md3_dark(desk);
    return desk;
}

void kanvas_desktop_destroy(kanvas_desktop_t* desk)
{
    if (!desk) return;
    for (int i = 0; i < desk->window_count; i++) {
        kanvas_window_destroy((kanvas_window_t*)desk->windows[i]);
    }
    if (desk->wallpaper_data) kapi_kfree(desk->wallpaper_data);
    kapi_kfree(desk);
}

void kanvas_desktop_paint(kanvas_desktop_t* desk)
{
    if (!desk || !desk->needs_repaint) return;
    uint32_t* fb = NULL;
    int stride = desk->screen_width * 4;
    int fw = desk->screen_width, fh = desk->screen_height;
    if (!fb) return;
    paint_wallpaper(desk, fb, stride, fw, fh);
    paint_desktop_icons(desk, fb, stride, fw, fh);
    for (int i = 0; i < desk->window_count; i++) {
        kanvas_window_t* win = (kanvas_window_t*)desk->windows[i];
        if (win && win->visible && !win->minimized)
            kanvas_window_paint(win, fb, stride, fw, fh);
    }
    paint_taskbar(desk, fb, stride, fw, fh);
    if (desk->start_menu.visible) {
        kanvas_start_menu_paint(&desk->start_menu, fb, stride, fw, fh);
    }
    paint_context_menu(desk, fb, stride, fw, fh);
    desk->needs_repaint = false;
}

void kanvas_desktop_handle_mouse(kanvas_desktop_t* desk, int x, int y, bool left, bool right, bool mid)
{
    if (!desk) return;
    desk->mouse_x = x; desk->mouse_y = y;
    if (desk->start_menu.visible) {
        if (x >= desk->start_menu.x && x < desk->start_menu.x + KANVAS_START_W &&
            y >= desk->start_menu.y && y < desk->start_menu.y + KANVAS_START_H) {
            kanvas_start_menu_handle_mouse(&desk->start_menu, x, y, left, false);
            return;
        } else if (left) {
            kanvas_desktop_close_start_menu(desk);
        }
    }
    if (desk->context_menu.visible) {
        if (x >= desk->context_menu.x && x < desk->context_menu.x + desk->context_menu.width &&
            y >= desk->context_menu.y && y < desk->context_menu.y + desk->context_menu.height) {
            if (left) {
                int idx = (y - desk->context_menu.y - 4) / 28;
                if (idx >= 0 && idx < desk->context_menu.item_count && desk->context_menu.items[idx].callback) {
                    desk->context_menu.items[idx].callback();
                    kanvas_desktop_close_context_menu(desk);
                }
            } else {
                desk->context_menu.hovered_index = (y - desk->context_menu.y - 4) / 28;
            }
            return;
        } else if (left) {
            kanvas_desktop_close_context_menu(desk);
        }
    }
    int tb_y = desk->screen_height - KANVAS_TASKBAR_HEIGHT;
    if (y >= tb_y && desk->taskbar.visible) {
        if (x >= KANVAS_TASKBAR_PADDING && x < KANVAS_TASKBAR_PADDING + 36) {
            desk->taskbar.start_hovered = true;
            if (left) kanvas_desktop_toggle_start_menu(desk);
        } else {
            desk->taskbar.start_hovered = false;
        }
        return;
    }
    if (right && y < tb_y) {
        kanvas_desktop_open_context_menu(desk, x, y);
        return;
    }
    for (int i = desk->window_count - 1; i >= 0; i--) {
        kanvas_window_t* win = (kanvas_window_t*)desk->windows[i];
        if (!win || !win->visible || win->minimized) continue;
        if (kanvas_window_hit_test(win, x, y)) {
            kanvas_win_ctrl_t ctrl = kanvas_window_hit_ctrl(win, x, y);
            if (left && ctrl != KANVAS_WIN_CTRL_NONE) {
                switch (ctrl) {
                case KANVAS_WIN_CTRL_CLOSE: kanvas_window_close(win); break;
                case KANVAS_WIN_CTRL_MAXIMIZE: kanvas_window_maximize(win); break;
                case KANVAS_WIN_CTRL_MINIMIZE: kanvas_window_minimize(win); break;
                default: break;
                }
                desk->needs_repaint = true;
                return;
            }
            if (left && kanvas_window_titlebar_hit(win, x, y)) {
                desk->drag_window_idx = i;
                desk->drag_offset_x = x - win->x;
                desk->drag_offset_y = y - win->y;
                desk->state = KANVAS_DESKTOP_STATE_DRAGGING;
                kanvas_desktop_focus_window(desk, (kui_window_t*)win);
                return;
            }
            kanvas_resize_edge_t edge = kanvas_window_hit_resize(win, x, y);
            if (left && edge != KANVAS_RESIZE_NONE) {
                desk->resize_edge = (int)edge;
                desk->state = KANVAS_DESKTOP_STATE_RESIZING;
                return;
            }
            kanvas_desktop_focus_window(desk, (kui_window_t*)win);
            if (win->on_event) win->on_event(win, KUI_MSG_LBUTTON_DOWN, (uint64_t)x, (uint64_t)y);
            return;
        }
    }
    if (left) {
        desk->active_window = NULL;
        desk->needs_repaint = true;
    }
}

void kanvas_desktop_handle_key(kanvas_desktop_t* desk, int key, bool down, uint32_t mods)
{
    if (!desk) return;
    if (desk->start_menu.visible && desk->start_menu.search_focused) {
        kanvas_start_menu_handle_key(&desk->start_menu, key, down, mods);
        desk->needs_repaint = true;
        return;
    }
    if (desk->active_window) {
        kanvas_window_t* win = (kanvas_window_t*)desk->active_window;
        if (win && win->on_event) win->on_event(win, down ? KUI_MSG_KEY_DOWN : KUI_MSG_KEY_UP, (uint64_t)key, (uint64_t)mods);
    }
}

void kanvas_desktop_handle_scroll(kanvas_desktop_t* desk, int delta)
{
    if (!desk) return;
    (void)delta;
}

void kanvas_desktop_add_icon(kanvas_desktop_t* desk, const char* name, int icon_id, int x, int y)
{
    if (!desk || desk->icon_count >= KANVAS_DESKTOP_MAX_ICONS) return;
    kanvas_desktop_icon_t* icon = &desk->icons[desk->icon_count++];
    memset(icon, 0, sizeof(kanvas_desktop_icon_t));
    if (name) {
        size_t len = kapi_strlen(name);
        if (len >= 64) len = 63;
        memcpy(icon->name, name, len);
    }
    icon->icon_id = icon_id;
    icon->x = x; icon->y = y;
    desk->needs_repaint = true;
}

void kanvas_desktop_remove_icon(kanvas_desktop_t* desk, int index)
{
    if (!desk || index < 0 || index >= desk->icon_count) return;
    desk->icons[index] = desk->icons[--desk->icon_count];
    desk->needs_repaint = true;
}

void kanvas_desktop_open_start_menu(kanvas_desktop_t* desk)
{
    if (!desk) return;
    desk->start_menu.visible = true;
    desk->start_menu.search_focused = true;
    desk->start_menu.search_text[0] = '\0';
    desk->start_menu.cursor_pos = 0;
    desk->start_menu.selected_index = 0;
    kanvas_start_menu_filter(&desk->start_menu);
    desk->state = KANVAS_DESKTOP_STATE_START_OPEN;
    desk->needs_repaint = true;
}

void kanvas_desktop_close_start_menu(kanvas_desktop_t* desk)
{
    if (!desk) return;
    desk->start_menu.visible = false;
    desk->start_menu.search_focused = false;
    if (desk->state == KANVAS_DESKTOP_STATE_START_OPEN)
        desk->state = KANVAS_DESKTOP_STATE_NORMAL;
    desk->needs_repaint = true;
}

void kanvas_desktop_toggle_start_menu(kanvas_desktop_t* desk)
{
    if (!desk) return;
    if (desk->start_menu.visible) kanvas_desktop_close_start_menu(desk);
    else kanvas_desktop_open_start_menu(desk);
}

void kanvas_desktop_open_context_menu(kanvas_desktop_t* desk, int x, int y)
{
    if (!desk) return;
    kanvas_context_menu_t* cm = &desk->context_menu;
    cm->visible = true;
    cm->x = x; cm->y = y;
    cm->width = 200; cm->height = 0;
    cm->item_count = 0;
    cm->hovered_index = -1;
    kanvas_menu_entry_t* items = cm->items;
    items[0] = (kanvas_menu_entry_t){"New Folder", 0, NULL, false, true}; cm->item_count++;
    items[1] = (kanvas_menu_entry_t){"New File", 0, NULL, false, true}; cm->item_count++;
    items[2] = (kanvas_menu_entry_t){"", 0, NULL, true, true}; cm->item_count++;
    items[3] = (kanvas_menu_entry_t){"Refresh", 0, NULL, false, true}; cm->item_count++;
    items[4] = (kanvas_menu_entry_t){"Display Settings", 0, NULL, false, true}; cm->item_count++;
    items[5] = (kanvas_menu_entry_t){"", 0, NULL, true, true}; cm->item_count++;
    items[6] = (kanvas_menu_entry_t){"About Kenux", 0, NULL, false, true}; cm->item_count++;
    cm->height = cm->item_count * 28 + 8;
    if (cm->x + cm->width > desk->screen_width) cm->x = desk->screen_width - cm->width;
    if (cm->y + cm->height > desk->screen_height - KANVAS_TASKBAR_HEIGHT)
        cm->y = desk->screen_height - KANVAS_TASKBAR_HEIGHT - cm->height;
    desk->state = KANVAS_DESKTOP_STATE_CONTEXT_MENU;
    desk->needs_repaint = true;
}

void kanvas_desktop_close_context_menu(kanvas_desktop_t* desk)
{
    if (!desk) return;
    desk->context_menu.visible = false;
    desk->context_menu.hovered_index = -1;
    if (desk->state == KANVAS_DESKTOP_STATE_CONTEXT_MENU)
        desk->state = KANVAS_DESKTOP_STATE_NORMAL;
    desk->needs_repaint = true;
}

void kanvas_desktop_launch_app(kanvas_desktop_t* desk, kanvas_app_entry_t* app)
{
    if (!desk || !app) return;
    (void)desk;
    app->running = true;
}

void kanvas_desktop_close_app(kanvas_desktop_t* desk, kui_window_t* win)
{
    if (!desk || !win) return;
    for (int i = 0; i < desk->window_count; i++) {
        if (desk->windows[i] == win) {
            desk->windows[i] = desk->windows[--desk->window_count];
            if (desk->active_window == win) desk->active_window = NULL;
            break;
        }
    }
    desk->needs_repaint = true;
}

void kanvas_desktop_focus_window(kanvas_desktop_t* desk, kui_window_t* win)
{
    if (!desk) return;
    desk->active_window = win;
    for (int i = 0; i < desk->window_count; i++) {
        kanvas_window_t* w = (kanvas_window_t*)desk->windows[i];
        if (w) w->focused = ((kui_window_t*)w == win);
    }
    desk->needs_repaint = true;
}

void kanvas_desktop_set_wallpaper_color(kanvas_desktop_t* desk, uint32_t color)
{
    if (!desk) return;
    desk->wallpaper_color = color;
    if (desk->wallpaper_data) { kapi_kfree(desk->wallpaper_data); desk->wallpaper_data = NULL; }
    desk->needs_repaint = true;
}

void kanvas_desktop_set_wallpaper_image(kanvas_desktop_t* desk, uint32_t* data, int w, int h)
{
    if (!desk || !data) return;
    if (desk->wallpaper_data) kapi_kfree(desk->wallpaper_data);
    size_t sz = (size_t)w * h * 4;
    desk->wallpaper_data = (uint32_t*)kapi_kmalloc(sz);
    if (desk->wallpaper_data) {
        memcpy(desk->wallpaper_data, data, sz);
        desk->wallpaper_w = w; desk->wallpaper_h = h;
    }
    desk->needs_repaint = true;
}

void kanvas_desktop_apply_theme_md3_dark(kanvas_desktop_t* desk)
{
    if (!desk) return;
    kanvas_desktop_theme_t* t = &desk->theme;
    t->desktop_bg = (kui_color_t){26, 26, 46, 255};
    t->desktop_fg = (kui_color_t){230, 230, 230, 255};
    t->taskbar_bg = (kui_color_t){30, 30, 30, 240};
    t->taskbar_fg = (kui_color_t){230, 230, 230, 255};
    t->taskbar_accent = (kui_color_t){98, 0, 238, 255};
    t->titlebar_bg = (kui_color_t){45, 45, 45, 255};
    t->titlebar_fg = (kui_color_t){230, 230, 230, 255};
    t->titlebar_active_bg = (kui_color_t){55, 55, 55, 255};
    t->titlebar_active_fg = (kui_color_t){255, 255, 255, 255};
    t->window_bg = (kui_color_t){30, 30, 30, 255};
    t->window_fg = (kui_color_t){230, 230, 230, 255};
    t->start_menu_bg = (kui_color_t){40, 40, 40, 245};
    t->start_menu_fg = (kui_color_t){230, 230, 230, 255};
    t->start_menu_hover = (kui_color_t){60, 60, 60, 255};
    t->context_menu_bg = (kui_color_t){40, 40, 40, 245};
    t->context_menu_fg = (kui_color_t){230, 230, 230, 255};
    t->context_menu_hover = (kui_color_t){60, 60, 60, 255};
    t->statusbar_bg = (kui_color_t){30, 30, 30, 255};
    t->statusbar_fg = (kui_color_t){180, 180, 180, 255};
    t->accent = (kui_color_t){98, 0, 238, 255};
    t->border = (kui_color_t){60, 60, 60, 255};
    t->shadow = (kui_color_t){0, 0, 0, 80};
    t->selection = (kui_color_t){98, 0, 238, 128};
    t->danger = (kui_color_t){224, 64, 64, 255};
    t->success = (kui_color_t){64, 180, 64, 255};
    t->warning = (kui_color_t){255, 180, 0, 255};
    t->corner_radius = 10;
    t->font_size = 14;
    t->font_size_small = 12;
    t->font_size_large = 18;
    memcpy(t->font_family, "Kenux Sans", 11);
    desk->wallpaper_color = 0x1A1A2EFF;
    desk->needs_repaint = true;
}

void kanvas_desktop_apply_theme_md3_light(kanvas_desktop_t* desk)
{
    if (!desk) return;
    kanvas_desktop_theme_t* t = &desk->theme;
    t->desktop_bg = (kui_color_t){240, 240, 248, 255};
    t->desktop_fg = (kui_color_t){30, 30, 30, 255};
    t->taskbar_bg = (kui_color_t){245, 245, 245, 240};
    t->taskbar_fg = (kui_color_t){30, 30, 30, 255};
    t->taskbar_accent = (kui_color_t){98, 0, 238, 255};
    t->titlebar_bg = (kui_color_t){230, 230, 230, 255};
    t->titlebar_fg = (kui_color_t){30, 30, 30, 255};
    t->titlebar_active_bg = (kui_color_t){240, 240, 240, 255};
    t->titlebar_active_fg = (kui_color_t){0, 0, 0, 255};
    t->window_bg = (kui_color_t){255, 255, 255, 255};
    t->window_fg = (kui_color_t){30, 30, 30, 255};
    t->start_menu_bg = (kui_color_t){250, 250, 250, 245};
    t->start_menu_fg = (kui_color_t){30, 30, 30, 255};
    t->start_menu_hover = (kui_color_t){220, 220, 220, 255};
    t->context_menu_bg = (kui_color_t){250, 250, 250, 245};
    t->context_menu_fg = (kui_color_t){30, 30, 30, 255};
    t->context_menu_hover = (kui_color_t){220, 220, 220, 255};
    t->statusbar_bg = (kui_color_t){245, 245, 245, 255};
    t->statusbar_fg = (kui_color_t){100, 100, 100, 255};
    t->accent = (kui_color_t){98, 0, 238, 255};
    t->border = (kui_color_t){200, 200, 200, 255};
    t->shadow = (kui_color_t){0, 0, 0, 40};
    t->selection = (kui_color_t){98, 0, 238, 64};
    t->danger = (kui_color_t){180, 40, 40, 255};
    t->success = (kui_color_t){40, 140, 40, 255};
    t->warning = (kui_color_t){200, 140, 0, 255};
    t->corner_radius = 10;
    t->font_size = 14;
    t->font_size_small = 12;
    t->font_size_large = 18;
    memcpy(t->font_family, "Kenux Sans", 11);
    desk->wallpaper_color = 0xF0F0F8FF;
    desk->needs_repaint = true;
}

void kanvas_desktop_apply_theme_classic(kanvas_desktop_t* desk)
{
    if (!desk) return;
    kanvas_desktop_theme_t* t = &desk->theme;
    t->desktop_bg = (kui_color_t){0, 0, 128, 255};
    t->desktop_fg = (kui_color_t){255, 255, 255, 255};
    t->taskbar_bg = (kui_color_t){192, 192, 192, 255};
    t->taskbar_fg = (kui_color_t){0, 0, 0, 255};
    t->taskbar_accent = (kui_color_t){0, 0, 128, 255};
    t->titlebar_bg = (kui_color_t){0, 0, 128, 255};
    t->titlebar_fg = (kui_color_t){255, 255, 255, 255};
    t->titlebar_active_bg = (kui_color_t){0, 0, 128, 255};
    t->titlebar_active_fg = (kui_color_t){255, 255, 255, 255};
    t->window_bg = (kui_color_t){192, 192, 192, 255};
    t->window_fg = (kui_color_t){0, 0, 0, 255};
    t->start_menu_bg = (kui_color_t){192, 192, 192, 255};
    t->start_menu_fg = (kui_color_t){0, 0, 0, 255};
    t->start_menu_hover = (kui_color_t){0, 0, 128, 255};
    t->context_menu_bg = (kui_color_t){192, 192, 192, 255};
    t->context_menu_fg = (kui_color_t){0, 0, 0, 255};
    t->context_menu_hover = (kui_color_t){0, 0, 128, 255};
    t->statusbar_bg = (kui_color_t){192, 192, 192, 255};
    t->statusbar_fg = (kui_color_t){0, 0, 0, 255};
    t->accent = (kui_color_t){0, 0, 128, 255};
    t->border = (kui_color_t){128, 128, 128, 255};
    t->shadow = (kui_color_t){0, 0, 0, 60};
    t->selection = (kui_color_t){0, 0, 128, 128};
    t->danger = (kui_color_t){200, 0, 0, 255};
    t->success = (kui_color_t){0, 128, 0, 255};
    t->warning = (kui_color_t){200, 200, 0, 255};
    t->corner_radius = 0;
    t->font_size = 14;
    t->font_size_small = 12;
    t->font_size_large = 18;
    memcpy(t->font_family, "Kenux Sans", 11);
    desk->wallpaper_color = 0x000080FF;
    desk->needs_repaint = true;
}

void kanvas_desktop_tray_add(kanvas_desktop_t* desk, const char* name, void (*on_click)(void))
{
    if (!desk || desk->taskbar.tray_count >= KANVAS_TASKBAR_MAX_TRAY) return;
    kanvas_tray_icon_t* tray = &desk->taskbar.tray[desk->taskbar.tray_count++];
    memset(tray, 0, sizeof(kanvas_tray_icon_t));
    if (name) {
        size_t len = kapi_strlen(name);
        if (len >= 32) len = 31;
        memcpy(tray->name, name, len);
    }
    tray->on_click = on_click;
    desk->needs_repaint = true;
}

void kanvas_desktop_tray_remove(kanvas_desktop_t* desk, int index)
{
    if (!desk || index < 0 || index >= desk->taskbar.tray_count) return;
    desk->taskbar.tray[index] = desk->taskbar.tray[--desk->taskbar.tray_count];
    desk->needs_repaint = true;
}

void kanvas_desktop_update(kanvas_desktop_t* desk, uint64_t now_ms)
{
    if (!desk) return;
    kanvas_taskbar_update_clock(&desk->taskbar, now_ms);
    if (desk->state == KANVAS_DESKTOP_STATE_DRAGGING && desk->drag_window_idx >= 0) {
        kanvas_window_t* win = (kanvas_window_t*)desk->windows[desk->drag_window_idx];
        if (win) {
            win->x = desk->mouse_x - desk->drag_offset_x;
            win->y = desk->mouse_y - desk->drag_offset_y;
            desk->needs_repaint = true;
        }
    }
}

void kanvas_desktop_run(kanvas_desktop_t* desk)
{
    if (!desk) return;
    desk->needs_repaint = true;
    kanvas_desktop_paint(desk);
}