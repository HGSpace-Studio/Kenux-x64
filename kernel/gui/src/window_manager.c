#include "window_manager.h"
#include "desktop.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "icon.h"
#include "cursor_data.h"
#include "taskmgr.h"
#include "thispc.h"
#include "sysinfo.h"
#include "filemgr.h"
#include "terminal.h"
#include "compat_center.h"

window_manager_t wm;

#define WM_DAMAGE_QUEUE_CAPACITY 32
#define WM_DAMAGE_MERGE_GAP 24
#define WM_DAMAGE_LARGE_PERCENT 68
#define WM_WINDOW_DAMAGE_PAD 12
#define WM_CURSOR_DAMAGE_PAD 4

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} wm_rect_t;

static uint32_t cursor_bg[CURSOR_DRAW_SIZE * CURSOR_DRAW_SIZE];
static wm_rect_t cursor_bg_rect;
static bool cursor_bg_valid = false;
static wm_rect_t cursor_last_damage_rect;
static bool cursor_last_damage_valid = false;
static wm_rect_t wm_damage_queue[WM_DAMAGE_QUEUE_CAPACITY];
static uint32_t wm_damage_count = 0;

static void wm_cursor_bounds(uint32_t x, uint32_t y, cursor_type_t type, wm_rect_t* r) {
    int32_t hot_x = 0;
    int32_t hot_y = 0;
    const int32_t center_hot = (int32_t)CURSOR_DRAW_SIZE / 2;

    switch (type) {
        case CURSOR_TEXT:
        case CURSOR_BUSY:
        case CURSOR_MOVE:
        case CURSOR_RESIZE_NS:
        case CURSOR_RESIZE_EW:
        case CURSOR_RESIZE_NESW:
        case CURSOR_RESIZE_NWSE:
        case CURSOR_UNAVAILABLE:
        case CURSOR_PRECISION:
            hot_x = center_hot;
            hot_y = center_hot;
            break;
        case CURSOR_LINK:
            hot_x = 12;
            hot_y = 3;
            break;
        case CURSOR_HELP:
        case CURSOR_DEFAULT:
        default:
            hot_x = 0;
            hot_y = 0;
            break;
    }

    int32_t dx = (int32_t)x - hot_x;
    int32_t dy = (int32_t)y - hot_y;
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;

    r->x = (uint32_t)dx;
    r->y = (uint32_t)dy;
    r->w = CURSOR_DRAW_SIZE;
    r->h = CURSOR_DRAW_SIZE;

    if (r->x >= fb.width || r->y >= fb.height) {
        r->w = 0;
        r->h = 0;
        return;
    }
    if ((uint64_t)r->x + (uint64_t)r->w > fb.width) r->w = fb.width - r->x;
    if ((uint64_t)r->y + (uint64_t)r->h > fb.height) r->h = fb.height - r->y;
}

static void wm_save_cursor_bg(const wm_rect_t* r) {
    if (!r || r->w == 0 || r->h == 0 || fb.base == NULL) {
        cursor_bg_valid = false;
        return;
    }

    for (uint32_t row = 0; row < r->h; row++) {
        uint32_t* src = (uint32_t*)(fb.base + (r->y + row) * fb.pitch + r->x * 4);
        for (uint32_t col = 0; col < r->w; col++) {
            cursor_bg[row * CURSOR_DRAW_SIZE + col] = src[col];
        }
    }

    cursor_bg_rect = *r;
    cursor_bg_valid = true;
}

static void wm_restore_cursor_bg(void) {
    if (!cursor_bg_valid || fb.base == NULL) return;

    for (uint32_t row = 0; row < cursor_bg_rect.h; row++) {
        uint32_t* dst = (uint32_t*)(fb.base + (cursor_bg_rect.y + row) * fb.pitch + cursor_bg_rect.x * 4);
        for (uint32_t col = 0; col < cursor_bg_rect.w; col++) {
            dst[col] = cursor_bg[row * CURSOR_DRAW_SIZE + col];
        }
    }
}

static wm_rect_t wm_rect_clip_screen(wm_rect_t r);

