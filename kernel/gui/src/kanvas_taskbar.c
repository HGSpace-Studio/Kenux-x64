#include "kanvas_taskbar.h"
#include "kapi.h"
#include <string.h>

static uint32_t kui_col32_inline(kui_color_t c)
{
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

kanvas_taskbar_t* kanvas_taskbar_create(int x, int y, int width)
{
    kanvas_taskbar_t* tb = (kanvas_taskbar_t*)kapi_kmalloc(sizeof(kanvas_taskbar_t));
    if (!tb) return NULL;
    memset(tb, 0, sizeof(kanvas_taskbar_t));
    tb->x = x; tb->y = y;
    tb->width = width;
    tb->visible = true;
    tb->bg_color = (kui_color_t){30, 30, 30, 240};
    tb->fg_color = (kui_color_t){230, 230, 230, 255};
    tb->accent_color = (kui_color_t){98, 0, 238, 255};
    tb->hover_color = (kui_color_t){50, 50, 50, 255};
    tb->active_color = (kui_color_t){60, 60, 60, 255};
    tb->start_bg = (kui_color_t){98, 0, 238, 255};
    tb->start_fg = (kui_color_t){255, 255, 255, 255};
    tb->start_hovered = false;
    tb->start_pressed = false;
    tb->hovered_item = -1;
    tb->hovered_tray = -1;
    tb->item_count = 0;
    tb->tray_count = 0;
    tb->clock_text[0] = '\0';
    tb->date_text[0] = '\0';
    return tb;
}

void kanvas_taskbar_destroy(kanvas_taskbar_t* tb)
{
    if (!tb) return;
    kapi_kfree(tb);
}

void kanvas_taskbar_paint(kanvas_taskbar_t* tb, uint32_t* fb, int stride, int fw, int fh)
{
    if (!tb || !tb->visible || !fb) return;
    uint32_t bg = kui_col32_inline(tb->bg_color);
    uint32_t fg = kui_col32_inline(tb->fg_color);
    uint32_t accent = kui_col32_inline(tb->accent_color);
    fill_rect_fb(fb, stride, fw, fh, tb->x, tb->y, tb->width, KANVAS_TASKBAR_H_PX, bg);
    int start_x = tb->x + KANVAS_TASKBAR_PAD;
    int start_y = tb->y + (KANVAS_TASKBAR_H_PX - 36) / 2;
    uint32_t start_col = tb->start_hovered ? accent : kui_col32_inline(tb->start_bg);
    kui_draw_rounded_rect(fb, stride, fw, fh, start_x, start_y, 36, 36, 8, start_col);
    kui_draw_text(fb, stride, fw, fh, start_x + 8, start_y + 8, "K", kui_col32_inline(tb->start_fg), 18, 1);
    int item_x = start_x + 36 + KANVAS_TASKBAR_PAD * 2;
    for (int i = 0; i < tb->item_count; i++) {
        kanvas_taskbar_item_t* item = &tb->items[i];
        int item_y = tb->y + (KANVAS_TASKBAR_H_PX - KANVAS_TASKBAR_ITEM_H) / 2;
        if (item->hovered) {
            kui_draw_rounded_rect(fb, stride, fw, fh, item_x, item_y, KANVAS_TASKBAR_ITEM_W, KANVAS_TASKBAR_ITEM_H, 6, kui_col32_inline(tb->hover_color));
        }
        if (item->active) {
            kui_draw_filled_rect(fb, stride, fw, fh, item_x + 8, tb->y + KANVAS_TASKBAR_H_PX - 3, KANVAS_TASKBAR_ITEM_W - 16, 2, accent);
        }
        kui_draw_icon(fb, stride, fw, fh, item_x + (KANVAS_TASKBAR_ITEM_W - 24) / 2, item_y + (KANVAS_TASKBAR_ITEM_H - 24) / 2, item->icon_id, 24, fg);
        item_x += KANVAS_TASKBAR_ITEM_W + KANVAS_TASKBAR_PAD;
    }
    int tray_x = tb->x + tb->width - KANVAS_TASKBAR_PAD;
    for (int i = tb->tray_count - 1; i >= 0; i--) {
        kanvas_tray_icon_t* tray = &tb->tray[i];
        tray_x -= 24;
        int tray_y = tb->y + (KANVAS_TASKBAR_H_PX - 24) / 2;
        kui_draw_icon(fb, stride, fw, fh, tray_x, tray_y, tray->icon_id, 20, fg);
        tray_x -= KANVAS_TASKBAR_PAD;
    }
    if (tb->clock_text[0]) {
        int cw = (int)(kapi_strlen(tb->clock_text) * 8);
        kui_draw_text(fb, stride, fw, fh, tb->x + tb->width - cw - 16, tb->y + (KANVAS_TASKBAR_H_PX - 13) / 2, tb->clock_text, fg, 13, 0);
    }
}

static void fill_rect_fb(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t color)
{
    (void)fh;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fw) w = fw - x;
    if (w <= 0 || h <= 0) return;
    for (int row = y; row < y + h; row++) {
        uint32_t* p = (uint32_t*)((uint8_t*)fb + row * stride);
        for (int col = x; col < x + w; col++) p[col] = color;
    }
}

