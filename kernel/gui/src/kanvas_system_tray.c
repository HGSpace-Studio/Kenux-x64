#include "kanvas_system_tray.h"
#include "kanvas_animator.h"
#include "kapi.h"
#include <string.h>

static uint32_t tray_col32(kui_color_t c)
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
        int dx = (int)__builtin_sqrtf((float)(r * r - dy * dy));
        int cx1 = x + r - dx, cx2 = x + w - r + dx;
        int ry1 = y + r - dy - 1, ry2 = y - r + h + dy;
        fill_rect(fb, stride, fw, fh, cx1, ry1, cx2 - cx1, 1, color);
        fill_rect(fb, stride, fw, fh, cx1, ry2, cx2 - cx1, 1, color);
    }
}

static void draw_icon_glyph(uint32_t* fb, int stride, int fw, int fh, int cx, int cy, int sz, kanvas_tray_ind_id_t type, uint32_t color, kanvas_system_tray_t* tray)
{
    (void)sz;
    switch (type) {
    case KANVAS_IND_NETWORK:
        if (tray->network.net_state == KANVAS_NET_OFF) {
            kui_draw_text(fb, stride, fw, fh, cx - 4, cy - 6, "X", 0x808080FF, 12, 0);
        } else if (tray->network.net_state == KANVAS_NET_WIFI) {
            for (int i = 0; i < 3; i++) {
                int r = 4 + i * 4;
                int arc_y = cy - r;
                for (int a = -30 - i * 15; a <= 30 + i * 15; a++) {
                    float rad = (float)a * 3.14159265f / 180.0f;
                    int px = cx + (int)(r * __builtin_sinf(rad));
                    int py = arc_y + (int)(r * __builtin_cosf(rad));
                    if (px >= 0 && px < fw && py >= 0 && py < fh) {
                        uint32_t* p = (uint32_t*)((uint8_t*)fb + py * stride);
                        p[px] = color;
                    }
                }
            }
        } else {
            kui_draw_text(fb, stride, fw, fh, cx - 5, cy - 6, "E", color, 12, 0);
        }
        break;
    case KANVAS_IND_BLUETOOTH:
        if (tray->bluetooth.bt_state != KANVAS_BT_OFF) {
            kui_draw_text(fb, stride, fw, fh, cx - 4, cy - 6, "B", color, 12, 0);
        } else {
            kui_draw_text(fb, stride, fw, fh, cx - 4, cy - 6, "B", 0x606060FF, 12, 0);
        }
        break;
    case KANVAS_IND_VOLUME:
        if (tray->volume.muted || tray->volume.volume == 0) {
            kui_draw_text(fb, stride, fw, fh, cx - 4, cy - 6, "M", 0x808080FF, 12, 0);
        } else {
            int bars = (tray->volume.volume + 32) / 33;
            for (int i = 0; i < bars; i++) {
                int bh = 4 + i * 4;
                fill_rect(fb, stride, fw, fh, cx - 6 + i * 5, cy + 6 - bh, 3, bh, color);
            }
        }
        break;
    case KANVAS_IND_BRIGHTNESS:
        {
            int rays = 6;
            for (int i = 0; i < rays; i++) {
                float angle = (float)i * 360.0f / rays * 3.14159265f / 180.0f;
                int x1 = cx + (int)(4 * __builtin_cosf(angle));
                int y1 = cy + (int)(4 * __builtin_sinf(angle));
                int x2 = cx + (int)(8 * __builtin_cosf(angle));
                int y2 = cy + (int)(8 * __builtin_sinf(angle));
                fill_rect(fb, stride, fw, fh, x1, y1, 1, 1, color);
                fill_rect(fb, stride, fw, fh, x2, y2, 1, 1, color);
            }
            fill_rect(fb, stride, fw, fh, cx - 2, cy - 2, 5, 5, color);
        }
        break;
    case KANVAS_IND_BATTERY:
        {
            int bw = 16, bh = 10;
            int bx = cx - bw / 2, by = cy - bh / 2;
            fill_rect(fb, stride, fw, fh, bx, by, bw, bh, 0x404040FF);
            fill_rect(fb, stride, fw, fh, bx + bw, by + 2, 2, bh - 4, 0x404040FF);
            int fill_w = (int)((float)(bw - 2) * (float)tray->battery.percentage / 100.0f);
            uint32_t bat_col = tray->battery.charging ? 0x40C040FF : (tray->battery.percentage < 20 ? 0xE04040FF : color);
            if (fill_w > 0) fill_rect(fb, stride, fw, fh, bx + 1, by + 1, fill_w, bh - 2, bat_col);
        }
        break;
    default:
        break;
    }
}

