#include "kanvas_boot_splash.h"
#include "kanvas_animator.h"
#include "kapi.h"
#include <string.h>

static uint32_t splash_col32(kui_color_t c)
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

static void blend_pixel(uint32_t* fb, int stride, int fw, int fh, int x, int y, uint32_t color, uint8_t alpha)
{
    if (x < 0 || x >= fw || y < 0 || y >= fh) return;
    uint32_t* p = (uint32_t*)((uint8_t*)fb + y * stride);
    uint32_t dst = p[x];
    uint8_t ia = 255 - alpha;
    uint8_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    uint8_t sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    p[x] = 0xFF000000u | (((sr * alpha + dr * ia) / 255) << 16) | (((sg * alpha + dg * ia) / 255) << 8) | ((sb * alpha + db * ia) / 255);
}

kanvas_boot_splash_t* kanvas_boot_splash_create(int screen_w, int screen_h)
{
    kanvas_boot_splash_t* s = (kanvas_boot_splash_t*)kapi_malloc(sizeof(kanvas_boot_splash_t));
    if (!s) return NULL;
    memset(s, 0, sizeof(kanvas_boot_splash_t));
    s->state = KENUX_SPLASH_STATE_LOGO;
    s->screen_w = screen_w;
    s->screen_h = screen_h;
    s->progress = 0.0f;
    s->fade_alpha = 1.0f;
    s->active = true;
    s->show_logo = true;
    s->show_progress = true;
    s->show_text = true;
    s->bg_color = (kui_color_t){10, 10, 18, 255};
    s->accent_color = (kui_color_t){98, 0, 238, 255};
    s->text_color = (kui_color_t){200, 200, 210, 255};
    s->bar_bg_color = (kui_color_t){40, 40, 50, 255};
    s->bar_fg_color = (kui_color_t){98, 0, 238, 255};
    memcpy(s->version_text, "Kenux OS v1.0", 14);
    memcpy(s->status_text, "Initializing kernel...", 23);
    s->anim_id = 0;
    return s;
}

void kanvas_boot_splash_destroy(kanvas_boot_splash_t* splash)
{
    if (!splash) return;
    kapi_free(splash);
}

void kanvas_boot_splash_paint(kanvas_boot_splash_t* splash, uint32_t* fb, int stride, int fw, int fh)
{
    if (!splash || !fb || !splash->active) return;

    uint32_t bg = splash_col32(splash->bg_color);
    fill_rect(fb, stride, fw, fh, 0, 0, fw, fh, bg);

    int cx = fw / 2;
    int cy = fh / 2 - 40;

    if (splash->show_logo) {
        int logo_sz = KENUX_SPLASH_LOGO_SIZE;
        int lx = cx - logo_sz / 2;
        int ly = cy - logo_sz / 2;

        uint32_t accent = splash_col32(splash->accent_color);
        int ring_r = logo_sz / 2;
        for (int angle = 0; angle < 360; angle += 2) {
            float rad = (float)angle * 3.14159265f / 180.0f;
            int px = cx + (int)(ring_r * __builtin_cosf(rad));
            int py = cy + (int)(ring_r * __builtin_sinf(rad));
            if (px >= 0 && px < fw && py >= 0 && py < fh) {
                blend_pixel(fb, stride, fw, fh, px, py, accent, 180);
            }
        }

        int inner_r = ring_r - 8;
        fill_rounded_rect(fb, stride, fw, fh, cx - inner_r, cy - inner_r, inner_r * 2, inner_r * 2, 24, 0x1A1A2EFF);

        kui_draw_text(fb, stride, fw, fh, cx - 30, cy - 14, "K", accent, 36, 1);
    }

    if (splash->show_text) {
        uint32_t text_col = splash_col32(splash->text_color);
        kui_draw_text(fb, stride, fw, fh, cx - 52, cy + KENUX_SPLASH_LOGO_SIZE / 2 + 16, splash->version_text, text_col, 16, 0);

        uint32_t status_col = 0x808090FF;
        kui_draw_text(fb, stride, fw, fh, cx - 80, cy + KENUX_SPLASH_LOGO_SIZE / 2 + 40, splash->status_text, status_col, 12, 0);
    }

    if (splash->show_progress) {
        int bar_x = cx - KENUX_SPLASH_BAR_W / 2;
        int bar_y = cy + KENUX_SPLASH_LOGO_SIZE / 2 + 64;
        uint32_t bar_bg = splash_col32(splash->bar_bg_color);
        uint32_t bar_fg = splash_col32(splash->bar_fg_color);

        fill_rounded_rect(fb, stride, fw, fh, bar_x, bar_y, KENUX_SPLASH_BAR_W, KENUX_SPLASH_BAR_H, KENUX_SPLASH_BAR_RADIUS, bar_bg);

        int fill_w = (int)((float)KENUX_SPLASH_BAR_W * splash->progress);
        if (fill_w > 0) {
            fill_rounded_rect(fb, stride, fw, fh, bar_x, bar_y, fill_w, KENUX_SPLASH_BAR_H, KENUX_SPLASH_BAR_RADIUS, bar_fg);
        }

        int dot_count = 3;
        int dot_phase = (int)(splash->now_ms / 400) % (dot_count + 1);
        for (int d = 0; d < dot_count; d++) {
            int dot_x = bar_x + KENUX_SPLASH_BAR_W + 16 + d * 10;
            int dot_y = bar_y;
            uint8_t dot_alpha = (d < dot_phase) ? 200 : 60;
            blend_pixel(fb, stride, fw, fh, dot_x, dot_y, bar_fg, dot_alpha);
        }
    }

    if (splash->state == KENUX_SPLASH_STATE_FADE_OUT) {
        uint8_t fade = (uint8_t)(255.0f * (1.0f - splash->fade_alpha));
        for (int y = 0; y < fh; y++) {
            uint32_t* p = (uint32_t*)((uint8_t*)fb + y * stride);
            for (int x = 0; x < fw; x++) {
                uint32_t c = p[x];
                uint8_t a = (c >> 24) & 0xFF;
                p[x] = ((uint32_t)((a * fade) / 255) << 24) | (c & 0x00FFFFFF);
            }
        }
    }
}

