#include "msf.h"
#include "desktop.h"
#include "window_manager.h"

msf_settings_t msf_settings;

static const char* default_msf =
    "[desktop]\n"
    "wallpaper_color1=08122A\n"
    "wallpaper_color2=332264\n"
    "icon_size=40\n"
    "icon_spacing=78\n"
    "show_icons=true\n"
    "\n"
    "[gui]\n"
    "window_bg=F7FAFF\n"
    "titlebar_active=F7FAFF\n"
    "titlebar_inactive=EAF0FA\n"
    "title_text=F4F7FF\n"
    "button_bg=F4F8FF\n"
    "button_text=172033\n"
    "border_color=D7E1F0\n"
    "menu_bg=F8FAFF\n"
    "menu_hover=E8F2FF\n"
    "menu_border=D7E1F0\n"
    "font_color=172033\n"
    "\n"
    "[personalization]\n"
    "theme=light\n"
    "accent_color=258DFF\n"
    "taskbar_color=0B1123\n"
    "taskbar_height=64\n"
    "start_button_color=258DFF\n"
    "cursor_style=default\n";

static uint32_t hex_to_uint(const char* s, uint32_t len) {
    uint32_t val = 0;
    for (uint32_t i = 0; i < len && s[i]; i++) {
        char c = s[i];
        uint32_t digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else break;
        val = val * 16 + digit;
    }
    return val;
}

static uint32_t parse_color_hex(const char* s) {
    uint32_t r, g, b;
    uint32_t len = 0;
    while (s[len] && s[len] != '\n' && s[len] != '\r') len++;
    if (len < 6) return 0;
    r = hex_to_uint(s, 2);
    g = hex_to_uint(s + 2, 2);
    b = hex_to_uint(s + 4, 2);
    return RGB(r, g, b);
}

static uint32_t parse_uint(const char* s) {
    uint32_t val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

static bool parse_bool(const char* s) {
    if (s[0] == 't' || s[0] == 'T' || s[0] == '1') return true;
    return false;
}

static bool str_eq(const char* a, const char* b, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (a[i] != b[i]) return false;
        if (a[i] == '\0' || b[i] == '\0') return a[i] == b[i];
    }
    return true;
}

static bool str_starts_with(const char* s, const char* prefix) {
    uint32_t i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i]) return false;
        i++;
    }
    return true;
}

static int32_t str_find_char(const char* s, char c) {
    int32_t i = 0;
    while (s[i]) {
        if (s[i] == c) return i;
        i++;
    }
    return -1;
}