kanvas_system_tray_t* kanvas_system_tray_create(void)
{
    kanvas_system_tray_t* t = (kanvas_system_tray_t*)kapi_malloc(sizeof(kanvas_system_tray_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(kanvas_system_tray_t));
    t->visible = true;
    t->bg_color = (kui_color_t){30, 30, 30, 240};
    t->fg_color = (kui_color_t){230, 230, 230, 255};
    t->accent_color = (kui_color_t){98, 0, 238, 255};
    t->hover_color = (kui_color_t){50, 50, 50, 255};
    t->hovered_indicator = -1;
    t->popup_visible = false;
    t->network.net_state = KANVAS_NET_WIFI;
    memcpy(t->network.wifi_ssid, "KenuxNet", 9);
    t->network.wifi_signal = 80;
    t->bluetooth.bt_state = KANVAS_BT_OFF;
    t->volume.volume = 75;
    t->volume.muted = false;
    t->volume.mic_volume = 80;
    t->volume.mic_muted = false;
    t->brightness.brightness = 80;
    t->brightness.min_brightness = 10;
    t->brightness.max_brightness = 100;
    t->brightness.auto_brightness = true;
    t->battery.percentage = 85;
    t->battery.charging = true;
    return t;
}

void kanvas_system_tray_destroy(kanvas_system_tray_t* tray)
{
    if (!tray) return;
    kapi_free(tray);
}

void kanvas_system_tray_paint(kanvas_system_tray_t* tray, uint32_t* fb, int stride, int fw, int fh, int tray_x, int tray_y)
{
    if (!tray || !fb || !tray->visible) return;

    uint32_t fg = tray_col32(tray->fg_color);
    uint32_t hover = tray_col32(tray->hover_color);
    int x = tray_x;
    int y = tray_y;
    int icon_sz = KANVAS_TRAY_ICON_SZ;
    int spacing = KANVAS_TRAY_SPACING;

    kanvas_tray_ind_id_t indicators[] = {
        KANVAS_IND_NETWORK, KANVAS_IND_BLUETOOTH, KANVAS_IND_VOLUME,
        KANVAS_IND_BRIGHTNESS, KANVAS_IND_BATTERY
    };
    int ind_count = 5;

    for (int i = 0; i < ind_count; i++) {
        int ix = x - icon_sz - spacing;
        int iy = y;
        if (i == tray->hovered_indicator) {
            fill_rounded_rect(fb, stride, fw, fh, ix - 2, iy - 2, icon_sz + 4, icon_sz + 4, 4, hover);
        }
        draw_icon_glyph(fb, stride, fw, fh, ix + icon_sz / 2, iy + icon_sz / 2, icon_sz, indicators[i], fg, tray);
        x = ix;
    }

    tray->x = x;
    tray->y = y;
}

void kanvas_system_tray_paint_popup(kanvas_system_tray_t* tray, uint32_t* fb, int stride, int fw, int fh)
{
    if (!tray || !fb || !tray->popup_visible) return;

    uint32_t bg = tray_col32(tray->bg_color);
    uint32_t fg = tray_col32(tray->fg_color);
    uint32_t accent = tray_col32(tray->accent_color);
    int px = tray->popup_x, py = tray->popup_y;
    int pw = KANVAS_TRAY_POPUP_W, ph = 0;

    switch (tray->popup_type) {
    case KANVAS_IND_NETWORK: ph = 180; break;
    case KANVAS_IND_BLUETOOTH: ph = 120; break;
    case KANVAS_IND_VOLUME: ph = 140; break;
    case KANVAS_IND_BRIGHTNESS: ph = 120; break;
    case KANVAS_IND_BATTERY: ph = 100; break;
    default: return;
    }

    fill_rounded_rect(fb, stride, fw, fh, px, py, pw, ph, KANVAS_TRAY_POPUP_R, bg);
    fill_rect(fb, stride, fw, fh, px + 1, py + 1, pw - 2, 1, 0x404040FF);

    int content_y = py + 16;

    switch (tray->popup_type) {
    case KANVAS_IND_NETWORK:
        {
            const char* title = tray->network.net_state == KANVAS_NET_WIFI ? "Wi-Fi Connected" : "Wi-Fi Off";
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, title, fg, 14, 0);
            content_y += 24;
            if (tray->network.net_state == KANVAS_NET_WIFI) {
                kui_draw_text(fb, stride, fw, fh, px + 16, content_y, tray->network.wifi_ssid, accent, 13, 0);
                content_y += 20;
                char sig_text[32] = "Signal: ";
                int sig = tray->network.wifi_signal;
                int pos = 8;
                if (sig == 0) { sig_text[pos++] = '0'; }
                else { char tmp[8]; int ti = 0; while (sig > 0) { tmp[ti++] = '0' + (sig % 10); sig /= 10; } while (ti > 0) sig_text[pos++] = tmp[--ti]; }
                sig_text[pos++] = '%'; sig_text[pos] = '\0';
                kui_draw_text(fb, stride, fw, fh, px + 16, content_y, sig_text, 0x808080FF, 12, 0);
                content_y += 24;
            }
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, "Available Networks:", 0x808080FF, 12, 0);
            content_y += 18;
            for (int i = 0; i < tray->network.wifi_network_count && i < 4; i++) {
                kui_draw_text(fb, stride, fw, fh, px + 24, content_y, tray->network.wifi_networks[i], 0xA0A0A0FF, 12, 0);
                content_y += 16;
            }
        }
        break;
    case KANVAS_IND_BLUETOOTH:
        {
            const char* title = tray->bluetooth.bt_state == KANVAS_BT_OFF ? "Bluetooth Off" : "Bluetooth On";
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, title, fg, 14, 0);
            content_y += 24;
            if (tray->bluetooth.bt_state >= KANVAS_BT_CONNECTED) {
                kui_draw_text(fb, stride, fw, fh, px + 16, content_y, "Connected:", 0x808080FF, 12, 0);
                content_y += 18;
                kui_draw_text(fb, stride, fw, fh, px + 24, content_y, tray->bluetooth.paired_device, accent, 12, 0);
            }
        }
        break;
    case KANVAS_IND_VOLUME:
        {
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, "Volume", fg, 14, 0);
            content_y += 28;
            int bar_x = px + 16, bar_w = pw - 32;
            fill_rounded_rect(fb, stride, fw, fh, bar_x, content_y, bar_w, 6, 3, 0x404040FF);
            int fill_w = (int)((float)bar_w * (float)tray->volume.volume / 100.0f);
            if (fill_w > 0) fill_rounded_rect(fb, stride, fw, fh, bar_x, content_y, fill_w, 6, 3, accent);
            content_y += 20;
            char vol_text[16] = "Vol: ";
            int v = tray->volume.muted ? 0 : tray->volume.volume;
            int pos = 5;
            if (v == 0) { vol_text[pos++] = '0'; }
            else { char tmp[8]; int ti = 0; while (v > 0) { tmp[ti++] = '0' + (v % 10); v /= 10; } while (ti > 0) vol_text[pos++] = tmp[--ti]; }
            vol_text[pos++] = '%'; vol_text[pos] = '\0';
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, vol_text, 0x808080FF, 12, 0);
            content_y += 24;
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, "Microphone", fg, 13, 0);
            content_y += 20;
            fill_rounded_rect(fb, stride, fw, fh, bar_x, content_y, bar_w, 6, 3, 0x404040FF);
            fill_w = (int)((float)bar_w * (float)tray->volume.mic_volume / 100.0f);
            if (fill_w > 0) fill_rounded_rect(fb, stride, fw, fh, bar_x, content_y, fill_w, 6, 3, 0x40C040FF);
        }
        break;
    case KANVAS_IND_BRIGHTNESS:
        {
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, "Brightness", fg, 14, 0);
            content_y += 28;
            int bar_x = px + 16, bar_w = pw - 32;
            fill_rounded_rect(fb, stride, fw, fh, bar_x, content_y, bar_w, 6, 3, 0x404040FF);
            int fill_w = (int)((float)bar_w * (float)tray->brightness.brightness / (float)tray->brightness.max_brightness);
            if (fill_w > 0) fill_rounded_rect(fb, stride, fw, fh, bar_x, content_y, fill_w, 6, 3, 0xFFC040FF);
            content_y += 20;
            const char* auto_str = tray->brightness.auto_brightness ? "Auto: On" : "Auto: Off";
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, auto_str, 0x808080FF, 12, 0);
            content_y += 18;
            const char* night_str = tray->brightness.night_mode ? "Night Mode: On" : "Night Mode: Off";
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, night_str, 0x808080FF, 12, 0);
        }
        break;
    case KANVAS_IND_BATTERY:
        {
            const char* title = tray->battery.charging ? "Battery Charging" : "Battery";
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, title, fg, 14, 0);
            content_y += 24;
            char pct_text[16] = "Level: ";
            int v = tray->battery.percentage;
            int pos = 7;
            if (v == 0) { pct_text[pos++] = '0'; }
            else { char tmp[8]; int ti = 0; while (v > 0) { tmp[ti++] = '0' + (v % 10); v /= 10; } while (ti > 0) pct_text[pos++] = tmp[--ti]; }
            pct_text[pos++] = '%'; pct_text[pos] = '\0';
            kui_draw_text(fb, stride, fw, fh, px + 16, content_y, pct_text, accent, 13, 0);
        }
        break;
    default:
        break;
    }
}

