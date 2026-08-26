/* ============================================================
 * sysinfo.c - System Information Application
 *
 * Displays OS version, kernel version, CPU model, memory info,
 * display resolution, and kernel feature list.
 * ============================================================ */

#include "sysinfo.h"
#include "widget.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "color.h"
#include "icon.h"
#include "msf.h"
#include "kenux_render.h"
#include "window_manager.h"

static window_t* sysinfo_win = NULL;

static void si_str_copy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void si_str_cat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void si_int_to_str(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int32_t j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

static void si_ip_to_str(uint32_t ip, char* buf) {
    char part[4];
    si_int_to_str((ip >> 24) & 0xFF, buf);
    si_str_cat(buf, ".");
    si_int_to_str((ip >> 16) & 0xFF, part);
    si_str_cat(buf, part);
    si_str_cat(buf, ".");
    si_int_to_str((ip >> 8) & 0xFF, part);
    si_str_cat(buf, part);
    si_str_cat(buf, ".");
    si_int_to_str(ip & 0xFF, part);
    si_str_cat(buf, part);
}

static const char* si_conn_state_text(uint32_t state) {
    switch (state) {
        case KAPI_CONN_STATE_CONNECTED: return "Connected";
        case KAPI_CONN_STATE_READY: return "API ready";
        case KAPI_CONN_STATE_CONNECTING: return "Connecting";
        case KAPI_CONN_STATE_DISCONNECTED: return "Disconnected";
        default: return "Unavailable";
    }
}

static uint32_t si_conn_state_color(uint32_t state) {
    switch (state) {
        case KAPI_CONN_STATE_CONNECTED: return RGB(0x00, 0xFF, 0x88);
        case KAPI_CONN_STATE_READY: return RGB(0x00, 0xD4, 0xFF);
        case KAPI_CONN_STATE_CONNECTING: return RGB(0xFF, 0xD7, 0x00);
        case KAPI_CONN_STATE_DISCONNECTED: return RGB(0xFF, 0x6B, 0x6B);
        default: return RGB(0x80, 0x80, 0x80);
    }
}

/* Draw info rows inside window content area */
static void si_draw_content(void) {
    if (!sysinfo_win) return;

    uint32_t cx, cy, cw, ch;
    window_get_content_rect(sysinfo_win, &cx, &cy, &cw, &ch);

    /* Title bar with icon */
    icon_draw(cx + 8, cy + 4, ICON_INFO, 24);
    font_draw_text(cx + 40, cy + 8, "System Information", msf_settings.accent_color);
    font_draw_text(cx + 40, cy + 24, "KenuxOS Kernel Details",
                   RGB(0xAE, 0xAE, 0xB0));

    /* Divider */
    gfx_draw_hline(cx + 4, cy + 44, cw - 8, RGB(0x33, 0x33, 0x33));

    uint32_t y = cy + 52;
    uint32_t label_x = cx + 12;
    uint32_t value_x = cx + 130;
    uint32_t row_h = 18;

    /* Helper macro-like function for drawing a row */
    /* OS Version */
    font_draw_text(label_x, y, "OS:", RGB(0x80, 0x80, 0x80));
    {
        char buf[48];
        KAPI_System_GetOSVersion(buf, sizeof(buf));
        font_draw_text(value_x, y, buf, RGB(0xFF, 0xFF, 0xFF));
    }
    y += row_h;

    /* Kernel Version */
    font_draw_text(label_x, y, "Kernel:", RGB(0x80, 0x80, 0x80));
    font_draw_text(value_x, y, "KenuxK 4.0 x86_64", RGB(0xFF, 0xFF, 0xFF));
    y += row_h;

    /* CPU Model */
    font_draw_text(label_x, y, "CPU:", RGB(0x80, 0x80, 0x80));
    {
        char buf[48];
        KAPI_System_GetCPUModel(buf, sizeof(buf));
        font_draw_text(value_x, y, buf, RGB(0xFF, 0xFF, 0xFF));
    }
    y += row_h;

    /* CPU Usage */
    font_draw_text(label_x, y, "CPU Usage:", RGB(0x80, 0x80, 0x80));
    {
        char buf[16];
        uint32_t cpu = KAPI_System_GetCPUUsage();
        si_int_to_str(cpu, buf);
        si_str_cat(buf, "%");
        uint32_t color = RGB(0x00, 0xFF, 0x88);
        if (cpu > 50) color = RGB(0xFF, 0x6B, 0x6B);
        else if (cpu > 25) color = RGB(0xFF, 0xD7, 0x00);
        font_draw_text(value_x, y, buf, color);
    }
    y += row_h;

    /* Memory Total */
    font_draw_text(label_x, y, "Memory:", RGB(0x80, 0x80, 0x80));
    {
        char buf[32];
        uint32_t total_kb = KAPI_System_GetMemTotal();
        uint32_t total_mb = total_kb / 1024;
        si_str_copy(buf, "");
        si_int_to_str(total_mb, buf);
        si_str_cat(buf, " MB total");
        font_draw_text(value_x, y, buf, RGB(0xFF, 0xFF, 0xFF));
    }
    y += row_h;

    /* Memory Free */
    font_draw_text(label_x, y, "Mem Free:", RGB(0x80, 0x80, 0x80));
    {
        char buf[32];
        uint32_t free_kb = KAPI_System_GetMemFree();
        uint32_t free_mb = free_kb / 1024;
        si_str_copy(buf, "");
        si_int_to_str(free_mb, buf);
        si_str_cat(buf, " MB free");
        font_draw_text(value_x, y, buf, RGB(0x00, 0xD4, 0xFF));
    }
    y += row_h;

    /* Display Resolution */
    font_draw_text(label_x, y, "Display:", RGB(0x80, 0x80, 0x80));
    {
        char buf[32];
        si_str_copy(buf, "");
        si_int_to_str(fb.width, buf);
        si_str_cat(buf, "x");
        char h[8];
        si_int_to_str(fb.height, h);
        si_str_cat(buf, h);
        si_str_cat(buf, " 32bpp GOP");
        font_draw_text(value_x, y, buf, RGB(0xFF, 0xFF, 0xFF));
    }
    y += row_h;

    /* Boot Mode */
    font_draw_text(label_x, y, "Boot:", RGB(0x80, 0x80, 0x80));
    font_draw_text(value_x, y, "UEFI (OVMF)", RGB(0xFF, 0xFF, 0xFF));
    y += row_h;

    /* Uptime */
    font_draw_text(label_x, y, "Uptime:", RGB(0x80, 0x80, 0x80));
    {
        char buf[16];
        uint32_t up = KAPI_System_Uptime();
        si_int_to_str(up, buf);
        si_str_cat(buf, "s");
        font_draw_text(value_x, y, buf, RGB(0xAE, 0xAE, 0xB0));
    }
    y += row_h + 4;

    /* Divider */
    gfx_draw_hline(cx + 4, y, cw - 8, RGB(0x33, 0x33, 0x33));
    y += 6;

    /* Connectivity section */
    font_draw_text(label_x, y, "Connectivity:", msf_settings.accent_color);
    y += row_h;

    {
        kapi_connectivity_status_t conn;
        if (KAPI_Connectivity_GetStatus(&conn) == KAPI_OK) {
            for (uint32_t i = 0; i < conn.link_count && i < KAPI_CONNECTIVITY_MAX_LINKS; i++) {
                kapi_connectivity_link_t* link = &conn.links[i];
                char buf[96];
                uint32_t color = si_conn_state_color(link->state);

                if (y + 14 > cy + ch - 52) break;

                font_draw_text(label_x + 8, y, link->label, RGB(0x80, 0x80, 0x80));
                si_str_copy(buf, si_conn_state_text(link->state));
                if (link->ip_addr != 0) {
                    char ipbuf[16];
                    si_str_cat(buf, " ");
                    si_ip_to_str(link->ip_addr, ipbuf);
                    si_str_cat(buf, ipbuf);
                }
                font_draw_text(value_x, y, buf, color);
                y += 14;
            }

            if (y + 14 <= cy + ch - 52) {
                font_draw_text(label_x + 8, y, "Time Sync", RGB(0x80, 0x80, 0x80));
                font_draw_text(value_x, y,
                    conn.ntp_synced ? "NTP synced" : "NTP API ready / waiting",
                    conn.ntp_synced ? RGB(0x00, 0xFF, 0x88) : RGB(0x00, 0xD4, 0xFF));
                y += 14;
            }
        } else {
            font_draw_text(label_x + 8, y, "Connectivity API unavailable", RGB(0xFF, 0x6B, 0x6B));
            y += 14;
        }
    }

    y += 4;
    gfx_draw_hline(cx + 4, y, cw - 8, RGB(0x33, 0x33, 0x33));
    y += 6;

    /* Features section */
    font_draw_text(label_x, y, "Kernel Features:", msf_settings.accent_color);
    y += row_h;

    const char* features[] = {
        "x86_64 long mode (4-level paging)",
        "GDT/IDT with 256 interrupt gates",
        "Ext2/Ext4 + JBD2 journaling",
        "TCP/IP + connectivity KAPI",
        "PS/2 + Bluetooth HID status API",
        "UEFI GOP framebuffer GUI",
        "Wi-Fi/Bluetooth/NTP GUI status",
        "CJK + Emoji bitmap fonts",
    };
    uint32_t feat_count = 8;
    for (uint32_t i = 0; i < feat_count; i++) {
        if (y + 14 > cy + ch - 4) break;
        font_draw_text(label_x + 8, y, "- ", RGB(0x00, 0xD4, 0xFF));
        font_draw_text(label_x + 20, y, features[i], RGB(0xAE, 0xAE, 0xB0));
        y += 14;
    }
    window_paint_widgets(sysinfo_win);
}

/* Refresh button */
static void si_refresh_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    if (sysinfo_win && sysinfo_win->visible) {
        window_paint(sysinfo_win);
        si_draw_content();
    }
}