void kanvas_taskbar_handle_mouse(kanvas_taskbar_t* tb, int mx, int my, bool left_down, bool left_up)
{
    if (!tb || !tb->visible) return;
    (void)my; (void)left_up;
    if (mx >= tb->x + KANVAS_TASKBAR_PAD && mx < tb->x + KANVAS_TASKBAR_PAD + 36) {
        tb->start_hovered = true;
        if (left_down) tb->start_pressed = true;
    } else {
        tb->start_hovered = false;
    }
    int item_x = tb->x + KANVAS_TASKBAR_PAD + 36 + KANVAS_TASKBAR_PAD * 2;
    tb->hovered_item = -1;
    for (int i = 0; i < tb->item_count; i++) {
        if (mx >= item_x && mx < item_x + KANVAS_TASKBAR_ITEM_W) {
            tb->hovered_item = i;
            tb->items[i].hovered = true;
        } else {
            tb->items[i].hovered = false;
        }
        item_x += KANVAS_TASKBAR_ITEM_W + KANVAS_TASKBAR_PAD;
    }
}

void kanvas_taskbar_update_clock(kanvas_taskbar_t* tb, uint64_t now_ms)
{
    if (!tb) return;
    uint64_t sec = now_ms / 1000;
    uint64_t min = sec / 60;
    uint64_t hr = min / 60;
    int h = (int)(hr % 24);
    int m = (int)(min % 60);
    tb->clock_text[0] = (char)('0' + h / 10);
    tb->clock_text[1] = (char)('0' + h % 10);
    tb->clock_text[2] = ':';
    tb->clock_text[3] = (char)('0' + m / 10);
    tb->clock_text[4] = (char)('0' + m % 10);
    tb->clock_text[5] = '\0';
}

int kanvas_taskbar_add_item(kanvas_taskbar_t* tb, const char* name, int icon_id, bool pinned, void* app_data)
{
    if (!tb || tb->item_count >= KANVAS_TASKBAR_MAX_ITEMS) return -1;
    int idx = tb->item_count++;
    kanvas_taskbar_item_t* item = &tb->items[idx];
    memset(item, 0, sizeof(kanvas_taskbar_item_t));
    if (name) {
        size_t len = kapi_strlen(name);
        if (len >= 64) len = 63;
        memcpy(item->name, name, len);
    }
    item->icon_id = icon_id;
    item->pinned = pinned;
    item->app_data = app_data;
    return idx;
}

void kanvas_taskbar_remove_item(kanvas_taskbar_t* tb, int index)
{
    if (!tb || index < 0 || index >= tb->item_count) return;
    tb->items[index] = tb->items[--tb->item_count];
}

void kanvas_taskbar_set_item_running(kanvas_taskbar_t* tb, int index, bool running)
{
    if (!tb || index < 0 || index >= tb->item_count) return;
    tb->items[index].running = running;
}

void kanvas_taskbar_set_item_active(kanvas_taskbar_t* tb, int index, bool active)
{
    if (!tb || index < 0 || index >= tb->item_count) return;
    tb->items[index].active = active;
}

int kanvas_taskbar_add_tray(kanvas_taskbar_t* tb, const char* name, int icon_id, void (*on_click)(void))
{
    if (!tb || tb->tray_count >= KANVAS_TASKBAR_MAX_TRAY) return -1;
    int idx = tb->tray_count++;
    kanvas_tray_icon_t* tray = &tb->tray[idx];
    memset(tray, 0, sizeof(kanvas_tray_icon_t));
    if (name) {
        size_t len = kapi_strlen(name);
        if (len >= 32) len = 31;
        memcpy(tray->name, name, len);
    }
    tray->icon_id = icon_id;
    tray->on_click = on_click;
    return idx;
}

void kanvas_taskbar_remove_tray(kanvas_taskbar_t* tb, int index)
{
    if (!tb || index < 0 || index >= tb->tray_count) return;
    tb->tray[index] = tb->tray[--tb->tray_count];
}

void kanvas_taskbar_tray_notify(kanvas_taskbar_t* tb, int index, int count)
{
    if (!tb || index < 0 || index >= tb->tray_count) return;
    tb->tray[index].has_notification = count > 0;
    tb->tray[index].notification_count = count;
}