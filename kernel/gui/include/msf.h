#ifndef MSF_H
#define MSF_H

#include "types.h"
#include "framebuffer.h"

#define MSF_MAX_SECTIONS 8
#define MSF_MAX_KEYS 32
#define MSF_MAX_KEY_LEN 48
#define MSF_MAX_VAL_LEN 64

typedef enum {
    MSF_SECTION_NONE = 0,
    MSF_SECTION_DESKTOP,
    MSF_SECTION_GUI,
    MSF_SECTION_PERSONALIZATION
} msf_section_t;

typedef enum {
    THEME_LIGHT = 0,
    THEME_DARK,
    THEME_BLUE,
    THEME_GREEN,
    THEME_SUNSET,
    THEME_OCEAN,
    THEME_FOREST,
    THEME_AURORA,
    THEME_ROSE,
    THEME_GOLD,
    THEME_MIDNIGHT,
    THEME_LAVENDER,
    THEME_MD3_DARK,
    THEME_MD3_LIGHT,
    THEME_COUNT
} theme_id_t;

typedef struct {
    /* [desktop] */
    uint32_t wallpaper_color1;
    uint32_t wallpaper_color2;
    uint32_t icon_size;
    uint32_t icon_spacing;
    bool show_icons;

    /* [gui] */
    uint32_t window_bg;
    uint32_t titlebar_active;
    uint32_t titlebar_inactive;
    uint32_t title_text;
    uint32_t button_bg;
    uint32_t button_text;
    uint32_t border_color;
    uint32_t menu_bg;
    uint32_t menu_hover;
    uint32_t menu_border;
    uint32_t font_color;

    /* [personalization] */
    theme_id_t theme;
    uint32_t accent_color;
    uint32_t taskbar_color;
    uint32_t taskbar_height;
    uint32_t start_button_color;
    uint32_t cursor_style;
} msf_settings_t;

extern msf_settings_t msf_settings;

void msf_load_defaults(void);
void msf_parse(const char* text);
void msf_apply(void);
uint32_t msf_get_color(const char* key, uint32_t default_color);
uint32_t msf_get_uint(const char* key, uint32_t default_val);
bool msf_get_bool(const char* key, bool default_val);

void msf_set_theme(theme_id_t theme);
void msf_set_accent(uint32_t color);
void msf_set_wallpaper(uint32_t c1, uint32_t c2);

void msf_save_to_buffer(char* buf, uint32_t buf_size);

#endif