/* Close button */
static void si_close_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    if (sysinfo_win) {
        sysinfo_win->visible = false;
        wm_invalidate_window(sysinfo_win);
        wm_flush_dirty();
    }
}

/* Content click — click anywhere in content to refresh info */
static void si_content_click(window_t* win, uint32_t x, uint32_t y, void* user_data) {
    (void)win; (void)user_data; (void)x; (void)y;
    if (sysinfo_win && sysinfo_win->visible) {
        window_paint(sysinfo_win);
        si_draw_content();
        wm_invalidate_window(sysinfo_win);
    }
}

window_t* sysinfo_create(void) {
    if (sysinfo_win) return sysinfo_win;

    sysinfo_win = window_create(200, 50, 430, 460, "System Info");
    sysinfo_win->titlebar_color = msf_settings.titlebar_active;
    sysinfo_win->visible = false;
    sysinfo_win->on_content_click = si_content_click;
    window_set_statusbar(sysinfo_win, "KenuxOS 4.0");

    /* Refresh button at bottom */
    widget_t* btn_refresh = widget_create_button(10, 410, 80, 24,
        "Refresh", si_refresh_click, NULL);
    window_add_widget(sysinfo_win, btn_refresh);

    /* Close button */
    widget_t* btn_close = widget_create_button(100, 410, 80, 24,
        "Close", si_close_click, NULL);
    window_add_widget(sysinfo_win, btn_close);

    wm_add_window(sysinfo_win);
    return sysinfo_win;
}

/* Called when window is shown to draw content */
void sysinfo_on_show(void) {
    if (sysinfo_win && sysinfo_win->visible) {
        window_paint(sysinfo_win);
        si_draw_content();
    }
}