void kanvas_system_tray_handle_mouse(kanvas_system_tray_t* tray, int mx, int my, bool left_down, bool left_up)
{
    if (!tray || !tray->visible) return;
    (void)mx; (void)my; (void)left_down; (void)left_up;
}

void kanvas_system_tray_update(kanvas_system_tray_t* tray, uint64_t now_ms)
{
    if (!tray) return;
    (void)now_ms;
}

int kanvas_system_tray_get_width(kanvas_system_tray_t* tray)
{
    if (!tray) return 0;
    (void)tray;
    int ind_count = 5;
    return ind_count * (KANVAS_TRAY_ICON_SZ + KANVAS_TRAY_SPACING) + KANVAS_TRAY_PAD;
}

void kanvas_system_tray_set_network(kanvas_system_tray_t* tray, kanvas_net_state_t state, const char* ssid, int signal)
{
    if (!tray) return;
    tray->network.net_state = state;
    if (ssid) { size_t len = strlen(ssid); if (len >= 32) len = 31; memcpy(tray->network.wifi_ssid, ssid, len); tray->network.wifi_ssid[len] = '\0'; }
    tray->network.wifi_signal = signal;
}

void kanvas_system_tray_set_bluetooth(kanvas_system_tray_t* tray, kanvas_bt_state_t state, const char* device)
{
    if (!tray) return;
    tray->bluetooth.bt_state = state;
    if (device) { size_t len = strlen(device); if (len >= 32) len = 31; memcpy(tray->bluetooth.paired_device, device, len); tray->bluetooth.paired_device[len] = '\0'; }
}