static wm_rect_t wm_rect_union(wm_rect_t a, wm_rect_t b) {
    if (a.w == 0 || a.h == 0) return b;
    if (b.w == 0 || b.h == 0) return a;

    uint64_t x1 = a.x < b.x ? a.x : b.x;
    uint64_t y1 = a.y < b.y ? a.y : b.y;
    uint64_t ax2 = (uint64_t)a.x + (uint64_t)a.w;
    uint64_t ay2 = (uint64_t)a.y + (uint64_t)a.h;
    uint64_t bx2 = (uint64_t)b.x + (uint64_t)b.w;
    uint64_t by2 = (uint64_t)b.y + (uint64_t)b.h;
    uint64_t x2 = ax2 > bx2 ? ax2 : bx2;
    uint64_t y2 = ay2 > by2 ? ay2 : by2;

    wm_rect_t out;
    out.x = x1 > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)x1;
    out.y = y1 > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)y1;
    out.w = (x2 <= x1) ? 0 : (x2 - x1 > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)(x2 - x1));
    out.h = (y2 <= y1) ? 0 : (y2 - y1 > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)(y2 - y1));
    return wm_rect_clip_screen(out);
}

static bool wm_rect_empty(wm_rect_t r) {
    return r.w == 0 || r.h == 0;
}

static uint64_t wm_rect_area(wm_rect_t r) {
    return (uint64_t)r.w * (uint64_t)r.h;
}

static bool wm_rect_intersects(wm_rect_t a, wm_rect_t b) {
    if (wm_rect_empty(a) || wm_rect_empty(b)) return false;
    uint64_t ax2 = (uint64_t)a.x + (uint64_t)a.w;
    uint64_t ay2 = (uint64_t)a.y + (uint64_t)a.h;
    uint64_t bx2 = (uint64_t)b.x + (uint64_t)b.w;
    uint64_t by2 = (uint64_t)b.y + (uint64_t)b.h;
    return (uint64_t)a.x < bx2 && (uint64_t)b.x < ax2 &&
           (uint64_t)a.y < by2 && (uint64_t)b.y < ay2;
}

static bool wm_rect_close_enough(wm_rect_t a, wm_rect_t b) {
    if (wm_rect_intersects(a, b)) return true;
    uint64_t ax2 = (uint64_t)a.x + (uint64_t)a.w + WM_DAMAGE_MERGE_GAP;
    uint64_t ay2 = (uint64_t)a.y + (uint64_t)a.h + WM_DAMAGE_MERGE_GAP;
    uint64_t bx2 = (uint64_t)b.x + (uint64_t)b.w + WM_DAMAGE_MERGE_GAP;
    uint64_t by2 = (uint64_t)b.y + (uint64_t)b.h + WM_DAMAGE_MERGE_GAP;
    return (uint64_t)a.x <= bx2 &&
           (uint64_t)b.x <= ax2 &&
           (uint64_t)a.y <= by2 &&
           (uint64_t)b.y <= ay2;
}

static wm_rect_t wm_rect_clip_screen(wm_rect_t r) {
    if (fb.width == 0 || fb.height == 0 || r.x >= fb.width || r.y >= fb.height) {
        r.w = 0;
        r.h = 0;
        return r;
    }
    if ((uint64_t)r.x + (uint64_t)r.w > fb.width) r.w = fb.width - r.x;
    if ((uint64_t)r.y + (uint64_t)r.h > fb.height) r.h = fb.height - r.y;
    return r;
}

