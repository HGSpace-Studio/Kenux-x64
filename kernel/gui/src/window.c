#include "window.h"
#include "window_manager.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "icon.h"
#include "msf.h"
#include "desktop.h"

extern window_manager_t wm;

#define MAX_WINDOWS 16
#define MAX_WIDGETS 256
#define MAX_STRINGS 4096

static window_t window_pool[MAX_WINDOWS];
static uint32_t window_pool_idx = 0;

static widget_t widget_pool[MAX_WIDGETS];
static uint32_t widget_pool_idx = 0;

static char string_pool[MAX_STRINGS];
static uint32_t string_pool_idx = 0;

static char* pool_strdup(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    if (string_pool_idx + len + 1 > MAX_STRINGS) return NULL;
    char* dst = &string_pool[string_pool_idx];
    for (uint32_t i = 0; i <= len; i++) {
        dst[i] = s[i];
    }
    string_pool_idx += len + 1;
    return dst;
}

#define COLOR_WIN_CTRL_CLOSE_HOVER RGB(232, 17, 35)

static uint32_t strlen8(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

static void str_copy_local(char* dst, const char* src, uint32_t max) {
    uint32_t i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

window_t* window_create(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char* title) {
    if (window_pool_idx >= MAX_WINDOWS) return NULL;
    window_t* win = &window_pool[window_pool_idx++];
    uint32_t sx = gui_scale_x(x);
    uint32_t sy = gui_scale_y(y);
    uint32_t sw = gui_scale_x(w);
    uint32_t sh = gui_scale_y(h);
    uint32_t min_w = gui_scale_size(220);
    uint32_t min_h = gui_scale_size(140);
    uint32_t margin = gui_scale_size(8);
    uint32_t left_limit = gui_scale_size(84);
    if (sw < min_w) sw = min_w;
    if (sh < min_h) sh = min_h;
    if (fb.width > left_limit + margin && sx < left_limit) sx = left_limit;
    if (sw + margin * 2 > fb.width && fb.width > margin * 2) sw = fb.width - margin * 2;
    if (sh + margin * 2 > fb.height && fb.height > margin * 2) sh = fb.height - margin * 2;
    if (sx + sw + margin > fb.width && fb.width > sw + margin) sx = fb.width - sw - margin;
    if (sy + sh + margin > fb.height && fb.height > sh + margin) sy = fb.height - sh - margin;
    win->x = sx;
    win->y = sy;
    win->width = sw;
    win->height = sh;
    win->visible = true;
    win->has_border = true;
    win->has_titlebar = true;
    win->movable = true;
    win->resizable = true;
    win->closable = true;
    win->minimizable = true;
    win->maximizable = true;
    win->bg_color = msf_settings.window_bg;
    win->titlebar_color = msf_settings.titlebar_active;
    win->title_text_color = msf_settings.title_text;
    win->widgets = NULL;
    win->focused_widget = NULL;
    win->next = NULL;
    
    win->title = pool_strdup(title);
    
    win->state.minimized = false;
    win->state.maximized = false;
    win->state.prev_x = sx;
    win->state.prev_y = sy;
    win->state.prev_w = sw;
    win->state.prev_h = sh;
    win->state.has_statusbar = false;
    win->state.statusbar_text[0] = '\0';
    win->state.hover_ctrl = WIN_CTRL_NONE;
    
    win->on_content_click = NULL;
    win->content_click_data = NULL;
    
    return win;
}

void window_destroy(window_t* win) {
    widget_t* w = win->widgets;
    while (w) {
        widget_t* next = w->next;
        widget_destroy(w);
        w = next;
    }
}

static void draw_window_border(window_t* win, bool is_active) {
    uint32_t x = win->x;
    uint32_t y = win->y;
    uint32_t w = win->width;
    uint32_t h = win->height;

    uint32_t outer = is_active
        ? color_lighten(msf_settings.border_color, 26)
        : color_darken(msf_settings.border_color, 10);
    if (w > 2 && h > 2) {
        fb_fill_rounded_rect(x, y, w, 1, 1, outer);
        fb_fill_rounded_rect(x, y + h - 1, w, 1, 1, outer);
        fb_fill_rounded_rect(x, y, 1, h, 1, outer);
        fb_fill_rounded_rect(x + w - 1, y, 1, h, 1, outer);
    }

    if (is_active && w > 48) {
        fb_fill_rounded_rect(x + 20, y + h - 2, w - 40, 2, 1, msf_settings.accent_color);
    }
}

static uint32_t get_ctrl_btn_x(window_t* win, int32_t idx) {
    uint32_t right = win->x + win->width - BORDER_WIDTH - 6;
    uint32_t btn_total = WIN_CTRL_BTN_SIZE + WIN_CTRL_BTN_SPACING;
    return right - (uint32_t)(3 - idx) * btn_total;
}

static void draw_control_buttons(window_t* win) {
    uint32_t btn_y = win->y + BORDER_WIDTH;
    uint32_t btn_h = TITLEBAR_HEIGHT;
    uint32_t btn_center_y = btn_y + (btn_h - WIN_CTRL_BTN_SIZE) / 2;

    uint32_t btn_x;

    if (win->minimizable) {
        btn_x = get_ctrl_btn_x(win, 0);
        if (win->state.hover_ctrl == WIN_CTRL_MINIMIZE) {
            fb_blend_rect(btn_x - 7, btn_y, WIN_CTRL_BTN_SIZE + 14, btn_h,
                          RGB(255, 255, 255), 25);
        }
        icon_draw(btn_x, btn_center_y, ICON_MINIMIZE, WIN_CTRL_BTN_SIZE);
    }

    if (win->maximizable) {
        btn_x = get_ctrl_btn_x(win, 1);
        if (win->state.hover_ctrl == WIN_CTRL_MAXIMIZE) {
            fb_blend_rect(btn_x - 7, btn_y, WIN_CTRL_BTN_SIZE + 14, btn_h,
                          RGB(255, 255, 255), 25);
        }
        icon_id_t icon = win->state.maximized ? ICON_RESTORE : ICON_MAXIMIZE;
        icon_draw(btn_x, btn_center_y, icon, WIN_CTRL_BTN_SIZE);
    }

    if (win->closable) {
        btn_x = get_ctrl_btn_x(win, 2);
        if (win->state.hover_ctrl == WIN_CTRL_CLOSE) {
            fb_fill_rect(btn_x - 7, btn_y, WIN_CTRL_BTN_SIZE + 14, btn_h,
                         COLOR_WIN_CTRL_CLOSE_HOVER);
        }
        icon_draw(btn_x, btn_center_y, ICON_CLOSE, WIN_CTRL_BTN_SIZE);
    }
}

static void draw_statusbar(window_t* win, uint32_t x, uint32_t y, uint32_t w) {
    if (!win->state.has_statusbar) return;

    gfx_draw_hline(x, y, w, msf_settings.border_color);

    uint32_t text_y = y + (STATUSBAR_HEIGHT > 16 ? (STATUSBAR_HEIGHT - 16) / 2 : 4);
    font_draw_text(x + 8, text_y, win->state.statusbar_text,
                   color_lighten(msf_settings.font_color, 30));
}

void window_paint(window_t* win) {
    if (!win->visible) return;
    if (win->state.minimized) return;

    bool is_active = (win == wm.active_window);

    fb_draw_window_shadow(win->x, win->y, win->width, win->height,
                          WINDOW_CORNER_RADIUS, WINDOW_SHADOW_BLUR, WINDOW_SHADOW_ALPHA);

    fb_fill_rounded_rect(win->x, win->y, win->width, win->height,
                         WINDOW_CORNER_RADIUS, win->bg_color);

    draw_window_border(win, is_active);

    uint32_t content_x = win->x + BORDER_WIDTH;
    uint32_t content_y = win->y + BORDER_WIDTH;
    uint32_t content_w = win->width - BORDER_WIDTH * 2;
    uint32_t content_h = win->height - BORDER_WIDTH * 2;

    if (win->has_titlebar) {
        uint32_t tb_color = is_active
            ? win->titlebar_color
            : msf_settings.titlebar_inactive;
        fb_fill_rounded_rect(content_x, content_y, content_w, TITLEBAR_HEIGHT + 8,
                             WINDOW_CORNER_RADIUS - 1, tb_color);
        fb_fill_rect(content_x, content_y + TITLEBAR_HEIGHT - 8, content_w, 16, tb_color);
        fb_blend_rect(content_x + 12, content_y + 3, content_w - 24,
                      TITLEBAR_HEIGHT / 2, RGB(255, 255, 255), is_active ? 8 : 4);

        gfx_draw_hline(content_x + 12, content_y + TITLEBAR_HEIGHT,
                       content_w - 24, color_lighten(msf_settings.border_color, 8));

        uint32_t icon_sz = 20;
        uint32_t icon_x = content_x + 10;
        uint32_t icon_y = content_y + (TITLEBAR_HEIGHT - icon_sz) / 2;
        icon_draw(icon_x, icon_y, ICON_APP, icon_sz);

        uint32_t text_x = content_x + 38;
        uint32_t text_y = content_y + (TITLEBAR_HEIGHT > 16 ? (TITLEBAR_HEIGHT - 16) / 2 : 8);
        font_draw_text(text_x, text_y, win->title, win->title_text_color);

        draw_control_buttons(win);

        content_y += TITLEBAR_HEIGHT;
        content_h -= TITLEBAR_HEIGHT;
    }

    if (win->state.has_statusbar) {
        uint32_t statusbar_y = content_y + content_h - STATUSBAR_HEIGHT;
        draw_statusbar(win, content_x, statusbar_y, content_w);
        content_h -= STATUSBAR_HEIGHT;
    }

    window_paint_widgets(win);
}

void window_paint_widgets(window_t* win) {
    if (!win || !win->visible || win->state.minimized) return;
    widget_t* w = win->widgets;
    while (w) {
        widget_paint(w, win);
        w = w->next;
    }
}

void window_add_widget(window_t* win, widget_t* widget) {
    widget->next = win->widgets;
    win->widgets = widget;
}

void window_remove_widget(window_t* win, widget_t* widget) {
    widget_t** p = &win->widgets;
    while (*p) {
        if (*p == widget) {
            *p = widget->next;
            widget->next = NULL;
            return;
        }
        p = &(*p)->next;
    }
}

widget_t* window_get_widget_at(window_t* win, uint32_t x, uint32_t y) {
    widget_t* w = win->widgets;
    while (w) {
        if (w->visible && widget_point_inside(w, x, y)) {
            return w;
        }
        w = w->next;
    }
    return NULL;
}

bool window_point_inside(window_t* win, uint32_t px, uint32_t py) {
    if (win->state.minimized) return false;
    return (px >= win->x && px < win->x + win->width &&
            py >= win->y && py < win->y + win->height);
}

bool window_titlebar_point_inside(window_t* win, uint32_t px, uint32_t py) {
    if (!win->has_titlebar || win->state.minimized) return false;
    uint32_t title_y = win->y + BORDER_WIDTH;
    uint32_t title_bottom = title_y + TITLEBAR_HEIGHT;
    return (px >= win->x + BORDER_WIDTH && px < win->x + win->width - BORDER_WIDTH &&
            py >= title_y && py < title_bottom);
}

win_ctrl_hit_t window_hit_control(window_t* win, uint32_t px, uint32_t py) {
    if (!win->has_titlebar || win->state.minimized) return WIN_CTRL_NONE;
    
    uint32_t btn_y = win->y + BORDER_WIDTH + (TITLEBAR_HEIGHT - WIN_CTRL_BTN_SIZE) / 2;
    if (py < btn_y || py >= btn_y + WIN_CTRL_BTN_SIZE) return WIN_CTRL_NONE;
    
    if (win->minimizable) {
        uint32_t bx = get_ctrl_btn_x(win, 0);
        if (px >= bx && px < bx + WIN_CTRL_BTN_SIZE) return WIN_CTRL_MINIMIZE;
    }
    if (win->maximizable) {
        uint32_t bx = get_ctrl_btn_x(win, 1);
        if (px >= bx && px < bx + WIN_CTRL_BTN_SIZE) return WIN_CTRL_MAXIMIZE;
    }
    if (win->closable) {
        uint32_t bx = get_ctrl_btn_x(win, 2);
        if (px >= bx && px < bx + WIN_CTRL_BTN_SIZE) return WIN_CTRL_CLOSE;
    }
    
    return WIN_CTRL_NONE;
}

void window_minimize(window_t* win) {
    win->state.minimized = true;
    win->visible = false;
}

void window_maximize(window_t* win) {
    if (win->state.maximized) {
        window_restore(win);
        return;
    }
    win->state.prev_x = win->x;
    win->state.prev_y = win->y;
    win->state.prev_w = win->width;
    win->state.prev_h = win->height;
    uint32_t left = desktop.tb_w ? desktop.tb_x + desktop.tb_w + gui_scale_size(8) : gui_scale_size(72);
    uint32_t margin = gui_scale_size(4);
    win->x = left;
    win->y = margin;
    win->width = fb.width > left + margin ? fb.width - left - margin : fb.width;
    win->height = fb.height > margin * 2 ? fb.height - margin * 2 : fb.height;
    win->state.maximized = true;
}

void window_restore(window_t* win) {
    if (win->state.maximized) {
        win->x = win->state.prev_x;
        win->y = win->state.prev_y;
        win->width = win->state.prev_w;
        win->height = win->state.prev_h;
        win->state.maximized = false;
    }
    if (win->state.minimized) {
        win->state.minimized = false;
        win->visible = true;
    }
}

void window_close(window_t* win) {
    win->visible = false;
}

void window_set_statusbar(window_t* win, const char* text) {
    win->state.has_statusbar = true;
    str_copy_local(win->state.statusbar_text, text, sizeof(win->state.statusbar_text));
}

void window_get_content_rect(window_t* win, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) {
    *x = win->x + BORDER_WIDTH;
    *y = win->y + BORDER_WIDTH;
    *w = win->width - BORDER_WIDTH * 2;
    *h = win->height - BORDER_WIDTH * 2;
    
    if (win->has_titlebar) {
        *y += TITLEBAR_HEIGHT;
        *h -= TITLEBAR_HEIGHT;
    }
    if (win->state.has_statusbar) {
        *h -= STATUSBAR_HEIGHT;
    }
}