void kanvas_system_tray_set_volume(kanvas_system_tray_t* tray, int volume, bool muted)
{
    if (!tray) return;
    tray->volume.volume = volume < 0 ? 0 : (volume > 100 ? 100 : volume);
    tray->volume.muted = muted;
}

void kanvas_system_tray_set_brightness(kanvas_system_tray_t* tray, int brightness)
{
    if (!tray) return;
    tray->brightness.brightness = brightness < tray->brightness.min_brightness ? tray->brightness.min_brightness : (brightness > tray->brightness.max_brightness ? tray->brightness.max_brightness : brightness);
}

void kanvas_system_tray_set_battery(kanvas_system_tray_t* tray, int percentage, bool charging)
{
    if (!tray) return;
    tray->battery.percentage = percentage < 0 ? 0 : (percentage > 100 ? 100 : percentage);
    tray->battery.charging = charging;
}

void kanvas_system_tray_toggle_popup(kanvas_system_tray_t* tray, kanvas_tray_ind_id_t type)
{
    if (!tray) return;
    if (tray->popup_visible && tray->popup_type == type) {
        tray->popup_visible = false;
    } else {
        tray->popup_visible = true;
        tray->popup_type = type;
        tray->anim_id = kanvas_animator_menu_open(tray, tray->popup_x, tray->popup_y);
    }
}

void kanvas_system_tray_close_popup(kanvas_system_tray_t* tray)
{
    if (!tray) return;
    tray->popup_visible = false;
}