static wm_rect_t wm_rect_expand(wm_rect_t r, uint32_t pad) {
    if (wm_rect_empty(r) || pad == 0) return r;

    int32_t x = (int32_t)r.x - (int32_t)pad;
    int32_t y = (int32_t)r.y - (int32_t)pad;
    int32_t w = (int32_t)r.w + (int32_t)pad * 2;
    int32_t h = (int32_t)r.h + (int32_t)pad * 2;

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (w <= 0 || h <= 0) {
        wm_rect_t empty = {0, 0, 0, 0};
        return empty;
    }

    wm_rect_t out = {(uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h};
    return wm_rect_clip_screen(out);
}

static wm_rect_t wm_window_bounds(window_t* win) {
    wm_rect_t r = {0, 0, 0, 0};
    if (!win) return r;
    int32_t x = (int32_t)win->x - WM_WINDOW_DAMAGE_PAD;
    int32_t y = (int32_t)win->y - WM_WINDOW_DAMAGE_PAD;
    int32_t w = (int32_t)win->width + WM_WINDOW_DAMAGE_PAD * 2;
    int32_t h = (int32_t)win->height + WM_WINDOW_DAMAGE_PAD * 2;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (w <= 0 || h <= 0) return r;
    r.x = (uint32_t)x;
    r.y = (uint32_t)y;
    r.w = (uint32_t)w;
    r.h = (uint32_t)h;
    return wm_rect_clip_screen(r);
}

static bool wm_damage_should_merge(wm_rect_t a, wm_rect_t b) {
    if (wm_rect_intersects(a, b)) return true;
    if (!wm_rect_close_enough(a, b)) return false;

    wm_rect_t merged = wm_rect_union(a, b);
    uint64_t source_area = wm_rect_area(a) + wm_rect_area(b);
    uint64_t merged_area = wm_rect_area(merged);
    if (merged_area <= source_area) return true;
    return (merged_area - source_area) * 100 <= source_area * 45;
}

static void wm_damage_compact(void) {
    for (uint32_t i = 0; i < wm_damage_count; i++) {
        for (uint32_t j = i + 1; j < wm_damage_count;) {
            if (!wm_damage_should_merge(wm_damage_queue[i], wm_damage_queue[j])) {
                j++;
                continue;
            }
            wm_damage_queue[i] = wm_rect_union(wm_damage_queue[i], wm_damage_queue[j]);
            wm_damage_queue[j] = wm_damage_queue[wm_damage_count - 1];
            wm_damage_count--;
        }
    }
}

static void wm_draw_cursor_and_cache(void) {
    wm_rect_t r;
    wm_cursor_bounds(wm.mouse_x, wm.mouse_y, desktop.cursor_type, &r);
    wm_save_cursor_bg(&r);
    cursor_paint(wm.mouse_x, wm.mouse_y, desktop.cursor_type);
    cursor_last_damage_rect = wm_rect_expand(r, WM_CURSOR_DAMAGE_PAD);
    cursor_last_damage_valid = !wm_rect_empty(cursor_last_damage_rect);
}

void wm_invalidate_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    wm_rect_t rect = {x, y, w, h};
    rect = wm_rect_clip_screen(rect);
    if (wm_rect_empty(rect)) return;

    wm.needs_flush = true;

    uint64_t screen_area = (uint64_t)fb.width * (uint64_t)fb.height;
    if (screen_area != 0 && wm_rect_area(rect) * 100 >= screen_area * WM_DAMAGE_LARGE_PERCENT) {
        wm_damage_queue[0].x = 0;
        wm_damage_queue[0].y = 0;
        wm_damage_queue[0].w = fb.width;
        wm_damage_queue[0].h = fb.height;
        wm_damage_count = 1;
        return;
    }

    for (uint32_t i = 0; i < wm_damage_count;) {
        if (!wm_damage_should_merge(wm_damage_queue[i], rect)) {
            i++;
            continue;
        }
        rect = wm_rect_union(rect, wm_damage_queue[i]);
        wm_damage_queue[i] = wm_damage_queue[wm_damage_count - 1];
        wm_damage_count--;
    }

    if (wm_damage_count < WM_DAMAGE_QUEUE_CAPACITY) {
        wm_damage_queue[wm_damage_count++] = rect;
    } else {
        uint32_t best = 0;
        uint64_t best_growth = (uint64_t)-1;
        for (uint32_t i = 0; i < wm_damage_count; i++) {
            wm_rect_t merged = wm_rect_union(wm_damage_queue[i], rect);
            uint64_t growth = wm_rect_area(merged) - wm_rect_area(wm_damage_queue[i]);
            if (growth < best_growth) {
                best_growth = growth;
                best = i;
            }
        }
        wm_damage_queue[best] = wm_rect_union(wm_damage_queue[best], rect);
        wm_damage_compact();
    }
}

void wm_invalidate_window(window_t* win) {
    wm_rect_t r = wm_window_bounds(win);
    wm_invalidate_rect(r.x, r.y, r.w, r.h);
}

