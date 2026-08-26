#include "compat_center.h"
#include "framebuffer.h"
#include "font.h"
#include "graphics.h"
#include "icon.h"
#include "msf.h"
#include "window_manager.h"
#include "terminal.h"
#include "filemgr.h"

static window_t* compat_win;

static void compat_card(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                        icon_id_t icon, const char* title, const char* status,
                        const char* detail, uint32_t color) {
    fb_fill_rounded_rect(x, y, w, h, 10, RGB(25, 30, 43));
    fb_blend_rect(x + 8, y + 4, w - 16, h / 2, RGB(255, 255, 255), 5);
    icon_draw(x + 14, y + 15, icon, 28);
    font_draw_text(x + 54, y + 12, title, RGB(238, 242, 250));
    fb_fill_rounded_rect(x + 54, y + 35, 8, 8, 4, color);
    font_draw_text(x + 68, y + 31, status, color);
    font_draw_text(x + 14, y + 59, detail, RGB(155, 166, 186));
    gfx_draw_hline(x + 14, y + h - 18, w - 28, RGB(45, 52, 69));
    font_draw_text(x + 14, y + h - 15, "Open integration", RGB(111, 159, 235));
}

static void compat_paint(void) {
    if (!compat_win || !compat_win->visible) return;
    uint32_t cx, cy, cw, ch;
    window_get_content_rect(compat_win, &cx, &cy, &cw, &ch);
    fb_fill_rect(cx, cy, cw, ch, RGB(16, 20, 30));

    icon_draw(cx + 18, cy + 18, ICON_开关开启, 30);
    font_draw_text(cx + 58, cy + 18, "Compatibility Center", RGB(241, 245, 252));
    font_draw_text(cx + 58, cy + 37, "Runtime backends and integration status", RGB(143, 156, 180));
    gfx_draw_hline(cx + 18, cy + 64, cw - 36, RGB(45, 53, 72));

    uint32_t gap = 14;
    uint32_t card_w = (cw - 54) / 2;
    uint32_t card_h = 112;
    uint32_t x1 = cx + 18;
    uint32_t x2 = x1 + card_w + gap;
    uint32_t y1 = cy + 78;
    uint32_t y2 = y1 + card_h + gap;

    compat_card(x1, y1, card_w, card_h, ICON_代码,
                "Linux ELF", "Runtime enabled", "ELF loader, user mode, argv/envp", RGB(64, 209, 142));
    compat_card(x2, y1, card_w, card_h, ICON_显示器,
                "Win32", "Module available", "Fast boot skips subsystem init", RGB(247, 184, 77));
    compat_card(x1, y2, card_w, card_h, ICON_FOLDER,
                "NTFS", "Module available", "Mount path requires a real NTFS device", RGB(93, 164, 255));
    compat_card(x2, y2, card_w, card_h, ICON_GRID,
                "Containers", "Lifecycle demo", "Create/start/stop API; no isolation", RGB(183, 133, 255));

    font_draw_text(cx + 18, cy + ch - 26,
                   "Statuses reflect the current release build; no simulated running state.",
                   RGB(126, 139, 162));
}

static void compat_open_terminal(void) {
    window_t* win = terminal_create();
    if (!win) return;
    win->visible = true;
    win->state.minimized = false;
    wm_set_active(win);
    terminal_on_show();
    wm_invalidate_window(win);
}

static void compat_open_files(void) {
    window_t* win = filemgr_create();
    if (!win) return;
    win->visible = true;
    win->state.minimized = false;
    wm_set_active(win);
    filemgr_on_show();
    wm_invalidate_window(win);
}

static void compat_click(window_t* win, uint32_t x, uint32_t y, void* data) {
    (void)win; (void)data;
    uint32_t cx, cy, cw, ch;
    window_get_content_rect(compat_win, &cx, &cy, &cw, &ch);
    uint32_t gap = 14;
    uint32_t card_w = (cw - 54) / 2;
    uint32_t card_h = 112;
    uint32_t x1 = cx + 18;
    uint32_t x2 = x1 + card_w + gap;
    uint32_t y1 = cy + 78;
    uint32_t y2 = y1 + card_h + gap;
    if (y >= y1 && y < y1 + card_h && ((x >= x1 && x < x1 + card_w) || (x >= x2 && x < x2 + card_w))) {
        compat_open_terminal();
    } else if (x >= x1 && x < x1 + card_w && y >= y2 && y < y2 + card_h) {
        compat_open_files();
    }
}

window_t* compat_center_create(void) {
    if (compat_win) return compat_win;
    compat_win = window_create(170, 80, 650, 390, "Compatibility Center");
    if (!compat_win) return NULL;
    compat_win->visible = false;
    compat_win->titlebar_color = msf_settings.titlebar_active;
    compat_win->on_content_click = compat_click;
    window_set_statusbar(compat_win, "Kenux runtime integrations");
    wm_add_window(compat_win);
    return compat_win;
}

void compat_center_on_show(void) {
    if (!compat_win || !compat_win->visible) return;
    window_paint(compat_win);
    compat_paint();
}
