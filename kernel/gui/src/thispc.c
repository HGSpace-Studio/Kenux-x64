/* ============================================================
 * thispc.c - This PC Application
 *
 * Shows drives, devices, and storage usage in a file-explorer
 * style interface with sidebar navigation.
 * ============================================================ */

#include "thispc.h"
#include "widget.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "color.h"
#include "icon.h"
#include "msf.h"
#include "kenux_render.h"
#include "window_manager.h"

static window_t* thispc_win = NULL;
static int32_t pc_selected_drive = -1;

/* Drive entries loaded from KAPI */
typedef struct {
    char name[24];
    char type[12];
    uint64_t total_bytes;
    uint64_t used_bytes;
    icon_id_t icon;
} drive_entry_t;

#define PC_MAX_DRIVES 8
static drive_entry_t s_drives[PC_MAX_DRIVES];
static uint32_t s_drive_count = 0;

static void pc_str_copy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void pc_str_cat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void pc_int_to_str(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int32_t j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

static void pc_load_drives(void) {
    s_drive_count = 0;

    uint32_t kapi_count = KAPI_Disk_GetCount();
    if (kapi_count > PC_MAX_DRIVES) kapi_count = PC_MAX_DRIVES;

    kapi_disk_info_t disks[PC_MAX_DRIVES];
    if (kapi_count > 0) {
        int32_t ret = KAPI_Disk_GetList(disks, kapi_count);
        if (ret == KAPI_OK) {
            for (uint32_t i = 0; i < kapi_count && s_drive_count < PC_MAX_DRIVES; i++) {
                drive_entry_t* d = &s_drives[s_drive_count];
                pc_str_copy(d->name, disks[i].name);
                d->total_bytes = disks[i].capacity_bytes;
                d->used_bytes = d->total_bytes / 2;
                if (disks[i].readonly) {
                    pc_str_copy(d->type, "Read-Only");
                } else {
                    pc_str_copy(d->type, "Disk");
                }
                d->icon = (s_drive_count == 0) ? ICON_COMPUTER : ICON_FOLDER;
                s_drive_count++;
            }
        }
    }

    if (s_drive_count == 0) {
        drive_entry_t* d = &s_drives[0];
        pc_str_copy(d->name, "Kenux Boot");
        pc_str_copy(d->type, "EFI Disk");
        d->total_bytes = 512 * 1024 * 1024ULL;
        d->used_bytes = 128 * 1024 * 1024ULL;
        d->icon = ICON_COMPUTER;
        s_drive_count = 1;
    }
}

/* Format bytes to human-readable string */
static void pc_format_size(uint64_t bytes, char* buf) {
    if (bytes >= 1024ULL * 1024 * 1024) {
        uint64_t gb = bytes / (1024 * 1024 * 1024);
        uint64_t gb_frac = (bytes % (1024 * 1024 * 1024)) * 10 / (1024 * 1024 * 1024);
        pc_int_to_str((uint32_t)gb, buf);
        char frac[4];
        frac[0] = '.';
        frac[1] = '0' + (uint8_t)gb_frac;
        frac[2] = '\0';
        pc_str_cat(buf, frac);
        pc_str_cat(buf, " GB");
    } else if (bytes >= 1024 * 1024) {
        uint32_t mb = (uint32_t)(bytes / (1024 * 1024));
        pc_int_to_str(mb, buf);
        pc_str_cat(buf, " MB");
    } else if (bytes >= 1024) {
        uint32_t kb = (uint32_t)(bytes / 1024);
        pc_int_to_str(kb, buf);
        pc_str_cat(buf, " KB");
    } else {
        pc_int_to_str((uint32_t)bytes, buf);
        pc_str_cat(buf, " B");
    }
}

/* Draw drive list and storage bars */
static void pc_draw_content(void) {
    if (!thispc_win) return;

    uint32_t cx, cy, cw, ch;
    window_get_content_rect(thispc_win, &cx, &cy, &cw, &ch);

    /* Sidebar */
    uint32_t sidebar_w = 100;
    fb_fill_rect(cx, cy, sidebar_w, ch, RGB(0x1A, 0x1A, 0x1A));
    gfx_draw_vline(cx + sidebar_w, cy, ch, RGB(0x33, 0x33, 0x33));

    /* Sidebar items */
    const char* sidebar_items[] = { "Quick access", "This PC", "Network", "Home" };
    uint32_t sb_count = 4;
    for (uint32_t i = 0; i < sb_count; i++) {
        uint32_t sy = cy + 4 + i * 20;
        if (i == 1) {
            /* Highlight "This PC" */
            fb_fill_rect(cx + 2, sy, sidebar_w - 4, 18, RGB(0x33, 0x33, 0x33));
            font_draw_text(cx + 8, sy + 3, sidebar_items[i], RGB(0x00, 0xD4, 0xFF));
        } else {
            font_draw_text(cx + 8, sy + 3, sidebar_items[i], RGB(0xAE, 0xAE, 0xB0));
        }
    }

    /* Main content area */
    uint32_t mx = cx + sidebar_w + 4;
    uint32_t mw = cw - sidebar_w - 8;

    /* Title */
    font_draw_text(mx, cy + 4, "Devices and drives", RGB(0xFF, 0xFF, 0xFF));

    /* Drive list */
    uint32_t dy = cy + 24;
    for (uint32_t i = 0; i < s_drive_count; i++) {
        uint32_t row_h = 56;
        if (dy + row_h > cy + ch) break;

        /* Row background */
        uint32_t row_bg = RGB(0x1E, 0x1E, 0x1E);
        if ((int32_t)i == pc_selected_drive) {
            row_bg = RGB(0x2A, 0x3A, 0x5C);
        }
        fb_fill_rect(mx, dy, mw, row_h - 4, row_bg);
        gfx_draw_rect(mx, dy, mw, row_h - 4, RGB(0x33, 0x33, 0x33));

        /* Drive icon */
        icon_draw(mx + 6, dy + 6, s_drives[i].icon, 24);

        /* Drive name */
        font_draw_text(mx + 38, dy + 6, s_drives[i].name, RGB(0xFF, 0xFF, 0xFF));

        /* Drive type */
        font_draw_text(mx + 38, dy + 22, s_drives[i].type, RGB(0x80, 0x80, 0x80));

        /* Size info */
        char size_str[32];
        pc_str_copy(size_str, "Total: ");
        char num[16];
        pc_format_size(s_drives[i].total_bytes, num);
        pc_str_cat(size_str, num);
        font_draw_text(mx + 38, dy + 38, size_str, RGB(0xAE, 0xAE, 0xB0));

        /* Usage bar */
        uint32_t bar_x = mx + 200;
        uint32_t bar_w = mw > 210 ? mw - 210 : 10;
        uint32_t bar_y = dy + 20;
        uint32_t bar_h = 8;

        fb_fill_rect(bar_x, bar_y, bar_w, bar_h, RGB(0x33, 0x33, 0x33));
        uint32_t pct = 0;
        if (s_drives[i].total_bytes > 0) {
            pct = (uint32_t)((s_drives[i].used_bytes * 100) / s_drives[i].total_bytes);
        }
        uint32_t fill_w = (bar_w * pct) / 100;

        uint32_t bar_color = RGB(0x00, 0xD4, 0xFF);
        if (pct > 80) bar_color = RGB(0xFF, 0x6B, 0x6B);
        else if (pct > 60) bar_color = RGB(0xFF, 0xD7, 0x00);

        if (fill_w > 0) {
            fb_fill_rect(bar_x, bar_y, fill_w, bar_h, bar_color);
        }

        /* Usage text */
        char usage_str[32];
        pc_str_copy(usage_str, "");
        pc_format_size(s_drives[i].used_bytes, usage_str);
        pc_str_cat(usage_str, " / ");
        char total_str[16];
        pc_format_size(s_drives[i].total_bytes, total_str);
        pc_str_cat(usage_str, total_str);
        font_draw_text(bar_x, bar_y + 12, usage_str, RGB(0xAE, 0xAE, 0xB0));

        /* Percentage */
        char pct_str[8];
        pc_int_to_str(pct, pct_str);
        pc_str_cat(pct_str, "%");
        font_draw_text(bar_x + bar_w - 24, bar_y + 12, pct_str, bar_color);

        dy += row_h;
    }

    /* Bottom info bar */
    uint32_t info_y = cy + ch - 20;
    fb_fill_rect(cx, info_y, cw, 20, RGB(0x1A, 0x1A, 0x1A));
    font_draw_text(cx + 4, info_y + 3, "4 drives  |  Total: 105 GB  |  Free: 48 GB",
                   RGB(0x80, 0x80, 0x80));
    window_paint_widgets(thispc_win);
}

/* Refresh button */
static void pc_refresh_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    if (thispc_win && thispc_win->visible) {
        pc_draw_content();
    }
}