static bool wm_str_equal(const char* a, const char* b) {
    uint32_t i = 0;
    if (!a || !b) return false;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static void wm_paint_app_content(window_t* win) {
    if (!win || !win->visible || win->state.minimized || !win->title) return;

    /* The window manager repaints frames; apps repaint only their content.
     * Do not call APIs that repaint the window frame again from here. */
    if (wm_str_equal(win->title, "This PC")) {
        thispc_on_show();
    } else if (wm_str_equal(win->title, "System Info")) {
        sysinfo_on_show();
    } else if (wm_str_equal(win->title, "Kenux Explorer")) {
        filemgr_refresh();
    } else if (wm_str_equal(win->title, "Terminal")) {
        terminal_refresh();
    } else if (wm_str_equal(win->title, "Task Manager")) {
        taskmgr_paint_content();
    } else if (wm_str_equal(win->title, "Compatibility Center")) {
        compat_center_on_show();
    }
}

void wm_init(uint32_t desktop_color) {
    wm.windows = NULL;
    wm.active_window = NULL;
    wm.mouse_x = fb.width / 2;
    wm.mouse_y = fb.height / 2;
    wm.mouse_left_down = false;
    wm.mouse_right_down = false;
    wm.dragging = false;
    wm.drag_offset_x = 0;
    wm.drag_offset_y = 0;
    wm.drag_window = NULL;
    wm.desktop_color = desktop_color;
    wm.need_full_repaint = false;
    wm.needs_flush = false;

    desktop_init();
}

void wm_add_window(window_t* win) {
    win->next = wm.windows;
    wm.windows = win;
    if (!wm.active_window) {
        wm.active_window = win;
    }
    taskbar_add_window(win);
    taskbar_update_active(win);
}

void wm_remove_window(window_t* win) {
    window_t** p = &wm.windows;
    while (*p) {
        if (*p == win) {
            *p = win->next;
            win->next = NULL;
            if (wm.active_window == win) {
                wm.active_window = wm.windows;
            }
            taskbar_remove_window(win);
            taskbar_update_active(wm.active_window);
            return;
        }
        p = &(*p)->next;
    }
}

void wm_set_active(window_t* win) {
    if (win == wm.active_window) return;

    window_t* old_active = wm.active_window;

    wm_remove_window(win);
    win->next = wm.windows;
    wm.windows = win;
    wm.active_window = win;
    taskbar_update_active(win);

    if (old_active) {
        wm_invalidate_window(old_active);
    }
    wm_invalidate_window(win);
    if (desktop.tb_w > 0 && desktop.tb_h > 0) {
        wm_invalidate_rect(desktop.tb_x, desktop.tb_y,
                           desktop.tb_w, desktop.tb_h);
    }
}

void wm_paint() {
    wm_damage_count = 0;
    cursor_bg_valid = false;
    cursor_last_damage_valid = false;
    desktop_paint();
    
    window_t* w = wm.windows;
    window_t* win_list[64];
    uint32_t count = 0;
    
    while (w && count < 64) {
        win_list[count++] = w;
        w = w->next;
    }
    
    for (int32_t i = (int32_t)count - 1; i >= 0; i--) {
        window_paint(win_list[i]);
        wm_paint_app_content(win_list[i]);
    }

    /* Start menu and context menus must be above every window and receive
     * input first, matching the user's "start menu above all layers" request. */
    desktop_paint_overlays();
    
    wm_draw_cursor_and_cache();
    gui_fb_flush();
}

void wm_paint_fast(void) {
    if (wm_damage_count == 0) {
        /* Even without damage, overlays may have just become visible.
         * Check and paint them so menus appear immediately. */
        if (desktop.start_menu_open || desktop.context_menu.visible) {
            wm_flush_dirty();
            return;
        }
        wm_paint_cursor_only();
        return;
    }
    wm_flush_dirty();
}

void wm_flush_dirty(void) {
    bool has_overlay = desktop.start_menu_open || desktop.context_menu.visible;

    if (wm_damage_count == 0 && !has_overlay) {
        wm.needs_flush = false;
        return;
    }

    wm.needs_flush = false;
    wm_damage_compact();

    wm_restore_cursor_bg();
    cursor_bg_valid = false;
    cursor_last_damage_valid = false;

    /* Collect window list once */
    window_t* w = wm.windows;
    window_t* win_list[64];
    uint32_t count = 0;
    while (w && count < 64) {
        win_list[count++] = w;
        w = w->next;
    }

    /* Track which windows have been painted to avoid duplicate full repaints */
    bool painted[64];
    for (uint32_t i = 0; i < count; i++) painted[i] = false;

    /* Union of all dirty rects for final hardware flush */
    wm_rect_t flush_union = {0, 0, 0, 0};

    /* Process each dirty rect individually — avoids creating one giant
     * union rect that would force every window to repaint. */
    for (uint32_t di = 0; di < wm_damage_count; di++) {
        wm_rect_t dirty = wm_damage_queue[di];
        dirty = wm_rect_clip_screen(dirty);
        if (wm_rect_empty(dirty)) continue;

        desktop_paint_region(dirty.x, dirty.y, dirty.w, dirty.h);

        for (int32_t i = (int32_t)count - 1; i >= 0; i--) {
            if (painted[i]) continue;
            if (!win_list[i]->visible || win_list[i]->state.minimized) continue;
            wm_rect_t wr = {win_list[i]->x, win_list[i]->y,
                            win_list[i]->width, win_list[i]->height};
            if (!wm_rect_intersects(wr, dirty)) continue;
            window_paint(win_list[i]);
            wm_paint_app_content(win_list[i]);
            painted[i] = true;
        }

        if (wm_rect_empty(flush_union)) {
            flush_union = dirty;
        } else {
            flush_union = wm_rect_union(flush_union, dirty);
        }
    }

    wm_damage_count = 0;

    /* Paint overlays above all windows */
    if (has_overlay) {
        desktop_paint_overlays();
        /* Ensure overlay region is included in flush */
        if (desktop.start_menu_open && desktop.sm_w > 0) {
            wm_rect_t sm = {desktop.sm_x, desktop.sm_y, desktop.sm_w, desktop.sm_h};
            if (wm_rect_empty(flush_union)) flush_union = sm;
            else flush_union = wm_rect_union(flush_union, sm);
        }
        if (desktop.context_menu.visible && desktop.context_menu.width > 0) {
            wm_rect_t cm = {desktop.context_menu.x, desktop.context_menu.y,
                            desktop.context_menu.width, desktop.context_menu.height};
            if (wm_rect_empty(flush_union)) flush_union = cm;
            else flush_union = wm_rect_union(flush_union, cm);
        }
    }

    wm_draw_cursor_and_cache();
    if (cursor_last_damage_valid) {
        if (wm_rect_empty(flush_union)) {
            flush_union = cursor_last_damage_rect;
        } else {
            flush_union = wm_rect_union(flush_union, cursor_last_damage_rect);
        }
    }

    if (!wm_rect_empty(flush_union)) {
        gui_fb_flush_rect(flush_union.x, flush_union.y,
                          flush_union.w, flush_union.h);
    }
}

void wm_paint_cursor_only(void) {
    /* 鼠标移动时必须走正常脏矩形路径重绘旧光标区和新光标区。
     * 直接恢复缓存背景虽然快，但窗口内容刷新、菜单覆盖层、应用局部 flush
     * 都可能让缓存背景过期；一旦把过期背景写回 framebuffer，就会留下拖尾。
     * 两个小矩形重绘仍然很轻，但能保证旧光标完整擦掉。 */
    wm_rect_t raw_new_rect;
    wm_rect_t old_rect = cursor_last_damage_valid
        ? cursor_last_damage_rect
        : wm_rect_expand(cursor_bg_rect, WM_CURSOR_DAMAGE_PAD);
    wm_cursor_bounds(wm.mouse_x, wm.mouse_y, desktop.cursor_type, &raw_new_rect);
    wm_rect_t new_rect = wm_rect_expand(raw_new_rect, WM_CURSOR_DAMAGE_PAD);

    cursor_bg_valid = false;
    cursor_last_damage_valid = false;

    if (!wm_rect_empty(old_rect)) {
        wm_invalidate_rect(old_rect.x, old_rect.y, old_rect.w, old_rect.h);
    }
    if (!wm_rect_empty(new_rect)) {
        wm_invalidate_rect(new_rect.x, new_rect.y, new_rect.w, new_rect.h);
    }
    wm_flush_dirty();
}

void wm_invalidate_cursor_cache(void) {
    cursor_bg_valid = false;
    /* 这里不能清 cursor_last_damage_valid。
     * 光标类型变化发生在鼠标坐标更新之后，但旧光标仍然画在上一次的矩形里。
     * 保留 last damage，下一次重绘才能把旧光标完整擦掉。 */
}

window_t* wm_get_window_at(uint32_t x, uint32_t y) {
    window_t* w = wm.windows;
    while (w) {
        if (w->visible && !w->state.minimized && window_point_inside(w, x, y)) {
            return w;
        }
        w = w->next;
    }
    return NULL;
}

static widget_t* get_widget_at_screen(window_t* win, uint32_t x, uint32_t y) {
    uint32_t content_x = win->x + BORDER_WIDTH;
    uint32_t content_y = win->y + BORDER_WIDTH + TITLEBAR_HEIGHT;
    
    if (x < content_x || y < content_y) return NULL;
    
    return window_get_widget_at(win, x - content_x, y - content_y);
}

static cursor_type_t wm_pick_cursor(uint32_t x, uint32_t y) {
    if (wm.dragging || desktop.icon_dragging) {
        return CURSOR_MOVE;
    }

    if (desktop.context_menu.visible && context_menu_point_inside(x, y)) {
        return CURSOR_LINK;
    }
    if (desktop.start_menu_open && start_menu_point_inside(x, y)) {
        return CURSOR_LINK;
    }
    if (taskbar_point_inside(x, y)) {
        return CURSOR_LINK;
    }
    if (desktop_get_icon_at(x, y) >= 0) {
        return CURSOR_LINK;
    }

    window_t* win = wm_get_window_at(x, y);
    if (!win) {
        return CURSOR_DEFAULT;
    }

    win_ctrl_hit_t ctrl = window_hit_control(win, x, y);
    if (ctrl != WIN_CTRL_NONE) {
        return CURSOR_LINK;
    }
    if (window_titlebar_point_inside(win, x, y) && win->movable) {
        return CURSOR_MOVE;
    }

    widget_t* wgt = get_widget_at_screen(win, x, y);
    if (wgt && wgt->enabled) {
        if (wgt->type == WIDGET_TEXTBOX) {
            return CURSOR_TEXT;
        }
        if (wgt->on_click || wgt->type == WIDGET_BUTTON || wgt->type == WIDGET_CHECKBOX) {
            return CURSOR_LINK;
        }
    }

    return CURSOR_DEFAULT;
}

void wm_handle_mouse_move(uint32_t x, uint32_t y) {
    wm_rect_t old_drag_rect = {0, 0, 0, 0};
    win_ctrl_hit_t old_hover = WIN_CTRL_NONE;
    bool had_drag_rect = false;
    bool had_hover = false;

    if (wm.dragging && wm.drag_window) {
        old_drag_rect = wm_window_bounds(wm.drag_window);
        had_drag_rect = true;
    }
    if (wm.active_window && !wm.dragging && !desktop.icon_dragging) {
        old_hover = wm.active_window->state.hover_ctrl;
        had_hover = true;
    }

    wm.mouse_x = x;
    wm.mouse_y = y;
    
    if (wm.dragging && wm.drag_window) {
        wm.drag_window->x = (uint32_t)((int32_t)x - wm.drag_offset_x);
        wm.drag_window->y = (uint32_t)((int32_t)y - wm.drag_offset_y);
        if (had_drag_rect) {
            wm_rect_t new_drag_rect = wm_window_bounds(wm.drag_window);
            wm_rect_t dirty = wm_rect_union(old_drag_rect, new_drag_rect);
            wm_invalidate_rect(dirty.x, dirty.y, dirty.w, dirty.h);
        }
    }
    
    /* Desktop icon drag */
    if (desktop.icon_dragging) {
        desktop_icon_drag_move(x, y);
    }
    
    if (wm.active_window && !wm.dragging && !desktop.icon_dragging) {
        win_ctrl_hit_t ctrl = window_hit_control(wm.active_window, x, y);
        wm.active_window->state.hover_ctrl = ctrl;
        if (had_hover && old_hover != ctrl) {
            wm_invalidate_window(wm.active_window);
        }
    }

    cursor_set_type(wm_pick_cursor(x, y));
}

void wm_handle_mouse_down(uint8_t button, uint32_t x, uint32_t y) {
    if (button == 0) {
        wm.mouse_left_down = true;
        cursor_set_type(CURSOR_BUSY);
    } else if (button == 1) {
        wm.mouse_right_down = true;
    }
    
    if (button == 1) {
        /* Right-click: show desktop context menu if not on taskbar/start menu */
        desktop_handle_context_menu(x, y);
        return;
    }
    
    if (desktop.context_menu.visible) {
        context_menu_handle_click(x, y);
        return;
    }
    
    if (taskbar_handle_click(x, y)) return;

    if (desktop.start_menu_open) {
        if (start_menu_handle_click(x, y)) return;
    }
    
    window_t* win = wm_get_window_at(x, y);
    if (win) {
        wm_set_active(win);
        
        win_ctrl_hit_t ctrl = window_hit_control(win, x, y);
        if (ctrl == WIN_CTRL_CLOSE) {
            window_close(win);
            wm.need_full_repaint = true;
            return;
        } else if (ctrl == WIN_CTRL_MINIMIZE) {
            window_minimize(win);
            wm.need_full_repaint = true;
            return;
        } else if (ctrl == WIN_CTRL_MAXIMIZE) {
            window_maximize(win);
            wm.need_full_repaint = true;
            return;
        }
        
        if (button == 0 && window_titlebar_point_inside(win, x, y) && win->movable) {
            wm.dragging = true;
            wm.drag_window = win;
            wm.drag_offset_x = (int32_t)x - (int32_t)win->x;
            wm.drag_offset_y = (int32_t)y - (int32_t)win->y;
        } else {
            widget_t* wgt = get_widget_at_screen(win, x, y);
            if (wgt && wgt->enabled) {
                wgt->focused = true;
                if (win->focused_widget && win->focused_widget != wgt) {
                    win->focused_widget->focused = false;
                }
                win->focused_widget = wgt;
                
                if (button == 0 && wgt->on_click) {
                    wgt->on_click(wgt, wgt->user_data);
                }
            } else if (button == 0 && win->on_content_click) {
                uint32_t cx, cy, cw, ch;
                window_get_content_rect(win, &cx, &cy, &cw, &ch);
                win->on_content_click(win, x - cx, y - cy, win->content_click_data);
                wm_invalidate_window(win);
            }
        }
    } else {
        /* Check if clicking on a desktop icon (start drag) */
        if (button == 0 && desktop_icon_drag_start(x, y)) {
            return;  /* Icon drag started */
        }
        desktop_handle_click(x, y, button);
    }
}

void wm_handle_mouse_up(uint8_t button, uint32_t x, uint32_t y) {
    if (button == 0) {
        wm.mouse_left_down = false;
    } else if (button == 1) {
        wm.mouse_right_down = false;
    }
    
    if (wm.dragging && button == 0) {
        wm.dragging = false;
        wm.drag_window = NULL;
    }
    
    /* End desktop icon drag */
    if (desktop.icon_dragging && button == 0) {
        desktop_icon_drag_end();
    }

    cursor_set_type(wm_pick_cursor(x, y));
}

void wm_handle_key(uint16_t key_code, uint16_t key_char) {
    if (wm.active_window && wm.active_window->focused_widget) {
        widget_t* wgt = wm.active_window->focused_widget;
        if (wgt->type == WIDGET_TEXTBOX && key_char) {
            char new_text[256];
            uint32_t len = 0;
            if (wgt->text) {
                while (wgt->text[len] && len < 254) {
                    new_text[len] = wgt->text[len];
                    len++;
                }
            }
            
            if (key_code == 8 && len > 0) {
                len--;
            } else if (key_char >= 32 && key_char <= 126 && len < 254) {
                new_text[len++] = (char)key_char;
            }
            new_text[len] = '\0';
            
            widget_set_text(wgt, new_text);
        }
    }
}