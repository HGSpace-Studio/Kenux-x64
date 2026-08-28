#ifndef KANVAS_BOOT_SPLASH_H
#define KANVAS_BOOT_SPLASH_H

#include "kapi_kanvasui.h"
#include <stdint.h>
#include <stdbool.h>

#define KENUX_SPLASH_DURATION_MS    3500
#define KENUX_SPLASH_FADE_MS        500
#define KENUX_SPLASH_LOGO_SIZE      128
#define KENUX_SPLASH_BAR_W          320
#define KENUX_SPLASH_BAR_H          4
#define KENUX_SPLASH_BAR_RADIUS     2

typedef enum {
    KENUX_SPLASH_STATE_LOGO = 0,
    KENUX_SPLASH_STATE_LOADING,
    KENUX_SPLASH_STATE_FADE_OUT,
    KENUX_SPLASH_STATE_DONE
} kenux_splash_state_t;

typedef struct {
    kenux_splash_state_t state;
    uint64_t start_ms;
    uint64_t now_ms;
    int screen_w, screen_h;
    float progress;
    float fade_alpha;
    bool active;
    bool show_logo;
    bool show_progress;
    bool show_text;
    kui_color_t bg_color;
    kui_color_t accent_color;
    kui_color_t text_color;
    kui_color_t bar_bg_color;
    kui_color_t bar_fg_color;
    char version_text[32];
    char status_text[64];
    uint32_t anim_id;
} kanvas_boot_splash_t;

kanvas_boot_splash_t* kanvas_boot_splash_create(int screen_w, int screen_h);
void kanvas_boot_splash_destroy(kanvas_boot_splash_t* splash);

void kanvas_boot_splash_paint(kanvas_boot_splash_t* splash, uint32_t* fb, int stride, int fw, int fh);
void kanvas_boot_splash_update(kanvas_boot_splash_t* splash, uint64_t now_ms);

void kanvas_boot_splash_set_progress(kanvas_boot_splash_t* splash, float progress);
void kanvas_boot_splash_set_status(kanvas_boot_splash_t* splash, const char* text);

bool kanvas_boot_splash_is_done(kanvas_boot_splash_t* splash);
void kanvas_boot_splash_skip(kanvas_boot_splash_t* splash);

#endif