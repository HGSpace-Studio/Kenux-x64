#include "desktop.h"
#include "framebuffer.h"
#include "color.h"
#include "graphics.h"
#include "font.h"
#include "window_manager.h"
#include "msf.h"
#include "cursor_data.h"
#include "wallpaper_data.h"
#include "logo_data.h"

/* ============================================================
 *  Kenux Desktop — Left-side vertical capsule taskbar
 *  + Three-column start menu + Clean desktop
 * ============================================================ */

desktop_t desktop;

/* Forward declaration — defined below taskbar_paint */
static int32_t sqrt_approx(int32_t n);

/* Access active window from window_manager.c */
extern window_manager_t wm;

/* --- color helpers (local macros) --- */
#define CLR_TASKBAR      msf_settings.taskbar_color
#define CLR_TASKBAR_LT   color_lighten(msf_settings.taskbar_color, 25)
#define CLR_TASKBAR_DK   color_darken(msf_settings.taskbar_color, 20)
#define CLR_START_BTN    msf_settings.start_button_color
#define CLR_START_HV     color_lighten(msf_settings.start_button_color, 35)
#define CLR_ACCENT       msf_settings.accent_color
#define CLR_MENU_BG      msf_settings.menu_bg
#define CLR_MENU_HV      msf_settings.menu_hover
#define CLR_MENU_SEP     msf_settings.border_color
#define CLR_FONT         msf_settings.font_color
#define CLR_TITLE        msf_settings.title_text
#define CLR_ICON_TEXT    msf_settings.font_color
#define CLR_CLOCK        msf_settings.font_color