void kanvas_boot_splash_update(kanvas_boot_splash_t* splash, uint64_t now_ms)
{
    if (!splash || !splash->active) return;

    if (splash->start_ms == 0) splash->start_ms = now_ms;
    splash->now_ms = now_ms;

    uint64_t elapsed = now_ms - splash->start_ms;

    switch (splash->state) {
    case KENUX_SPLASH_STATE_LOGO:
        if (elapsed > 800) {
            splash->state = KENUX_SPLASH_STATE_LOADING;
            memcpy(splash->status_text, "Loading system services...", 26);
        }
        break;
    case KENUX_SPLASH_STATE_LOADING:
        splash->progress = (float)elapsed / (float)KENUX_SPLASH_DURATION_MS;
        if (splash->progress > 1.0f) splash->progress = 1.0f;

        if (splash->progress < 0.3f)
            memcpy(splash->status_text, "Loading kernel modules...", 25);
        else if (splash->progress < 0.5f)
            memcpy(splash->status_text, "Starting system services...", 27);
        else if (splash->progress < 0.7f)
            memcpy(splash->status_text, "Initializing GUI subsystem...", 29);
        else if (splash->progress < 0.9f)
            memcpy(splash->status_text, "Loading desktop environment...", 29);
        else
            memcpy(splash->status_text, "Starting Kenux Desktop...", 25);

        if (elapsed >= KENUX_SPLASH_DURATION_MS) {
            splash->state = KENUX_SPLASH_STATE_FADE_OUT;
            splash->fade_alpha = 1.0f;
        }
        break;
    case KENUX_SPLASH_STATE_FADE_OUT:
        {
            uint64_t fade_elapsed = elapsed - KENUX_SPLASH_DURATION_MS;
            splash->fade_alpha = 1.0f - (float)fade_elapsed / (float)KENUX_SPLASH_FADE_MS;
            if (splash->fade_alpha <= 0.0f) {
                splash->fade_alpha = 0.0f;
                splash->state = KENUX_SPLASH_STATE_DONE;
                splash->active = false;
            }
        }
        break;
    default:
        break;
    }
}

void kanvas_boot_splash_set_progress(kanvas_boot_splash_t* splash, float progress)
{
    if (!splash) return;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    splash->progress = progress;
}

void kanvas_boot_splash_set_status(kanvas_boot_splash_t* splash, const char* text)
{
    if (!splash || !text) return;
    size_t len = strlen(text);
    if (len >= 64) len = 63;
    memcpy(splash->status_text, text, len);
    splash->status_text[len] = '\0';
}

bool kanvas_boot_splash_is_done(kanvas_boot_splash_t* splash)
{
    if (!splash) return true;
    return splash->state == KENUX_SPLASH_STATE_DONE;
}

void kanvas_boot_splash_skip(kanvas_boot_splash_t* splash)
{
    if (!splash) return;
    splash->state = KENUX_SPLASH_STATE_FADE_OUT;
    splash->fade_alpha = 1.0f;
    splash->progress = 1.0f;
    splash->start_ms = splash->now_ms - KENUX_SPLASH_DURATION_MS;
}