/* Content click handler — sidebar items are clickable */
static void pc_content_click(window_t* win, uint32_t x, uint32_t y, void* user_data) {
    (void)win; (void)user_data;
    if (!thispc_win || !thispc_win->visible) return;

    /* Sidebar area (x < 100) */
    if (x < 100) {
        /* Sidebar items at y = 4, 24, 44, 64 (each 18px high) */
        uint32_t sb_y = 4;
        for (uint32_t i = 0; i < 4; i++) {
            uint32_t iy = sb_y + i * 20;
            if (y >= iy && y < iy + 18) {
                pc_draw_content();
                return;
            }
        }
        return;
    }

    /* Drive entries: starting at y=24, each 56px high */
    uint32_t drive_y = 24;
    for (uint32_t i = 0; i < s_drive_count; i++) {
        uint32_t ry = drive_y + i * 56;
        if (y >= ry && y < ry + 52) {
            pc_selected_drive = (int32_t)i;
            pc_draw_content();
            return;
        }
    }
}

window_t* thispc_create(void) {
    if (thispc_win) return thispc_win;

    thispc_win = window_create(80, 40, 460, 340, "This PC");
    thispc_win->titlebar_color = msf_settings.titlebar_active;
    thispc_win->visible = false;
    thispc_win->on_content_click = pc_content_click;
    window_set_statusbar(thispc_win, "4 items");

    /* Refresh button */
    widget_t* btn_refresh = widget_create_button(10, 280, 80, 24,
        "Refresh", pc_refresh_click, NULL);
    window_add_widget(thispc_win, btn_refresh);

    wm_add_window(thispc_win);

    /* Draw content on first show */
    return thispc_win;
}

/* Called when window becomes visible to draw content */
void thispc_on_show(void) {
    if (thispc_win && thispc_win->visible) {
        pc_load_drives();
        pc_draw_content();
    }
}