/* --- string helpers --- */
static void int_to_str(uint32_t val, char* buf, uint32_t* len) {
    if (val == 0) { buf[0] = '0'; *len = 1; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    *len = i;
    for (int32_t j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
}

static void str_copy(char* dst, const char* src, uint32_t max) {
    uint32_t i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static bool str_equal(const char* a, const char* b) {
    uint32_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

/* ============================================================
 *  Desktop
 * ============================================================ */

void desktop_init(void) {
    desktop.icon_count = 0;
    desktop.selected_icon = -1;
    desktop.show_icons = false;  /* design: icons hidden by default */
    desktop.wallpaper_color1 = msf_settings.wallpaper_color1;
    desktop.wallpaper_color2 = msf_settings.wallpaper_color2;
    desktop.start_menu_open = false;
    desktop.start_menu_count = 0;
    desktop.start_menu_selected = -1;
    desktop.taskbar_button_count = 0;
    desktop.dock_count = 0;
    desktop.cursor_type = CURSOR_DEFAULT;
    desktop.clock_hour = 12;
    desktop.clock_minute = 0;

    desktop.context_menu.visible = false;
    desktop.context_menu.item_count = 0;
    desktop.context_menu.selected_index = -1;

    desktop.icon_dragging = false;
    desktop.icon_drag_idx = -1;
    desktop.icon_drag_offset_x = 0;
    desktop.icon_drag_offset_y = 0;
    desktop.icon_drag_moved = false;

    start_menu_init();
}

void desktop_show_icons(bool show) {
    desktop.show_icons = show;
}

static void draw_glass_panel(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t r, uint32_t bg, uint32_t border) {
    if (w == 0 || h == 0) return;
    if (r == 0) r = 8;
    if (x + w + 5 < fb.width && y + h + 6 < fb.height) {
        fb_fill_rounded_rect(x + 5, y + 6, w, h, r, RGB(3, 6, 18));
    }
    fb_fill_rounded_rect(x, y, w, h, r, bg);
    if (w > 6 && h > 6) {
        fb_blend_rect(x + 2, y + 2, w - 4, h / 2, RGB(255, 255, 255), 10);
        fb_blend_rect(x + 2, y + h / 2, w - 4, h / 2 > 2 ? h / 2 - 2 : 1,
                      RGB(0, 0, 0), 10);
    }
    gfx_draw_hline(x + r / 2, y, w > r ? w - r : w, color_lighten(border, 70));
    gfx_draw_hline(x + r / 2, y + h - 1, w > r ? w - r : w, color_darken(border, 130));
    gfx_draw_vline(x, y + r / 2, h > r ? h - r : h, color_lighten(border, 100));
    gfx_draw_vline(x + w - 1, y + r / 2, h > r ? h - r : h, color_darken(border, 120));
}

static void draw_kenux_logo(uint32_t x, uint32_t y, uint32_t size) {
    if (size == 0) return;
    if (size == KENUX_LOGO_WIDTH && size == KENUX_LOGO_HEIGHT) {
        fb_blit_alpha(x, y, size, size, kenux_logo_data);
    } else {
        fb_blit_scaled(x, y, size, size, kenux_logo_data, KENUX_LOGO_WIDTH, KENUX_LOGO_HEIGHT);
    }
}

/* Full-screen bitmap wallpaper, cover mode with centered crop */
static uint32_t wp_src_x_map[2048];
static uint32_t wp_src_y_map[2048];
static bool wp_map_valid = false;
static uint32_t wp_map_screen_w = 0;
static uint32_t wp_map_screen_h = 0;

static void wp_build_map(void) {
    if (wp_map_valid && wp_map_screen_w == fb.width && wp_map_screen_h == fb.height)
        return;
    if (fb.width == 0 || fb.height == 0) return;
    if (fb.width > 2048 || fb.height > 2048) return;

    const uint32_t src_w = KENUX_WALLPAPER_WIDTH;
    const uint32_t src_h = KENUX_WALLPAPER_HEIGHT;
    bool scale_by_width = ((uint64_t)fb.width * src_h) >= ((uint64_t)fb.height * src_w);
    uint32_t scaled_w, scaled_h, crop_x = 0, crop_y = 0;

    if (scale_by_width) {
        scaled_w = fb.width;
        scaled_h = (uint32_t)(((uint64_t)src_h * fb.width + src_w - 1) / src_w);
        if (scaled_h > fb.height) crop_y = (scaled_h - fb.height) / 2;
    } else {
        scaled_h = fb.height;
        scaled_w = (uint32_t)(((uint64_t)src_w * fb.height + src_h - 1) / src_h);
        if (scaled_w > fb.width) crop_x = (scaled_w - fb.width) / 2;
    }

    for (uint32_t x = 0; x < fb.width; x++) {
        if (scale_by_width) {
            wp_src_x_map[x] = (uint32_t)(((uint64_t)x * src_w) / scaled_w);
        } else {
            wp_src_x_map[x] = (uint32_t)(((uint64_t)(x + crop_x) * src_h) / scaled_h);
        }
        if (wp_src_x_map[x] >= src_w) wp_src_x_map[x] = src_w - 1;
    }
    for (uint32_t y = 0; y < fb.height; y++) {
        if (scale_by_width) {
            wp_src_y_map[y] = (uint32_t)(((uint64_t)(y + crop_y) * src_w) / scaled_w);
        } else {
            wp_src_y_map[y] = (uint32_t)(((uint64_t)y * src_h) / scaled_h);
        }
        if (wp_src_y_map[y] >= src_h) wp_src_y_map[y] = src_h - 1;
    }

    wp_map_valid = true;
    wp_map_screen_w = fb.width;
    wp_map_screen_h = fb.height;
}

static uint32_t wallpaper_color(uint32_t x, uint32_t y) {
    uint32_t sw = KENUX_WALLPAPER_WIDTH;
    uint32_t sh = KENUX_WALLPAPER_HEIGHT;
    uint32_t dw = fb.width ? fb.width : 1;
    uint32_t dh = fb.height ? fb.height : 1;
    uint64_t lhs = (uint64_t)dw * sh;
    uint64_t rhs = (uint64_t)dh * sw;
    uint32_t scaled_w = lhs <= rhs ? dw : (uint32_t)(((uint64_t)sw * dh) / sh);
    uint32_t scaled_h = lhs <= rhs ? (uint32_t)(((uint64_t)sh * dw) / sw) : dh;
    uint32_t offset_x = dw > scaled_w ? (dw - scaled_w) / 2 : 0;
    uint32_t offset_y = dh > scaled_h ? (dh - scaled_h) / 2 : 0;
    if (x < offset_x || y < offset_y || x >= offset_x + scaled_w || y >= offset_y + scaled_h)
        return RGB(8, 12, 22);
    uint32_t sx = ((uint64_t)(x - offset_x) * sw) / scaled_w;
    uint32_t sy = ((uint64_t)(y - offset_y) * sh) / scaled_h;
    if (sx >= sw) sx = sw - 1;
    if (sy >= sh) sy = sh - 1;
    return kenux_wallpaper_data[sy * sw + sx];
}

static void draw_wallpaper(void) {
    if (!fb.base || !fb.width || !fb.height) return;
    for (uint32_t y = 0; y < fb.height; y++) {
        for (uint32_t x = 0; x < fb.width; x++) fb_set_pixel(x, y, wallpaper_color(x, y));
    }
}

static void draw_wallpaper_region(uint32_t rx, uint32_t ry, uint32_t rw, uint32_t rh) {
    if (!fb.base || !fb.width || !fb.height || rx >= fb.width || ry >= fb.height) return;
    if (rx + rw > fb.width) rw = fb.width - rx;
    if (ry + rh > fb.height) rh = fb.height - ry;
    for (uint32_t y = ry; y < ry + rh; y++) {
        for (uint32_t x = 0; x < rw; x++) fb_set_pixel(rx + x, y, wallpaper_color(rx + x, y));
    }
}

static void paint_desktop_icons(void) {
    if (!desktop.show_icons) return;
    for (uint32_t i = 0; i < desktop.icon_count; i++) {
        desktop_icon_t* ic = &desktop.icons[i];
        uint32_t icon_size = 32;
        uint32_t ix = ic->x + (ic->width - icon_size) / 2;
        uint32_t iy = ic->y;

        if (ic->selected) {
            fb_fill_rounded_rect(ic->x - 4, ic->y - 4, ic->width + 8,
                                 ic->height + 8, 6, color_with_alpha(CLR_ACCENT, 60));
        }
        icon_draw(ix, iy, ic->icon, icon_size);

        uint32_t text_w = font_text_width(ic->label);
        uint32_t tx = ic->x + (ic->width - text_w) / 2;
        uint32_t ty = ic->y + icon_size + 4;
        if (ic->selected) {
            fb_fill_rounded_rect(tx - 4, ty - 2, text_w + 8, FONT_HEIGHT + 4, 4, CLR_ACCENT);
        }
        font_draw_text(tx, ty, ic->label, CLR_ICON_TEXT);
    }
}

void desktop_paint(void) {
    draw_wallpaper();
    paint_desktop_icons();
    taskbar_paint();
}

void desktop_paint_no_wallpaper(void) {
    paint_desktop_icons();
    taskbar_paint();
}

void desktop_paint_region(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    draw_wallpaper_region(x, y, w, h);
    uint64_t rx2 = (uint64_t)x + (uint64_t)w;
    uint64_t ry2 = (uint64_t)y + (uint64_t)h;

    if (desktop.show_icons) {
        for (uint32_t i = 0; i < desktop.icon_count; i++) {
            desktop_icon_t* ic = &desktop.icons[i];
            uint64_t ix2 = (uint64_t)ic->x + (uint64_t)ic->width;
            uint64_t iy2 = (uint64_t)ic->y + (uint64_t)ic->height;
            if (ix2 < x || (uint64_t)ic->x > rx2 ||
                iy2 < y || (uint64_t)ic->y > ry2) continue;
            uint32_t icon_size = 32;
            uint32_t ix = ic->x + (ic->width - icon_size) / 2;
            uint32_t iy = ic->y;
            if (ic->selected) {
                fb_fill_rounded_rect(ic->x - 4, ic->y - 4, ic->width + 8,
                                     ic->height + 8, 6, color_with_alpha(CLR_ACCENT, 60));
            }
            icon_draw(ix, iy, ic->icon, icon_size);
            uint32_t text_w = font_text_width(ic->label);
            uint32_t tx = ic->x + (ic->width - text_w) / 2;
            uint32_t ty = ic->y + icon_size + 4;
            if (ic->selected) {
                fb_fill_rounded_rect(tx - 4, ty - 2, text_w + 8, FONT_HEIGHT + 4, 4, CLR_ACCENT);
            }
            font_draw_text(tx, ty, ic->label, CLR_ICON_TEXT);
        }
    }

    if (desktop.tb_w > 0 && desktop.tb_h > 0) {
        uint64_t tbx2 = (uint64_t)desktop.tb_x + (uint64_t)desktop.tb_w;
        uint64_t tby2 = (uint64_t)desktop.tb_y + (uint64_t)desktop.tb_h;
        if ((uint64_t)desktop.tb_x < rx2 && tbx2 > x &&
            (uint64_t)desktop.tb_y < ry2 && tby2 > y) {
            taskbar_paint();
        }
    }
}

void desktop_paint_overlays(void) {
    /* Three-column start menu (expands rightward from start button) */
    if (desktop.start_menu_open) {
        start_menu_paint();
    }

    /* Right-click context menu */
    if (desktop.context_menu.visible) {
        context_menu_paint();
    }
}

void desktop_add_icon(uint32_t x, uint32_t y, const char* label, icon_id_t icon) {
    desktop_add_icon_with_callback(x, y, label, icon, NULL);
}

void desktop_add_icon_with_callback(uint32_t x, uint32_t y, const char* label, icon_id_t icon, menu_callback_t callback) {
    if (desktop.icon_count >= MAX_DESKTOP_ICONS) return;
    desktop_icon_t* ic = &desktop.icons[desktop.icon_count++];
    ic->x = x;
    ic->y = y;
    ic->width = 60;
    ic->height = 56;
    str_copy(ic->label, label, sizeof(ic->label));
    ic->icon = icon;
    ic->callback = callback;
    ic->selected = false;
}

int32_t desktop_get_icon_at(uint32_t x, uint32_t y) {
    for (int32_t i = (int32_t)desktop.icon_count - 1; i >= 0; i--) {
        desktop_icon_t* ic = &desktop.icons[i];
        if (x >= ic->x && x < ic->x + ic->width &&
            y >= ic->y && y < ic->y + ic->height) {
            return i;
        }
    }
    return -1;
}

void desktop_handle_click(uint32_t x, uint32_t y, uint8_t button) {
    if (button == 0) {
        for (uint32_t i = 0; i < desktop.icon_count; i++) {
            desktop.icons[i].selected = false;
        }
        int32_t idx = desktop_get_icon_at(x, y);
        if (idx >= 0) {
            desktop.icons[idx].selected = true;
            desktop.selected_icon = idx;
        } else {
            desktop.selected_icon = -1;
        }
        if (desktop.start_menu_open && !start_menu_point_inside(x, y)) {
            desktop.start_menu_open = false;
            if (desktop.sm_w > 0) {
                wm_invalidate_rect(desktop.sm_x, desktop.sm_y,
                                   desktop.sm_w, desktop.sm_h);
            }
        }
        if (desktop.context_menu.visible && !context_menu_point_inside(x, y)) {
            desktop.context_menu.visible = false;
            wm_invalidate_rect(desktop.context_menu.x, desktop.context_menu.y,
                               desktop.context_menu.width, desktop.context_menu.height);
        }
    }
}

bool desktop_handle_context_menu(uint32_t x, uint32_t y) {
    /* Don't show context menu if clicking on taskbar or start menu */
    if (taskbar_point_inside(x, y)) return false;
    if (desktop.start_menu_open && start_menu_point_inside(x, y)) return false;
    context_menu_show(x, y);
    return true;
}

/* ============================================================
 *  Desktop Icon Drag (Windows-style)
 * ============================================================ */

bool desktop_icon_drag_start(uint32_t x, uint32_t y) {
    if (!desktop.show_icons) return false;
    int32_t idx = desktop_get_icon_at(x, y);
    if (idx < 0) return false;

    desktop_icon_t* ic = &desktop.icons[idx];
    desktop.icon_dragging = true;
    desktop.icon_drag_idx = idx;
    desktop.icon_drag_offset_x = (int32_t)x - (int32_t)ic->x;
    desktop.icon_drag_offset_y = (int32_t)y - (int32_t)ic->y;
    desktop.icon_drag_moved = false;

    /* Select the icon being dragged */
    for (uint32_t i = 0; i < desktop.icon_count; i++) {
        desktop.icons[i].selected = false;
    }
    ic->selected = true;
    desktop.selected_icon = idx;
    return true;
}

void desktop_icon_drag_move(uint32_t x, uint32_t y) {
    if (!desktop.icon_dragging || desktop.icon_drag_idx < 0) return;
    if ((uint32_t)desktop.icon_drag_idx >= desktop.icon_count) return;

    desktop_icon_t* ic = &desktop.icons[desktop.icon_drag_idx];
    int32_t new_x = (int32_t)x - desktop.icon_drag_offset_x;
    int32_t new_y = (int32_t)y - desktop.icon_drag_offset_y;

    /* Clamp to screen, avoiding left taskbar area */
    uint32_t min_x = desktop.tb_x + desktop.tb_w + 8;
    if (new_x < (int32_t)min_x) new_x = (int32_t)min_x;
    if (new_y < 0) new_y = 0;
    if (new_x + (int32_t)ic->width > (int32_t)fb.width) {
        new_x = (int32_t)fb.width - (int32_t)ic->width;
    }
    if (new_y + (int32_t)ic->height > (int32_t)fb.height) {
        new_y = (int32_t)fb.height - (int32_t)ic->height;
    }

    if (ic->x != (uint32_t)new_x || ic->y != (uint32_t)new_y) {
        desktop.icon_drag_moved = true;
        ic->x = (uint32_t)new_x;
        ic->y = (uint32_t)new_y;
    }
}

void desktop_icon_drag_end(void) {
    if (desktop.icon_dragging && !desktop.icon_drag_moved &&
        desktop.icon_drag_idx >= 0 &&
        (uint32_t)desktop.icon_drag_idx < desktop.icon_count) {
        desktop_icon_t* ic = &desktop.icons[desktop.icon_drag_idx];
        if (ic->callback) {
            ic->callback();
        }
    }
    desktop.icon_dragging = false;
    desktop.icon_drag_idx = -1;
    desktop.icon_drag_moved = false;
}

/* ============================================================
 *  Left-side Vertical Capsule Taskbar
 * ============================================================ */

void taskbar_paint(void) {
    uint32_t tb_x = gui_scale_x(16);
    uint32_t tb_y = gui_scale_y(18);
    uint32_t tb_w = gui_scale_size(58);
    uint32_t tb_h = fb.height > gui_scale_y(36) ? fb.height - gui_scale_y(36) : fb.height;

    /* Clamp height */
    if (tb_h < gui_scale_y(200)) tb_h = gui_scale_y(200);
    if (tb_y + tb_h > fb.height) tb_h = fb.height - tb_y;

    /* Cache geometry */
    desktop.tb_x = tb_x;
    desktop.tb_y = tb_y;
    desktop.tb_w = tb_w;
    desktop.tb_h = tb_h;

    draw_glass_panel(tb_x, tb_y, tb_w, tb_h, gui_scale_size(22), RGB(13, 18, 30), RGB(54, 64, 88));
    fb_blend_rect(tb_x + gui_scale_x(7), tb_y + gui_scale_y(5), tb_w - gui_scale_x(14), gui_scale_y(46), RGB(255, 255, 255), 7);

    /* === Element 1: Start button (top) — Ubuntu Activities style === */
    uint32_t sb_size = gui_scale_size(38);
    uint32_t sb_x = tb_x + (tb_w - sb_size) / 2;
    uint32_t sb_y = tb_y + gui_scale_y(12);

    fb_fill_rounded_rect(sb_x + gui_scale_x(2), sb_y + gui_scale_y(3), sb_size, sb_size,
                         gui_scale_size(13), RGB(4, 7, 14));
    fb_fill_rounded_rect(sb_x, sb_y, sb_size, sb_size, gui_scale_size(13), RGB(24, 31, 48));
    fb_blend_rect(sb_x + gui_scale_x(5), sb_y + gui_scale_y(3),
                  sb_size - gui_scale_x(10), sb_size / 2, RGB(255, 255, 255), 8);
    uint32_t logo_pad = gui_scale_size(6);
    if (logo_pad * 2 >= sb_size) logo_pad = 2;
    draw_kenux_logo(sb_x + logo_pad, sb_y + logo_pad, sb_size - logo_pad * 2);

    uint32_t dock_y = sb_y + sb_size + gui_scale_y(18);
    uint32_t dock_icon_size = gui_scale_size(30);
    uint32_t dock_spacing = gui_scale_y(10);
    uint32_t dock_limit_y = tb_y + tb_h - gui_scale_y(96);
    uint32_t visible_dock_count = desktop.dock_count < MAX_DOCK_APPS ? desktop.dock_count : MAX_DOCK_APPS;
    uint32_t extra_count = 0;

    for (uint32_t i = 0; i < desktop.taskbar_button_count; i++) {
        taskbar_button_t* btn = &desktop.taskbar_buttons[i];
        if (!btn->window || !btn->window->visible) continue;
        bool in_dock = false;
        for (uint32_t j = 0; j < desktop.dock_count; j++) {
            if (desktop.dock_apps[j].window == btn->window) { in_dock = true; break; }
        }
        if (!in_dock) extra_count++;
    }

    uint32_t total_icons = visible_dock_count + extra_count;
    if (total_icons > 0 && dock_limit_y > dock_y) {
        uint32_t available_h = dock_limit_y - dock_y;
        if (dock_icon_size * total_icons > available_h) {
            dock_icon_size = available_h / total_icons;
            if (dock_icon_size < gui_scale_size(18)) dock_icon_size = gui_scale_size(18);
        }
        if (total_icons > 1 && dock_icon_size * total_icons < available_h) {
            dock_spacing = (available_h - dock_icon_size * total_icons) / (total_icons - 1);
            if (dock_spacing > gui_scale_y(10)) dock_spacing = gui_scale_y(10);
        } else {
            dock_spacing = 0;
        }
    }

    for (uint32_t i = 0; i < visible_dock_count; i++) {
        dock_app_t* app = &desktop.dock_apps[i];
        uint32_t cell_size = dock_icon_size + gui_scale_size(16);
        if (cell_size > tb_w - gui_scale_x(8)) cell_size = tb_w - gui_scale_x(8);
        uint32_t iy = dock_y + i * (dock_icon_size + dock_spacing);
        uint32_t cell_x = tb_x + (tb_w - cell_size) / 2;
        uint32_t cell_y = iy > gui_scale_y(8) ? iy - gui_scale_y(8) : iy;
        uint32_t ix = tb_x + (tb_w - dock_icon_size) / 2;
        if (iy + dock_icon_size > dock_limit_y) break;
        bool app_running = app->window && app->window->visible && !app->window->state.minimized;
        bool is_active = app->window && app->window == wm.active_window;

        if (is_active) {
            fb_fill_rounded_rect(cell_x, cell_y, cell_size, cell_size, cell_size / 2, RGB(37, 45, 66));
            fb_blend_rect(cell_x + gui_scale_x(5), cell_y + gui_scale_y(4),
                          cell_size - gui_scale_x(10), cell_size / 2, RGB(255, 255, 255), 7);
        }

        if (app_running || is_active) {
            fb_fill_rounded_rect(tb_x + (tb_w - gui_scale_x(5)) / 2,
                                 iy + dock_icon_size + gui_scale_y(3),
                                 gui_scale_x(5), gui_scale_y(3), gui_scale_size(2), CLR_ACCENT);
        }
        icon_draw(ix, iy, app->icon, dock_icon_size);
    }

    /* Also show running windows not in dock */
    uint32_t extra_y = dock_y + visible_dock_count * (dock_icon_size + dock_spacing);
    for (uint32_t i = 0; i < desktop.taskbar_button_count; i++) {
        taskbar_button_t* btn = &desktop.taskbar_buttons[i];
        if (!btn->window || !btn->window->visible) continue;

        /* Check if already in dock */
        bool in_dock = false;
        for (uint32_t j = 0; j < desktop.dock_count; j++) {
            if (desktop.dock_apps[j].window == btn->window) { in_dock = true; break; }
        }
        if (in_dock) continue;

        uint32_t cell_size = dock_icon_size + gui_scale_size(16);
        if (cell_size > tb_w - gui_scale_x(8)) cell_size = tb_w - gui_scale_x(8);
        uint32_t cell_x = tb_x + (tb_w - cell_size) / 2;
        uint32_t iy = extra_y;
        uint32_t cell_y = iy > gui_scale_y(8) ? iy - gui_scale_y(8) : iy;
        uint32_t ix = tb_x + (tb_w - dock_icon_size) / 2;
        if (iy + dock_icon_size > dock_limit_y) break;

        if (btn->active) {
            fb_fill_rounded_rect(cell_x, cell_y, cell_size, cell_size, gui_scale_size(14), RGB(43, 55, 86));
            fb_blend_rect(cell_x + gui_scale_x(3), cell_y + gui_scale_y(3),
                          cell_size - gui_scale_x(6), cell_size / 2, RGB(255, 255, 255), 10);
        } else {
            fb_fill_rounded_rect(cell_x, cell_y, cell_size, cell_size, gui_scale_size(14), RGB(23, 31, 50));
        }
        fb_fill_rounded_rect(tb_x + gui_scale_x(5), iy + gui_scale_y(6), gui_scale_x(4), dock_icon_size - gui_scale_y(12), gui_scale_size(3), CLR_ACCENT);

        icon_draw(ix, iy, ICON_APP, dock_icon_size);

        btn->x = ix;
        btn->y = iy;
        btn->width = dock_icon_size;
        btn->height = dock_icon_size;

        extra_y += dock_icon_size + dock_spacing;
    }

    /* === Element 3: System status (bottom area) === */
    uint32_t status_y = tb_y + tb_h - gui_scale_y(88);
    uint32_t status_cx = tb_x + tb_w / 2;

    gfx_draw_hline(tb_x + gui_scale_x(16), status_y, tb_w - gui_scale_x(32), RGB(52, 61, 82));
    status_y += gui_scale_y(16);

    icon_draw(status_cx - gui_scale_x(9), status_y + gui_scale_y(3), ICON_NETWORK, gui_scale_size(18));
    status_y += gui_scale_y(32);

    /* Clock */
    char time_buf[8];
    uint32_t tlen = 0;
    if (desktop.clock_hour < 10) {
        time_buf[tlen++] = '0' + desktop.clock_hour;
    } else {
        time_buf[tlen++] = '0' + (desktop.clock_hour / 10);
        time_buf[tlen++] = '0' + (desktop.clock_hour % 10);
    }
    time_buf[tlen++] = ':';
    if (desktop.clock_minute < 10) {
        time_buf[tlen++] = '0';
        time_buf[tlen++] = '0' + desktop.clock_minute;
    } else {
        time_buf[tlen++] = '0' + (desktop.clock_minute / 10);
        time_buf[tlen++] = '0' + (desktop.clock_minute % 10);
    }
    time_buf[tlen] = '\0';

    uint32_t tw = font_text_width(time_buf);
    fb_fill_rounded_rect(status_cx - gui_scale_x(20), status_y - gui_scale_y(4),
                         gui_scale_x(40), gui_scale_y(24), gui_scale_size(10), RGB(24, 30, 44));
    font_draw_text(status_cx - tw / 2, status_y, time_buf, RGB(205, 215, 232));
}

/* Integer sqrt approximation for capsule drawing */
static int32_t sqrt_approx(int32_t n) {
    if (n <= 0) return 0;
    int32_t x = n;
    int32_t y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}

void dock_add_app(icon_id_t icon, const char* label, menu_callback_t cb) {
    if (desktop.dock_count >= MAX_DOCK_APPS) return;
    dock_app_t* app = &desktop.dock_apps[desktop.dock_count++];
    app->icon = icon;
    str_copy(app->label, label, sizeof(app->label));
    app->callback = cb;
    app->window = NULL;
    app->running = false;
}

void taskbar_add_window(window_t* win) {
    if (desktop.taskbar_button_count >= MAX_TASKBAR_BUTTONS) return;
    taskbar_button_t* btn = &desktop.taskbar_buttons[desktop.taskbar_button_count++];
    btn->window = win;
    str_copy(btn->label, win->title, sizeof(btn->label));
    btn->active = false;
    btn->x = 0; btn->y = 0; btn->width = 0; btn->height = 0;

    for (uint32_t i = 0; i < desktop.dock_count; i++) {
        dock_app_t* app = &desktop.dock_apps[i];
        if (str_equal(app->label, win->title)) {
            app->window = win;
            app->running = true;
            break;
        }
    }
}

void taskbar_remove_window(window_t* win) {
    for (uint32_t i = 0; i < desktop.dock_count; i++) {
        if (desktop.dock_apps[i].window == win) {
            desktop.dock_apps[i].window = NULL;
            desktop.dock_apps[i].running = false;
        }
    }

    for (uint32_t i = 0; i < desktop.taskbar_button_count; i++) {
        if (desktop.taskbar_buttons[i].window == win) {
            for (uint32_t j = i; j < desktop.taskbar_button_count - 1; j++) {
                desktop.taskbar_buttons[j] = desktop.taskbar_buttons[j + 1];
            }
            desktop.taskbar_button_count--;
            return;
        }
    }
}

void taskbar_update_active(window_t* win) {
    for (uint32_t i = 0; i < desktop.taskbar_button_count; i++) {
        desktop.taskbar_buttons[i].active = (desktop.taskbar_buttons[i].window == win);
    }
}

bool taskbar_point_inside(uint32_t x, uint32_t y) {
    return (x >= desktop.tb_x && x < desktop.tb_x + desktop.tb_w &&
            y >= desktop.tb_y && y < desktop.tb_y + desktop.tb_h);
}

bool taskbar_handle_click(uint32_t x, uint32_t y) {
    uint32_t tb_x = desktop.tb_x;
    uint32_t tb_y = desktop.tb_y;
    uint32_t tb_w = desktop.tb_w;
    uint32_t tb_h = desktop.tb_h;

    if (x < tb_x || x >= tb_x + tb_w || y < tb_y || y >= tb_y + tb_h)
        return false;

    /* Start button click */
    uint32_t sb_size = gui_scale_size(42);
    uint32_t sb_x = tb_x + (tb_w - sb_size) / 2;
    uint32_t sb_y = tb_y + gui_scale_y(12);
    if (x >= sb_x && x < sb_x + sb_size && y >= sb_y && y < sb_y + sb_size) {
        start_menu_toggle();
        return true;
    }

    /* Dock app clicks: use the same dynamic layout as taskbar_paint() */
    uint32_t dock_y = sb_y + sb_size + gui_scale_y(18);
    uint32_t dock_icon_size = gui_scale_size(30);
    uint32_t dock_spacing = gui_scale_y(10);
    uint32_t dock_limit_y = tb_y + tb_h - gui_scale_y(96);
    uint32_t visible_dock_count = desktop.dock_count < MAX_DOCK_APPS ? desktop.dock_count : MAX_DOCK_APPS;
    uint32_t extra_count = 0;

    for (uint32_t i = 0; i < desktop.taskbar_button_count; i++) {
        taskbar_button_t* btn = &desktop.taskbar_buttons[i];
        if (!btn->window || !btn->window->visible) continue;
        bool in_dock = false;
        for (uint32_t j = 0; j < desktop.dock_count; j++) {
            if (desktop.dock_apps[j].window == btn->window) { in_dock = true; break; }
        }
        if (!in_dock) extra_count++;
    }

    uint32_t total_icons = visible_dock_count + extra_count;
    if (total_icons > 0 && dock_limit_y > dock_y) {
        uint32_t available_h = dock_limit_y - dock_y;
        if (dock_icon_size * total_icons > available_h) {
            dock_icon_size = available_h / total_icons;
            if (dock_icon_size < gui_scale_size(18)) dock_icon_size = gui_scale_size(18);
        }
        if (total_icons > 1 && dock_icon_size * total_icons < available_h) {
            dock_spacing = (available_h - dock_icon_size * total_icons) / (total_icons - 1);
            if (dock_spacing > gui_scale_y(10)) dock_spacing = gui_scale_y(10);
        } else {
            dock_spacing = 0;
        }
    }

    for (uint32_t i = 0; i < visible_dock_count; i++) {
        dock_app_t* app = &desktop.dock_apps[i];
        uint32_t ix = tb_x + (tb_w - dock_icon_size) / 2;
        uint32_t iy = dock_y + i * (dock_icon_size + dock_spacing);
        if (iy + dock_icon_size > dock_limit_y) break;

        if (x >= ix && x < ix + dock_icon_size && y >= iy && y < iy + dock_icon_size) {
            if (app->callback) app->callback();
            return true;
        }
    }

    /* Running window buttons (not in dock) */
    uint32_t extra_y = dock_y + visible_dock_count * (dock_icon_size + dock_spacing);
    for (uint32_t i = 0; i < desktop.taskbar_button_count; i++) {
        taskbar_button_t* btn = &desktop.taskbar_buttons[i];
        if (!btn->window || !btn->window->visible) continue;

        bool in_dock = false;
        for (uint32_t j = 0; j < desktop.dock_count; j++) {
            if (desktop.dock_apps[j].window == btn->window) { in_dock = true; break; }
        }
        if (in_dock) continue;

        uint32_t ix = tb_x + (tb_w - dock_icon_size) / 2;
        uint32_t iy = extra_y;
        if (iy + dock_icon_size > dock_limit_y) break;

        if (x >= ix && x < ix + dock_icon_size && y >= iy && y < iy + dock_icon_size) {
            if (btn->window) {
                wm_set_active(btn->window);
                taskbar_update_active(btn->window);
            }
            return true;
        }
        extra_y += dock_icon_size + dock_spacing;
    }

    return true;  /* Click was inside taskbar, consume it */
}

void taskbar_update_clock(uint32_t hour, uint32_t minute) {
    desktop.clock_hour = hour;
    desktop.clock_minute = minute;
}

/* ============================================================
 *  Three-Column Start Menu
 * ============================================================ */

void start_menu_init(void) {
    desktop.start_menu_count = 0;
    desktop.start_menu_selected = -1;
}

void start_menu_add_item(const char* label, icon_id_t icon, menu_callback_t callback, uint8_t column) {
    if (desktop.start_menu_count >= MAX_MENU_ITEMS) return;
    start_menu_item_t* item = &desktop.start_menu_items[desktop.start_menu_count++];
    str_copy(item->label, label, sizeof(item->label));
    item->icon = icon;
    item->callback = callback;
    item->column = column;
    item->separator = (label[0] == '\0' && icon == ICON_NONE && callback == NULL);
}

void start_menu_toggle(void) {
    desktop.start_menu_open = !desktop.start_menu_open;

    /* Invalidate the area the start menu occupies (or will occupy).
     * If geometry hasn't been computed yet, estimate from taskbar. */
    uint32_t sm_x, sm_y, sm_w, sm_h;
    if (desktop.sm_w > 0 && desktop.sm_h > 0) {
        sm_x = desktop.sm_x;
        sm_y = desktop.sm_y;
        sm_w = desktop.sm_w;
        sm_h = desktop.sm_h;
    } else {
        sm_x = desktop.tb_x + desktop.tb_w + gui_scale_x(8);
        sm_y = desktop.tb_y;
        sm_w = gui_scale_x(START_MENU_WIDTH);
        sm_h = gui_scale_y(START_MENU_HEIGHT);
    }
    wm_invalidate_rect(sm_x, sm_y, sm_w, sm_h);
    /* Also invalidate taskbar for button highlight change */
    if (desktop.tb_w > 0) {
        wm_invalidate_rect(desktop.tb_x, desktop.tb_y,
                           desktop.tb_w, desktop.tb_h);
    }
}

void start_menu_paint(void) {
    /* Position: right of taskbar, starting from start button level */
    uint32_t sm_x = desktop.tb_x + desktop.tb_w + gui_scale_x(8);
    uint32_t sm_y = desktop.tb_y;
    uint32_t sm_w = gui_scale_x(START_MENU_WIDTH);
    uint32_t sm_h = gui_scale_y(START_MENU_HEIGHT);

    /* Clamp to screen */
    if (sm_x + sm_w > fb.width) sm_w = fb.width - sm_x - 4;
    if (sm_y + sm_h > fb.height) sm_h = fb.height - sm_y - 4;

    /* Cache geometry */
    desktop.sm_x = sm_x;
    desktop.sm_y = sm_y;
    desktop.sm_w = sm_w;
    desktop.sm_h = sm_h;

    /* Shadow and glass background */
    fb_draw_window_shadow(sm_x, sm_y, sm_w, sm_h, 14, 12, 95);
    uint32_t bg_top = color_lighten(CLR_MENU_BG, 42);
    uint32_t bg_mid = CLR_MENU_BG;
    uint32_t bg_bottom = color_darken(CLR_MENU_BG, 150);
    fb_fill_rounded_rect(sm_x, sm_y, sm_w, sm_h, 14, bg_mid);
    for (uint32_t i = 10; i + 10 < sm_h; i++) {
        uint32_t c = i < sm_h / 2
                         ? color_mix(bg_top, bg_mid, (i * 256u) / (sm_h / 2 ? sm_h / 2 : 1))
                         : color_mix(bg_mid, bg_bottom, ((i - sm_h / 2) * 256u) / (sm_h / 2 ? sm_h / 2 : 1));
        fb_fill_rect(sm_x + 2, sm_y + i, sm_w - 4, 1, c);
    }
    fb_blend_rect(sm_x + 4, sm_y + 4, sm_w - 8, 46, RGB(255, 255, 255), 13);
    gfx_draw_hline(sm_x + 14, sm_y, sm_w - 28, color_lighten(CLR_MENU_SEP, 80));
    gfx_draw_hline(sm_x + 14, sm_y + sm_h - 1, sm_w - 28, color_darken(CLR_MENU_SEP, 135));

    /* === Search bar at top === */
    uint32_t search_h = 32;
    uint32_t search_y = sm_y + 12;
    uint32_t search_x = sm_x + 12;
    uint32_t search_w = sm_w - 24;

    fb_fill_rounded_rect(search_x + 2, search_y + 3, search_w, search_h, 10,
                         RGB(4, 7, 18));
    fb_fill_rounded_rect(search_x, search_y, search_w, search_h, 10,
                         color_darken(CLR_MENU_BG, 170));
    fb_blend_rect(search_x + 4, search_y + 3, search_w - 8, search_h / 2,
                  RGB(255, 255, 255), 11);
    draw_kenux_logo(search_x + 7, search_y + (search_h - 20) / 2, 20);
    icon_draw(search_x + 32, search_y + (search_h - 16) / 2, ICON_SEARCH, 16);
    font_draw_text(search_x + 54, search_y + (search_h - FONT_HEIGHT) / 2,
                   "Search apps...", color_lighten(CLR_FONT, 120));

    /* === Column dividers === */
    uint32_t col1_w = sm_w / 3;
    uint32_t col2_w = sm_w / 3;
    /* col3 = remaining */
    uint32_t content_y = search_y + search_h + 12;

    /* Vertical separators between columns */
    gfx_draw_vline(sm_x + col1_w, content_y, sm_h - (content_y - sm_y) - 12, color_darken(CLR_MENU_SEP, 145));
    gfx_draw_vline(sm_x + col1_w + col2_w, content_y, sm_h - (content_y - sm_y) - 12, color_darken(CLR_MENU_SEP, 145));

    /* === Column headers === */
    fb_fill_rounded_rect(sm_x + 10, content_y - 2, 54, FONT_HEIGHT + 4, 7, color_darken(CLR_ACCENT, 170));
    fb_fill_rounded_rect(sm_x + col1_w + 10, content_y - 2, 70, FONT_HEIGHT + 4, 7, color_darken(CLR_ACCENT, 170));
    fb_fill_rounded_rect(sm_x + col1_w + col2_w + 10, content_y - 2, 64, FONT_HEIGHT + 4, 7, color_darken(CLR_ACCENT, 170));
    font_draw_text(sm_x + 16, content_y, "Apps", color_lighten(CLR_ACCENT, 40));
    font_draw_text(sm_x + col1_w + 16, content_y, "System", color_lighten(CLR_ACCENT, 40));
    font_draw_text(sm_x + col1_w + col2_w + 16, content_y, "Power", color_lighten(CLR_ACCENT, 40));
    content_y += FONT_HEIGHT + 8;

    /* === Left column: Common apps === */
    uint32_t iy = content_y;
    for (uint32_t i = 0; i < desktop.start_menu_count; i++) {
        start_menu_item_t* item = &desktop.start_menu_items[i];
        if (item->column != 0) continue;
        if (item->separator) {
            gfx_draw_hline(sm_x + 8, iy, col1_w - 16, CLR_MENU_SEP);
            iy += 8;
            continue;
        }
        uint32_t item_h = 28;
        uint32_t item_bg = color_darken(CLR_MENU_BG, 182);
        if ((int32_t)i == desktop.start_menu_selected) {
            item_bg = color_mix(CLR_ACCENT, CLR_MENU_HV, 78);
        }
        fb_fill_rounded_rect(sm_x + 6, iy - 2, col1_w - 12, item_h, 8, item_bg);
        if ((int32_t)i == desktop.start_menu_selected) {
            fb_blend_rect(sm_x + 9, iy, col1_w - 18, item_h / 2,
                          RGB(255, 255, 255), 14);
        }
        icon_draw(sm_x + 12, iy + (item_h - 20) / 2, item->icon, 20);
        font_draw_text(sm_x + 38, iy + (item_h - FONT_HEIGHT) / 2,
                       item->label, CLR_FONT);
        iy += item_h;
        if (iy > sm_y + sm_h - 40) break;
    }

    /* === Middle column: System apps (cards) === */
    iy = content_y;
    for (uint32_t i = 0; i < desktop.start_menu_count; i++) {
        start_menu_item_t* item = &desktop.start_menu_items[i];
        if (item->column != 1) continue;
        if (item->separator) {
            iy += 8;
            continue;
        }
        uint32_t card_h = 40;
        uint32_t card_x = sm_x + col1_w + 8;
        uint32_t card_w = col2_w - 16;

        if (iy + card_h > sm_y + sm_h - 12) break;

        /* Card background */
        uint32_t card_bg = color_darken(CLR_MENU_BG, 182);
        if ((int32_t)i == desktop.start_menu_selected) {
            card_bg = color_mix(CLR_ACCENT, CLR_MENU_HV, 86);
        }
        fb_fill_rounded_rect(card_x + 2, iy + 3, card_w, card_h, 8, RGB(4, 7, 18));
        fb_fill_rounded_rect(card_x, iy, card_w, card_h, 8, card_bg);
        fb_blend_rect(card_x + 4, iy + 3, card_w - 8, card_h / 2, RGB(255, 255, 255), 11);
        fb_fill_rounded_rect(card_x + 4, iy + 6, 4, card_h - 12, 2, CLR_ACCENT);

        icon_draw(card_x + 8, iy + (card_h - 20) / 2, item->icon, 20);
        font_draw_text(card_x + 36, iy + 8, item->label, CLR_FONT);
        font_draw_text(card_x + 36, iy + 22, "Ready", color_darken(CLR_FONT, 40));

        iy += card_h + 4;
    }

    /* === Right column: Power options + User account === */
    iy = content_y;
    for (uint32_t i = 0; i < desktop.start_menu_count; i++) {
        start_menu_item_t* item = &desktop.start_menu_items[i];
        if (item->column != 2) continue;
        if (item->separator) {
            gfx_draw_hline(sm_x + col1_w + col2_w + 8, iy,
                           sm_w - col1_w - col2_w - 16, CLR_MENU_SEP);
            iy += 8;
            continue;
        }
        uint32_t item_h = 28;
        uint32_t item_bg = color_darken(CLR_MENU_BG, 182);
        if ((int32_t)i == desktop.start_menu_selected) {
            item_bg = color_mix(CLR_ACCENT, CLR_MENU_HV, 78);
        }
        fb_fill_rounded_rect(sm_x + col1_w + col2_w + 6, iy - 2,
                             sm_w - col1_w - col2_w - 12, item_h, 8, item_bg);
        if ((int32_t)i == desktop.start_menu_selected) {
            fb_blend_rect(sm_x + col1_w + col2_w + 9, iy,
                          sm_w - col1_w - col2_w - 18, item_h / 2,
                          RGB(255, 255, 255), 14);
        }
        icon_draw(sm_x + col1_w + col2_w + 12, iy + (item_h - 20) / 2,
                  item->icon, 20);
        font_draw_text(sm_x + col1_w + col2_w + 38, iy + (item_h - FONT_HEIGHT) / 2,
                       item->label, CLR_FONT);
        iy += item_h;
    }

    /* User account at bottom-right */
    uint32_t user_y = sm_y + sm_h - 40;
    uint32_t user_x = sm_x + col1_w + col2_w + 8;
    fb_fill_rounded_rect(user_x + 2, user_y + 3, sm_w - col1_w - col2_w - 16, 28, 8,
                         RGB(4, 7, 18));
    fb_fill_rounded_rect(user_x, user_y, sm_w - col1_w - col2_w - 16, 28, 8,
                         color_mix(CLR_MENU_BG, CLR_ACCENT, 38));
    fb_blend_rect(user_x + 4, user_y + 3, sm_w - col1_w - col2_w - 24, 11,
                  RGB(255, 255, 255), 12);
    draw_kenux_logo(user_x + 5, user_y + 3, 22);
    font_draw_text(user_x + 34, user_y + (28 - FONT_HEIGHT) / 2,
                   "KenuxOS", CLR_FONT);

    /* "Kenux" label at bottom-left */
    fb_fill_rounded_rect(sm_x + 10, sm_y + sm_h - FONT_HEIGHT - 12,
                         70, FONT_HEIGHT + 6, 8, color_darken(CLR_MENU_BG, 165));
    font_draw_text(sm_x + 12, sm_y + sm_h - FONT_HEIGHT - 8,
                   "Kenux", color_lighten(CLR_FONT, 130));
}

bool start_menu_point_inside(uint32_t x, uint32_t y) {
    if (!desktop.start_menu_open) return false;
    return (x >= desktop.sm_x && x < desktop.sm_x + desktop.sm_w &&
            y >= desktop.sm_y && y < desktop.sm_y + desktop.sm_h);
}

bool start_menu_handle_click(uint32_t x, uint32_t y) {
    if (!desktop.start_menu_open) return false;
    if (!start_menu_point_inside(x, y)) return false;

    uint32_t sm_x = desktop.sm_x;
    uint32_t sm_y = desktop.sm_y;
    uint32_t sm_w = desktop.sm_w;
    uint32_t sm_h = desktop.sm_h;
    uint32_t col1_w = sm_w / 3;
    uint32_t col2_w = sm_w / 3;

    /* Search bar area — just consume click */
    uint32_t search_y = sm_y + 12;
    uint32_t search_h = 32;
    if (y >= search_y && y < search_y + search_h) {
        return true;  /* TODO: search functionality */
    }

    uint32_t content_y = search_y + search_h + 12 + FONT_HEIGHT + 8;

    /* Determine which column was clicked */
    uint8_t col;
    if (x < sm_x + col1_w) col = 0;
    else if (x < sm_x + col1_w + col2_w) col = 1;
    else col = 2;

    /* Find clicked item in that column */
    uint32_t iy = content_y;
    for (uint32_t i = 0; i < desktop.start_menu_count; i++) {
        start_menu_item_t* item = &desktop.start_menu_items[i];
        if (item->column != col || item->separator) continue;

        uint32_t item_h = (col == 1) ? 40 : 28;
        if (col == 1) {
            /* Card style */
            uint32_t card_x = sm_x + col1_w + 8;
            uint32_t card_w = col2_w - 16;
            if (x >= card_x && x < card_x + card_w &&
                y >= iy && y < iy + item_h) {
                if (item->callback) item->callback();
                desktop.start_menu_open = false;
                wm_invalidate_rect(sm_x, sm_y, sm_w, sm_h);
                return true;
            }
            iy += item_h + 4;
        } else {
            if (y >= iy - 2 && y < iy - 2 + item_h) {
                if (item->callback) item->callback();
                desktop.start_menu_open = false;
                wm_invalidate_rect(sm_x, sm_y, sm_w, sm_h);
                return true;
            }
            iy += item_h;
        }
        if (iy > sm_y + sm_h - 12) break;
    }

    /* User account area */
    uint32_t user_y = sm_y + sm_h - 40;
    if (col == 2 && y >= user_y) {
        return true;  /* TODO: user menu */
    }

    return true;  /* Consume click inside menu */
}

/* ============================================================
 *  Context Menu (right-click on desktop)
 * ============================================================ */

void context_menu_init(void) {
    desktop.context_menu.visible = false;
    desktop.context_menu.item_count = 0;
}

void context_menu_show(uint32_t x, uint32_t y) {
    desktop.context_menu.visible = true;
    desktop.context_menu.x = x;
    desktop.context_menu.y = y;
    desktop.context_menu.width = 180;

    uint32_t h = 8;
    for (uint32_t i = 0; i < desktop.context_menu.item_count; i++) {
        if (desktop.context_menu.items[i].separator) h += 8;
        else h += 30;
    }
    desktop.context_menu.height = h;

    /* Clamp to screen (avoid left taskbar) */
    if (desktop.context_menu.x < desktop.tb_x + desktop.tb_w + 4) {
        desktop.context_menu.x = desktop.tb_x + desktop.tb_w + 8;
    }
    if (desktop.context_menu.x + desktop.context_menu.width > fb.width) {
        desktop.context_menu.x = fb.width - desktop.context_menu.width - 4;
    }
    if (desktop.context_menu.y + desktop.context_menu.height > fb.height) {
        desktop.context_menu.y = fb.height - desktop.context_menu.height - 4;
    }

    /* Invalidate the menu region so it gets painted immediately */
    wm_invalidate_rect(desktop.context_menu.x, desktop.context_menu.y,
                       desktop.context_menu.width, desktop.context_menu.height);
}

void context_menu_add_item(const char* label, icon_id_t icon, menu_callback_t callback, bool separator) {
    if (desktop.context_menu.item_count >= MAX_MENU_ITEMS) return;
    menu_item_t* item = &desktop.context_menu.items[desktop.context_menu.item_count++];
    if (separator) {
        item->label[0] = '\0';
        item->icon = ICON_NONE;
        item->callback = NULL;
        item->separator = true;
    } else {
        str_copy(item->label, label, sizeof(item->label));
        item->icon = icon;
        item->callback = callback;
        item->separator = false;
    }
}

void context_menu_paint(void) {
    if (!desktop.context_menu.visible) return;

    uint32_t x = desktop.context_menu.x;
    uint32_t y = desktop.context_menu.y;
    uint32_t w = desktop.context_menu.width;
    uint32_t h = desktop.context_menu.height;

    fb_draw_window_shadow(x, y, w, h, 10, 6, 70);
    fb_fill_rounded_rect(x, y, w, h, 10, CLR_MENU_BG);

    uint32_t iy = y + 6;
    for (uint32_t i = 0; i < desktop.context_menu.item_count; i++) {
        menu_item_t* item = &desktop.context_menu.items[i];

        if (item->separator) {
            gfx_draw_hline(x + 12, iy, w - 24, CLR_MENU_SEP);
            iy += 8;
            continue;
        }

        uint32_t item_h = 30;
        if (i == (uint32_t)desktop.context_menu.selected_index) {
            fb_fill_rounded_rect(x + 4, iy - 2, w - 8, item_h, 6,
                                 color_lighten(CLR_MENU_HV, 20));
        }
        if (item->icon != ICON_NONE) {
            icon_draw(x + 10, iy + (item_h - 20) / 2, item->icon, 20);
        }
        font_draw_text(x + 38, iy + (item_h - FONT_HEIGHT) / 2,
                       item->label, CLR_FONT);
        iy += item_h;
    }
}

bool context_menu_point_inside(uint32_t x, uint32_t y) {
    if (!desktop.context_menu.visible) return false;
    return (x >= desktop.context_menu.x &&
            x < desktop.context_menu.x + desktop.context_menu.width &&
            y >= desktop.context_menu.y &&
            y < desktop.context_menu.y + desktop.context_menu.height);
}

bool context_menu_handle_click(uint32_t x, uint32_t y) {
    if (!desktop.context_menu.visible) return false;
    if (!context_menu_point_inside(x, y)) {
        desktop.context_menu.visible = false;
        wm_invalidate_rect(desktop.context_menu.x, desktop.context_menu.y,
                           desktop.context_menu.width, desktop.context_menu.height);
        return false;
    }

    uint32_t mx = desktop.context_menu.x;
    uint32_t my = desktop.context_menu.y;
    uint32_t iy = my + 4;

    for (uint32_t i = 0; i < desktop.context_menu.item_count; i++) {
        menu_item_t* item = &desktop.context_menu.items[i];
        if (item->separator) { iy += 8; continue; }

        uint32_t item_h = 30;
        if (y >= iy - 2 && y < iy - 2 + item_h) {
            if (item->callback) item->callback();
            desktop.context_menu.visible = false;
            wm_invalidate_rect(desktop.context_menu.x, desktop.context_menu.y,
                               desktop.context_menu.width, desktop.context_menu.height);
            return true;
        }
        iy += item_h;
    }
    desktop.context_menu.visible = false;
    wm_invalidate_rect(desktop.context_menu.x, desktop.context_menu.y,
                       desktop.context_menu.width, desktop.context_menu.height);
    return true;
}

/* ============================================================
 *  Cursor
 * ============================================================ */

void cursor_set_type(cursor_type_t type) {
    if (desktop.cursor_type != type) {
        desktop.cursor_type = type;
        wm_invalidate_cursor_cache();
    }
}

void cursor_paint(uint32_t x, uint32_t y, cursor_type_t type) {
    const uint32_t* bmp = NULL;
    int32_t hot_x = 0, hot_y = 0;
    const int32_t center_hot = (int32_t)CURSOR_DRAW_SIZE / 2;

    switch (type) {
        case CURSOR_DEFAULT:  bmp = cursor_default; hot_x = 0; hot_y = 0; break;
        case CURSOR_TEXT:     bmp = cursor_text; hot_x = center_hot; hot_y = center_hot; break;
        case CURSOR_BUSY:     bmp = cursor_busy; hot_x = center_hot; hot_y = center_hot; break;
        case CURSOR_LINK:     bmp = cursor_link; hot_x = 12; hot_y = 3; break;
        case CURSOR_MOVE:     bmp = cursor_move; hot_x = center_hot; hot_y = center_hot; break;
        case CURSOR_RESIZE_NS: bmp = cursor_resize_ns; hot_x = center_hot; hot_y = center_hot; break;
        case CURSOR_RESIZE_EW: bmp = cursor_resize_ew; hot_x = center_hot; hot_y = center_hot; break;
        case CURSOR_RESIZE_NESW: bmp = cursor_resize_nesw; hot_x = center_hot; hot_y = center_hot; break;
        case CURSOR_RESIZE_NWSE: bmp = cursor_resize_nwse; hot_x = center_hot; hot_y = center_hot; break;
        case CURSOR_HELP:     bmp = cursor_help; hot_x = 0; hot_y = 0; break;
        case CURSOR_UNAVAILABLE: bmp = cursor_unavailable; hot_x = center_hot; hot_y = center_hot; break;
        case CURSOR_PRECISION: bmp = cursor_precision; hot_x = center_hot; hot_y = center_hot; break;
        default: bmp = cursor_default; hot_x = 0; hot_y = 0; break;
    }

    if (bmp) {
        int32_t dx = (int32_t)x - hot_x;
        int32_t dy = (int32_t)y - hot_y;
        if (dx < 0) dx = 0;
        if (dy < 0) dy = 0;
        fb_blit_scaled((uint32_t)dx, (uint32_t)dy,
                       CURSOR_DRAW_SIZE, CURSOR_DRAW_SIZE,
                       bmp, CURSOR_BITMAP_SIZE, CURSOR_BITMAP_SIZE);
    }
}