static void str_copy(char* dst, const char* src, uint32_t max) {
    uint32_t i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void trim(char* s) {
    uint32_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    if (start > 0) {
        uint32_t i = 0;
        while (s[start + i]) { s[i] = s[start + i]; i++; }
        s[i] = '\0';
    }
    int32_t end = 0;
    while (s[end]) end++;
    end--;
    while (end >= 0 && (s[end] == ' ' || s[end] == '\t' || s[end] == '\r' || s[end] == '\n')) {
        s[end] = '\0';
        end--;
    }
}

void msf_load_defaults(void) {
    msf_settings.wallpaper_color1  = RGB(0x08, 0x12, 0x2A);
    msf_settings.wallpaper_color2  = RGB(0x33, 0x22, 0x64);
    msf_settings.icon_size        = 40;
    msf_settings.icon_spacing     = 78;
    msf_settings.show_icons       = true;

    msf_settings.window_bg        = RGB(0xF7, 0xFA, 0xFF);
    msf_settings.titlebar_active   = RGB(0xF7, 0xFA, 0xFF);
    msf_settings.titlebar_inactive = RGB(0xEA, 0xF0, 0xFA);
    msf_settings.title_text       = RGB(0x17, 0x20, 0x33);
    msf_settings.button_bg        = RGB(0xF4, 0xF8, 0xFF);
    msf_settings.button_text      = RGB(0x17, 0x20, 0x33);
    msf_settings.border_color     = RGB(0xD7, 0xE1, 0xF0);
    msf_settings.menu_bg          = RGB(0xF8, 0xFA, 0xFF);
    msf_settings.menu_hover       = RGB(0xE8, 0xF2, 0xFF);
    msf_settings.menu_border      = RGB(0xD7, 0xE1, 0xF0);
    msf_settings.font_color       = RGB(0x17, 0x20, 0x33);

    msf_settings.theme             = THEME_LIGHT;
    msf_settings.accent_color      = RGB(0x25, 0x8D, 0xFF);
    msf_settings.taskbar_color     = RGB(0x0B, 0x11, 0x23);
    msf_settings.taskbar_height    = 64;
    msf_settings.start_button_color = RGB(0x25, 0x8D, 0xFF);
    msf_settings.cursor_style      = 0;
}

void msf_parse(const char* text) {
    if (!text) return;

    msf_section_t section = MSF_SECTION_NONE;
    const char* p = text;

    while (*p) {
        /* skip leading whitespace */
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '#' || *p == '\n' || *p == '\r') {
            /* skip comment or blank line */
            if (*p == '#') {
                while (*p && *p != '\n') p++;
            }
            if (*p) p++;
            continue;
        }

        if (*p == '[') {
            p++;
            char sec_name[32];
            uint32_t si = 0;
            while (*p && *p != ']' && *p != '\n' && si < 31) {
                sec_name[si++] = *p++;
            }
            sec_name[si] = '\0';
            if (*p == ']') p++;

            trim(sec_name);

            if (str_eq(sec_name, "desktop", 8)) {
                section = MSF_SECTION_DESKTOP;
            } else if (str_eq(sec_name, "gui", 4)) {
                section = MSF_SECTION_GUI;
            } else if (str_eq(sec_name, "personalization", 16)) {
                section = MSF_SECTION_PERSONALIZATION;
            } else {
                section = MSF_SECTION_NONE;
            }

            while (*p && *p != '\n') p++;
            if (*p) p++;
            continue;
        }

        /* parse key=value */
        char key[MSF_MAX_KEY_LEN];
        char value[MSF_MAX_VAL_LEN];
        uint32_t ki = 0, vi = 0;

        while (*p && *p != '=' && *p != '\n' && ki < MSF_MAX_KEY_LEN - 1) {
            key[ki++] = *p++;
        }
        key[ki] = '\0';

        if (*p == '=') {
            p++;
            while (*p && *p != '\n' && *p != '\r' && vi < MSF_MAX_VAL_LEN - 1) {
                value[vi++] = *p++;
            }
        }
        value[vi] = '\0';

        trim(key);
        trim(value);

        if (section == MSF_SECTION_DESKTOP) {
            if (str_eq(key, "wallpaper_color1", 17)) {
                msf_settings.wallpaper_color1 = parse_color_hex(value);
            } else if (str_eq(key, "wallpaper_color2", 17)) {
                msf_settings.wallpaper_color2 = parse_color_hex(value);
            } else if (str_eq(key, "icon_size", 10)) {
                msf_settings.icon_size = parse_uint(value);
            } else if (str_eq(key, "icon_spacing", 13)) {
                msf_settings.icon_spacing = parse_uint(value);
            } else if (str_eq(key, "show_icons", 10)) {
                msf_settings.show_icons = parse_bool(value);
            }
        } else if (section == MSF_SECTION_GUI) {
            if (str_eq(key, "window_bg", 10)) {
                msf_settings.window_bg = parse_color_hex(value);
            } else if (str_eq(key, "titlebar_active", 16)) {
                msf_settings.titlebar_active = parse_color_hex(value);
            } else if (str_eq(key, "titlebar_inactive", 17)) {
                msf_settings.titlebar_inactive = parse_color_hex(value);
            } else if (str_eq(key, "title_text", 11)) {
                msf_settings.title_text = parse_color_hex(value);
            } else if (str_eq(key, "button_bg", 10)) {
                msf_settings.button_bg = parse_color_hex(value);
            } else if (str_eq(key, "button_text", 12)) {
                msf_settings.button_text = parse_color_hex(value);
            } else if (str_eq(key, "border_color", 12)) {
                msf_settings.border_color = parse_color_hex(value);
            } else if (str_eq(key, "menu_bg", 8)) {
                msf_settings.menu_bg = parse_color_hex(value);
            } else if (str_eq(key, "menu_hover", 11)) {
                msf_settings.menu_hover = parse_color_hex(value);
            } else if (str_eq(key, "menu_border", 12)) {
                msf_settings.menu_border = parse_color_hex(value);
            } else if (str_eq(key, "font_color", 11)) {
                msf_settings.font_color = parse_color_hex(value);
            }
        } else if (section == MSF_SECTION_PERSONALIZATION) {
            if (str_eq(key, "theme", 6)) {
                if (str_eq(value, "dark", 5)) msf_settings.theme = THEME_DARK;
                else if (str_eq(value, "blue", 5)) msf_settings.theme = THEME_BLUE;
                else if (str_eq(value, "green", 6)) msf_settings.theme = THEME_GREEN;
                else if (str_eq(value, "sunset", 7)) msf_settings.theme = THEME_SUNSET;
                else if (str_eq(value, "ocean", 6)) msf_settings.theme = THEME_OCEAN;
                else if (str_eq(value, "forest", 7)) msf_settings.theme = THEME_FOREST;
                else if (str_eq(value, "aurora", 7)) msf_settings.theme = THEME_AURORA;
                else if (str_eq(value, "rose", 5)) msf_settings.theme = THEME_ROSE;
                else if (str_eq(value, "gold", 5)) msf_settings.theme = THEME_GOLD;
                else if (str_eq(value, "midnight", 9)) msf_settings.theme = THEME_MIDNIGHT;
                else if (str_eq(value, "lavender", 9)) msf_settings.theme = THEME_LAVENDER;
                else msf_settings.theme = THEME_LIGHT;
            } else if (str_eq(key, "accent_color", 13)) {
                msf_settings.accent_color = parse_color_hex(value);
            } else if (str_eq(key, "taskbar_color", 14)) {
                msf_settings.taskbar_color = parse_color_hex(value);
            } else if (str_eq(key, "taskbar_height", 15)) {
                msf_settings.taskbar_height = parse_uint(value);
            } else if (str_eq(key, "start_button_color", 19)) {
                msf_settings.start_button_color = parse_color_hex(value);
            } else if (str_eq(key, "cursor_style", 13)) {
                if (str_eq(value, "default", 8)) msf_settings.cursor_style = 0;
                else msf_settings.cursor_style = 0;
            }
        }

        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
}

void msf_apply(void) {
    /* Apply desktop colors */
    desktop.wallpaper_color1 = msf_settings.wallpaper_color1;
    desktop.wallpaper_color2 = msf_settings.wallpaper_color2;

    /* Apply WM desktop color */
    wm.desktop_color = msf_settings.wallpaper_color1;
}

void msf_set_theme(theme_id_t theme) {
    msf_settings.theme = theme;

    switch (theme) {
        case THEME_LIGHT:
            /* Modern light: soft wallpaper, glass surfaces, clear dark text */
            msf_settings.wallpaper_color1  = RGB(0x08, 0x12, 0x2A);
            msf_settings.wallpaper_color2  = RGB(0x33, 0x22, 0x64);
            msf_settings.window_bg         = RGB(0xF7, 0xFA, 0xFF);
            msf_settings.titlebar_active   = RGB(0xF7, 0xFA, 0xFF);
            msf_settings.titlebar_inactive = RGB(0xEA, 0xF0, 0xFA);
            msf_settings.title_text        = RGB(0x17, 0x20, 0x33);
            msf_settings.button_bg         = RGB(0xF4, 0xF8, 0xFF);
            msf_settings.button_text       = RGB(0x17, 0x20, 0x33);
            msf_settings.border_color      = RGB(0xB6, 0xC6, 0xDE);
            msf_settings.menu_bg           = RGB(0xF7, 0xFA, 0xFF);
            msf_settings.menu_hover        = RGB(0xD8, 0xEA, 0xFF);
            msf_settings.menu_border       = RGB(0xB6, 0xC6, 0xDE);
            msf_settings.font_color        = RGB(0x17, 0x20, 0x33);
            msf_settings.taskbar_color     = RGB(0x0B, 0x11, 0x23);
            msf_settings.accent_color      = RGB(0x25, 0x8D, 0xFF);
            msf_settings.start_button_color = RGB(0x25, 0x8D, 0xFF);
            break;

        case THEME_DARK:
            /* Windows 10 dark mode: neutral dark gray, blue accent, white borders */
            msf_settings.wallpaper_color1  = RGB(0x1A, 0x3A, 0x5C);
            msf_settings.wallpaper_color2  = RGB(0x0A, 0x1A, 0x2E);
            msf_settings.window_bg         = RGB(0x20, 0x20, 0x20);
            msf_settings.titlebar_active   = RGB(0x2B, 0x2B, 0x2B);
            msf_settings.titlebar_inactive = RGB(0x1A, 0x1A, 0x1A);
            msf_settings.title_text        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.button_bg         = RGB(0x33, 0x33, 0x33);
            msf_settings.button_text       = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.border_color      = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.menu_bg           = RGB(0x20, 0x20, 0x20);
            msf_settings.menu_hover        = RGB(0x40, 0x40, 0x40);
            msf_settings.menu_border       = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.font_color        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.taskbar_color     = RGB(0x1A, 0x1A, 0x1A);
            msf_settings.accent_color      = RGB(0x00, 0x78, 0xD4);
            msf_settings.start_button_color = RGB(0x00, 0x78, 0xD4);
            break;

        case THEME_BLUE:
            /* Light blue theme - airy and bright */
            msf_settings.wallpaper_color1  = RGB(0x41, 0x9E, 0xF4);
            msf_settings.wallpaper_color2  = RGB(0x00, 0x6B, 0xCE);
            msf_settings.window_bg         = RGB(0xF0, 0xF6, 0xFF);
            msf_settings.titlebar_active   = RGB(0x00, 0x7A, 0xE5);
            msf_settings.titlebar_inactive = RGB(0x80, 0xB0, 0xD0);
            msf_settings.title_text        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.button_bg         = RGB(0xE0, 0xEC, 0xFF);
            msf_settings.button_text       = RGB(0x00, 0x33, 0x66);
            msf_settings.border_color      = RGB(0x60, 0x90, 0xC0);
            msf_settings.menu_bg           = RGB(0xF0, 0xF6, 0xFF);
            msf_settings.menu_hover        = RGB(0x91, 0xC9, 0xFF);
            msf_settings.menu_border       = RGB(0x60, 0x90, 0xC0);
            msf_settings.font_color        = RGB(0x00, 0x22, 0x44);
            msf_settings.taskbar_color     = RGB(0xE0, 0xEC, 0xFF);
            msf_settings.accent_color      = RGB(0x00, 0x6B, 0xCE);
            msf_settings.start_button_color = RGB(0x00, 0x6B, 0xCE);
            break;

        case THEME_GREEN:
            /* Nature green theme */
            msf_settings.wallpaper_color1  = RGB(0x2E, 0x9E, 0x4F);
            msf_settings.wallpaper_color2  = RGB(0x0A, 0x3E, 0x1A);
            msf_settings.window_bg         = RGB(0xE8, 0xF5, 0xE9);
            msf_settings.titlebar_active   = RGB(0x2E, 0x7D, 0x32);
            msf_settings.titlebar_inactive = RGB(0x80, 0xA0, 0x80);
            msf_settings.title_text        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.button_bg         = RGB(0xE8, 0xF5, 0xE9);
            msf_settings.button_text       = RGB(0x1B, 0x5E, 0x20);
            msf_settings.border_color      = RGB(0x60, 0x90, 0x60);
            msf_settings.menu_bg           = RGB(0xE8, 0xF5, 0xE9);
            msf_settings.menu_hover        = RGB(0xA5, 0xD6, 0xA7);
            msf_settings.menu_border       = RGB(0x60, 0x90, 0x60);
            msf_settings.font_color        = RGB(0x1B, 0x5E, 0x20);
            msf_settings.taskbar_color     = RGB(0xD0, 0xE8, 0xD0);
            msf_settings.accent_color      = RGB(0x2E, 0x7D, 0x32);
            msf_settings.start_button_color = RGB(0x2E, 0x7D, 0x32);
            break;

        case THEME_SUNSET:
            /* Warm sunset theme - orange to purple */
            msf_settings.wallpaper_color1  = RGB(0xFF, 0x6B, 0x35);
            msf_settings.wallpaper_color2  = RGB(0x4A, 0x1A, 0x5E);
            msf_settings.window_bg         = RGB(0x4A, 0x3A, 0x4E);
            msf_settings.titlebar_active   = RGB(0x3A, 0x2A, 0x3E);
            msf_settings.titlebar_inactive = RGB(0x5A, 0x4A, 0x5E);
            msf_settings.title_text        = RGB(0xFF, 0xF0, 0xE0);
            msf_settings.button_bg         = RGB(0x5A, 0x4A, 0x5E);
            msf_settings.button_text       = RGB(0xFF, 0xF0, 0xE0);
            msf_settings.border_color      = RGB(0x6A, 0x5A, 0x6E);
            msf_settings.menu_bg           = RGB(0x4A, 0x3A, 0x4E);
            msf_settings.menu_hover        = RGB(0xFF, 0x9F, 0x0A);
            msf_settings.menu_border       = RGB(0x6A, 0x5A, 0x6E);
            msf_settings.font_color        = RGB(0xFF, 0xF0, 0xE0);
            msf_settings.taskbar_color     = RGB(0x3A, 0x2A, 0x3E);
            msf_settings.accent_color      = RGB(0xFF, 0x9F, 0x0A);
            msf_settings.start_button_color = RGB(0xFF, 0x9F, 0x0A);
            break;

        case THEME_OCEAN:
            /* Deep ocean theme - teal to dark blue */
            msf_settings.wallpaper_color1  = RGB(0x00, 0xB4, 0xD8);
            msf_settings.wallpaper_color2  = RGB(0x00, 0x1E, 0x3C);
            msf_settings.window_bg         = RGB(0x2A, 0x3E, 0x4E);
            msf_settings.titlebar_active   = RGB(0x1E, 0x32, 0x42);
            msf_settings.titlebar_inactive = RGB(0x3E, 0x52, 0x62);
            msf_settings.title_text        = RGB(0xE0, 0xF0, 0xFF);
            msf_settings.button_bg         = RGB(0x3E, 0x52, 0x62);
            msf_settings.button_text       = RGB(0xE0, 0xF0, 0xFF);
            msf_settings.border_color      = RGB(0x4E, 0x62, 0x72);
            msf_settings.menu_bg           = RGB(0x2A, 0x3E, 0x4E);
            msf_settings.menu_hover        = RGB(0x40, 0xCA, 0xE8);
            msf_settings.menu_border       = RGB(0x4E, 0x62, 0x72);
            msf_settings.font_color        = RGB(0xE0, 0xF0, 0xFF);
            msf_settings.taskbar_color     = RGB(0x1E, 0x32, 0x42);
            msf_settings.accent_color      = RGB(0x40, 0xCA, 0xE8);
            msf_settings.start_button_color = RGB(0x40, 0xCA, 0xE8);
            break;

        case THEME_FOREST:
            /* Dark forest theme - green tones */
            msf_settings.wallpaper_color1  = RGB(0x1B, 0x5E, 0x20);
            msf_settings.wallpaper_color2  = RGB(0x05, 0x1E, 0x0A);
            msf_settings.window_bg         = RGB(0x2E, 0x3E, 0x2A);
            msf_settings.titlebar_active   = RGB(0x1E, 0x2E, 0x1A);
            msf_settings.titlebar_inactive = RGB(0x3E, 0x4E, 0x3A);
            msf_settings.title_text        = RGB(0xE0, 0xFF, 0xE0);
            msf_settings.button_bg         = RGB(0x3E, 0x4E, 0x3A);
            msf_settings.button_text       = RGB(0xE0, 0xFF, 0xE0);
            msf_settings.border_color      = RGB(0x4E, 0x5E, 0x4A);
            msf_settings.menu_bg           = RGB(0x2E, 0x3E, 0x2A);
            msf_settings.menu_hover        = RGB(0x30, 0xD1, 0x58);
            msf_settings.menu_border       = RGB(0x4E, 0x5E, 0x4A);
            msf_settings.font_color        = RGB(0xE0, 0xFF, 0xE0);
            msf_settings.taskbar_color     = RGB(0x1E, 0x2E, 0x1A);
            msf_settings.accent_color      = RGB(0x30, 0xD1, 0x58);
            msf_settings.start_button_color = RGB(0x30, 0xD1, 0x58);
            break;

        case THEME_AURORA:
            /* Aurora theme - green to purple gradient, vibrant */
            msf_settings.wallpaper_color1  = RGB(0x00, 0xD4, 0xAA);  /* teal-green */
            msf_settings.wallpaper_color2  = RGB(0x6C, 0x5C, 0xE7);  /* indigo-purple */
            msf_settings.window_bg         = RGB(0x1A, 0x2A, 0x3E);
            msf_settings.titlebar_active   = RGB(0x00, 0xD4, 0xAA);
            msf_settings.titlebar_inactive = RGB(0x2A, 0x3A, 0x4E);
            msf_settings.title_text        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.button_bg         = RGB(0x2A, 0x4A, 0x5E);
            msf_settings.button_text       = RGB(0xE0, 0xFF, 0xF0);
            msf_settings.border_color      = RGB(0x00, 0xD4, 0xAA);
            msf_settings.menu_bg           = RGB(0x1A, 0x2A, 0x3E);
            msf_settings.menu_hover        = RGB(0x00, 0xD4, 0xAA);
            msf_settings.menu_border       = RGB(0x00, 0xB4, 0x8A);
            msf_settings.font_color        = RGB(0xE0, 0xFF, 0xF0);
            msf_settings.taskbar_color     = RGB(0x0A, 0x1A, 0x2E);
            msf_settings.accent_color      = RGB(0x00, 0xD4, 0xAA);
            msf_settings.start_button_color = RGB(0x00, 0xD4, 0xAA);
            break;

        case THEME_ROSE:
            /* Rose theme - pink to deep magenta, romantic */
            msf_settings.wallpaper_color1  = RGB(0xFF, 0x37, 0x85);  /* hot pink */
            msf_settings.wallpaper_color2  = RGB(0x4A, 0x14, 0x4E);  /* deep wine */
            msf_settings.window_bg         = RGB(0x3A, 0x1A, 0x3E);
            msf_settings.titlebar_active   = RGB(0xFF, 0x37, 0x85);
            msf_settings.titlebar_inactive = RGB(0x5A, 0x3A, 0x5E);
            msf_settings.title_text        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.button_bg         = RGB(0x5A, 0x2A, 0x5E);
            msf_settings.button_text       = RGB(0xFF, 0xE0, 0xF0);
            msf_settings.border_color      = RGB(0x8A, 0x4A, 0x8E);
            msf_settings.menu_bg           = RGB(0x3A, 0x1A, 0x3E);
            msf_settings.menu_hover        = RGB(0xFF, 0x37, 0x85);
            msf_settings.menu_border       = RGB(0x8A, 0x4A, 0x8E);
            msf_settings.font_color        = RGB(0xFF, 0xE0, 0xF0);
            msf_settings.taskbar_color     = RGB(0x2A, 0x0A, 0x2E);
            msf_settings.accent_color      = RGB(0xFF, 0x37, 0x85);
            msf_settings.start_button_color = RGB(0xFF, 0x37, 0x85);
            break;

        case THEME_GOLD:
            /* Gold theme - golden to deep brown, luxurious */
            msf_settings.wallpaper_color1  = RGB(0xFF, 0xD6, 0x0A);  /* bright gold */
            msf_settings.wallpaper_color2  = RGB(0xE6, 0x5C, 0x00);  /* deep orange */
            msf_settings.window_bg         = RGB(0x2E, 0x24, 0x10);
            msf_settings.titlebar_active   = RGB(0xFF, 0xD6, 0x0A);
            msf_settings.titlebar_inactive = RGB(0x4E, 0x44, 0x30);
            msf_settings.title_text        = RGB(0x1A, 0x10, 0x00);
            msf_settings.button_bg         = RGB(0x4E, 0x3E, 0x1A);
            msf_settings.button_text       = RGB(0xFF, 0xF0, 0xC0);
            msf_settings.border_color      = RGB(0x8A, 0x6E, 0x2A);
            msf_settings.menu_bg           = RGB(0x2E, 0x24, 0x10);
            msf_settings.menu_hover        = RGB(0xFF, 0xD6, 0x0A);
            msf_settings.menu_border       = RGB(0x8A, 0x6E, 0x2A);
            msf_settings.font_color        = RGB(0xFF, 0xF0, 0xC0);
            msf_settings.taskbar_color     = RGB(0x1E, 0x14, 0x00);
            msf_settings.accent_color      = RGB(0xFF, 0xD6, 0x0A);
            msf_settings.start_button_color = RGB(0xFF, 0xD6, 0x0A);
            break;

        case THEME_MIDNIGHT:
            /* Midnight theme - deep blue to black, elegant */
            msf_settings.wallpaper_color1  = RGB(0x0A, 0x0E, 0x27);  /* near-black blue */
            msf_settings.wallpaper_color2  = RGB(0x00, 0x00, 0x00);  /* pure black */
            msf_settings.window_bg         = RGB(0x12, 0x16, 0x33);
            msf_settings.titlebar_active   = RGB(0x3B, 0x82, 0xF6);  /* blue-500 */
            msf_settings.titlebar_inactive = RGB(0x1E, 0x29, 0x3B);
            msf_settings.title_text        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.button_bg         = RGB(0x1E, 0x29, 0x3B);
            msf_settings.button_text       = RGB(0xBF, 0xDB, 0xFE);
            msf_settings.border_color      = RGB(0x33, 0x41, 0x55);
            msf_settings.menu_bg           = RGB(0x12, 0x16, 0x33);
            msf_settings.menu_hover        = RGB(0x3B, 0x82, 0xF6);
            msf_settings.menu_border       = RGB(0x33, 0x41, 0x55);
            msf_settings.font_color        = RGB(0xBF, 0xDB, 0xFE);
            msf_settings.taskbar_color     = RGB(0x0A, 0x0E, 0x1A);
            msf_settings.accent_color      = RGB(0x3B, 0x82, 0xF6);
            msf_settings.start_button_color = RGB(0x3B, 0x82, 0xF6);
            break;

        case THEME_LAVENDER:
            /* Lavender theme - soft purple to lavender, calm */
            msf_settings.wallpaper_color1  = RGB(0xB5, 0x7B, 0xD7);  /* soft purple */
            msf_settings.wallpaper_color2  = RGB(0xE6, 0xE0, 0xF8);  /* lavender */
            msf_settings.window_bg         = RGB(0x3A, 0x30, 0x4E);
            msf_settings.titlebar_active   = RGB(0xB5, 0x7B, 0xD7);
            msf_settings.titlebar_inactive = RGB(0x5A, 0x50, 0x6E);
            msf_settings.title_text        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.button_bg         = RGB(0x5A, 0x4E, 0x7E);
            msf_settings.button_text       = RGB(0xF0, 0xE8, 0xFF);
            msf_settings.border_color      = RGB(0x8A, 0x7A, 0xAE);
            msf_settings.menu_bg           = RGB(0x3A, 0x30, 0x4E);
            msf_settings.menu_hover        = RGB(0xB5, 0x7B, 0xD7);
            msf_settings.menu_border       = RGB(0x8A, 0x7A, 0xAE);
            msf_settings.font_color        = RGB(0xF0, 0xE8, 0xFF);
            msf_settings.taskbar_color     = RGB(0x2A, 0x20, 0x3E);
            msf_settings.accent_color      = RGB(0xB5, 0x7B, 0xD7);
            msf_settings.start_button_color = RGB(0xB5, 0x7B, 0xD7);
            break;

        case THEME_MD3_DARK:
            msf_settings.wallpaper_color1  = RGB(0x1C, 0x1B, 0x1E);
            msf_settings.wallpaper_color2  = RGB(0x14, 0x13, 0x16);
            msf_settings.window_bg         = RGB(0x1C, 0x1B, 0x1E);
            msf_settings.titlebar_active   = RGB(0x4F, 0x37, 0x8A);
            msf_settings.titlebar_inactive = RGB(0x31, 0x30, 0x33);
            msf_settings.title_text        = RGB(0xE6, 0xE1, 0xE6);
            msf_settings.button_bg         = RGB(0xCF, 0xBC, 0xFF);
            msf_settings.button_text       = RGB(0x38, 0x1E, 0x72);
            msf_settings.border_color      = RGB(0x94, 0x8F, 0x99);
            msf_settings.menu_bg           = RGB(0x2B, 0x29, 0x30);
            msf_settings.menu_hover        = RGB(0x4A, 0x44, 0x58);
            msf_settings.menu_border       = RGB(0x49, 0x45, 0x4E);
            msf_settings.font_color        = RGB(0xE6, 0xE1, 0xE6);
            msf_settings.taskbar_color     = RGB(0x14, 0x13, 0x16);
            msf_settings.accent_color      = RGB(0xCF, 0xBC, 0xFF);
            msf_settings.start_button_color = RGB(0xCF, 0xBC, 0xFF);
            break;

        case THEME_MD3_LIGHT:
            msf_settings.wallpaper_color1  = RGB(0xFF, 0xFB, 0xFE);
            msf_settings.wallpaper_color2  = RGB(0xE8, 0xE0, 0xF8);
            msf_settings.window_bg         = RGB(0xFF, 0xFB, 0xFE);
            msf_settings.titlebar_active   = RGB(0x67, 0x50, 0xA4);
            msf_settings.titlebar_inactive = RGB(0xE7, 0xE0, 0xEC);
            msf_settings.title_text        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.button_bg         = RGB(0x67, 0x50, 0xA4);
            msf_settings.button_text       = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.border_color      = RGB(0xCA, 0xC4, 0xCF);
            msf_settings.menu_bg           = RGB(0xFF, 0xFB, 0xFE);
            msf_settings.menu_hover        = RGB(0xE8, 0xE0, 0xF8);
            msf_settings.menu_border       = RGB(0x79, 0x75, 0x85);
            msf_settings.font_color        = RGB(0x1D, 0x1B, 0x20);
            msf_settings.taskbar_color     = RGB(0xF0, 0xED, 0xF3);
            msf_settings.accent_color      = RGB(0x67, 0x50, 0xA4);
            msf_settings.start_button_color = RGB(0x67, 0x50, 0xA4);
            break;

        default: /* THEME_LIGHT */
            /* Clean light theme - bright with blue accents */
            msf_settings.wallpaper_color1  = RGB(0x6B, 0xC5, 0xF4);
            msf_settings.wallpaper_color2  = RGB(0x00, 0x78, 0xD7);
            msf_settings.window_bg         = RGB(0xFA, 0xFA, 0xFA);
            msf_settings.titlebar_active   = RGB(0x00, 0x78, 0xD7);
            msf_settings.titlebar_inactive = RGB(0xA0, 0xA0, 0xA0);
            msf_settings.title_text        = RGB(0xFF, 0xFF, 0xFF);
            msf_settings.button_bg         = RGB(0xF0, 0xF0, 0xF0);
            msf_settings.button_text       = RGB(0x00, 0x00, 0x00);
            msf_settings.border_color      = RGB(0xC0, 0xC0, 0xC0);
            msf_settings.menu_bg           = RGB(0xFA, 0xFA, 0xFA);
            msf_settings.menu_hover        = RGB(0xCC, 0xE8, 0xFF);
            msf_settings.menu_border       = RGB(0xA0, 0xA0, 0xA0);
            msf_settings.font_color        = RGB(0x00, 0x00, 0x00);
            msf_settings.taskbar_color     = RGB(0xF3, 0xF3, 0xF3);
            msf_settings.accent_color      = RGB(0x00, 0x78, 0xD7);
            msf_settings.start_button_color = RGB(0x00, 0x78, 0xD7);
            break;
    }

    msf_apply();
}

void msf_set_accent(uint32_t color) {
    msf_settings.accent_color = color;
    msf_settings.titlebar_active = color;
    msf_settings.start_button_color = color;
}

void msf_set_wallpaper(uint32_t c1, uint32_t c2) {
    msf_settings.wallpaper_color1 = c1;
    msf_settings.wallpaper_color2 = c2;
    desktop.wallpaper_color1 = c1;
    desktop.wallpaper_color2 = c2;
    wm.desktop_color = c1;
}

static void uint_to_hex(uint32_t val, char* buf) {
    const char* hex = "0123456789ABCDEF";
    buf[0] = hex[(val >> 20) & 0xF];
    buf[1] = hex[(val >> 16) & 0xF];
    buf[2] = hex[(val >> 12) & 0xF];
    buf[3] = hex[(val >> 8) & 0xF];
    buf[4] = hex[(val >> 4) & 0xF];
    buf[5] = hex[val & 0xF];
    buf[6] = '\0';
}

static uint8_t get_r(uint32_t rgb) { return (rgb >> 16) & 0xFF; }
static uint8_t get_g(uint32_t rgb) { return (rgb >> 8) & 0xFF; }
static uint8_t get_b(uint32_t rgb) { return rgb & 0xFF; }

static void color_to_hex(uint32_t rgb, char* buf) {
    const char* hex = "0123456789ABCDEF";
    uint8_t r = get_r(rgb), g = get_g(rgb), b = get_b(rgb);
    buf[0] = hex[r >> 4];
    buf[1] = hex[r & 0xF];
    buf[2] = hex[g >> 4];
    buf[3] = hex[g & 0xF];
    buf[4] = hex[b >> 4];
    buf[5] = hex[b & 0xF];
    buf[6] = '\0';
}

static void uint_to_str(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    for (int32_t j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
}

static uint32_t str_append(char* dst, const char* src) {
    uint32_t i = 0;
    while (dst[i]) i++;
    uint32_t j = 0;
    while (src[j]) { dst[i + j] = src[j]; j++; }
    dst[i + j] = '\0';
    return i + j;
}

void msf_save_to_buffer(char* buf, uint32_t buf_size) {
    char tmp[8];

    buf[0] = '\0';
    str_append(buf, "# MSF v1.0 - My Settings Format\r\n");
    str_append(buf, "# Desktop, GUI, Personalization\r\n\r\n");

    str_append(buf, "[desktop]\r\n");

    str_append(buf, "wallpaper_color1=");
    color_to_hex(msf_settings.wallpaper_color1, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "wallpaper_color2=");
    color_to_hex(msf_settings.wallpaper_color2, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "icon_size=");
    uint_to_str(msf_settings.icon_size, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "icon_spacing=");
    uint_to_str(msf_settings.icon_spacing, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "show_icons=");
    str_append(buf, msf_settings.show_icons ? "true" : "false");
    str_append(buf, "\r\n\r\n");

    str_append(buf, "[gui]\r\n");

    str_append(buf, "window_bg=");
    color_to_hex(msf_settings.window_bg, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "titlebar_active=");
    color_to_hex(msf_settings.titlebar_active, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "titlebar_inactive=");
    color_to_hex(msf_settings.titlebar_inactive, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "title_text=");
    color_to_hex(msf_settings.title_text, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "button_bg=");
    color_to_hex(msf_settings.button_bg, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "button_text=");
    color_to_hex(msf_settings.button_text, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "border_color=");
    color_to_hex(msf_settings.border_color, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "menu_bg=");
    color_to_hex(msf_settings.menu_bg, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "menu_hover=");
    color_to_hex(msf_settings.menu_hover, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "menu_border=");
    color_to_hex(msf_settings.menu_border, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "font_color=");
    color_to_hex(msf_settings.font_color, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n\r\n");

    str_append(buf, "[personalization]\r\n");

    str_append(buf, "theme=");
    switch (msf_settings.theme) {
        case THEME_DARK:  str_append(buf, "dark"); break;
        case THEME_BLUE:  str_append(buf, "blue"); break;
        case THEME_GREEN: str_append(buf, "green"); break;
        case THEME_SUNSET: str_append(buf, "sunset"); break;
        case THEME_OCEAN: str_append(buf, "ocean"); break;
        case THEME_FOREST: str_append(buf, "forest"); break;
        case THEME_AURORA: str_append(buf, "aurora"); break;
        case THEME_ROSE:  str_append(buf, "rose"); break;
        case THEME_GOLD:  str_append(buf, "gold"); break;
        case THEME_MIDNIGHT: str_append(buf, "midnight"); break;
        case THEME_LAVENDER: str_append(buf, "lavender"); break;
        default:          str_append(buf, "light"); break;
    }
    str_append(buf, "\r\n");

    str_append(buf, "accent_color=");
    color_to_hex(msf_settings.accent_color, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "taskbar_color=");
    color_to_hex(msf_settings.taskbar_color, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "taskbar_height=");
    uint_to_str(msf_settings.taskbar_height, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "start_button_color=");
    color_to_hex(msf_settings.start_button_color, tmp);
    str_append(buf, tmp);
    str_append(buf, "\r\n");

    str_append(buf, "cursor_style=default\r\n");
}