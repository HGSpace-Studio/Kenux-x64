/*
 * Kenux OS - KDE Plasma Desktop Environment (Minimal)
 * Implementation
 */

#include "kde.h"

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/wait.h>
#else
/* MinGW 兼容垫片 */
#include <ctype.h>
#include <errno.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#include <windows.h>

/* POSIX 函数替代 */
#define fork()         (-1)
#define waitpid(p,s,o) (-1)
#define kill(p,s)      (-1)
#define sleep(s)       Sleep((s)*1000)

/* setenv 替代: MinGW 无 setenv, 使用 _putenv_s */
#define setenv(n,v,o) _putenv_s((n),(v))

/* usleep 替代: MinGW 无 usleep, 使用 Sleep (毫秒) */
#define usleep(us) Sleep((us)/1000)

/* sys/wait.h 宏 stubs: MinGW 不提供 POSIX wait 宏 */
#define WIFEXITED(s)   (((s) & 0xff) == 0)
#define WEXITSTATUS(s) ((s) & 0xff)
#define WIFSIGNALED(s) 0
#define WTERMSIG(s)    0

/* getuid/getgid 不存在 */
static inline int getuid(void) { return 0; }
static inline int getgid(void) { return 0; }
/* gethostname 在 <winsock.h>/<windows.h> 中已有非 static 声明,
   此处改名为 kde_gethostname 并用宏重定向, 避免与非 static 声明冲突 */
static inline int kde_gethostname(char *name, int len) {
    DWORD sz = (DWORD)len; return GetComputerNameA(name, &sz) ? 0 : -1;
}
#define gethostname(n,l) kde_gethostname((n),(l))

/* mkdir 在 Windows 上只接受 1 个参数 */
#define mkdir(path,mode) _mkdir(path)

/* dirent 替代 */
typedef struct DIR DIR;
struct dirent {
    char d_name[260];
};
static DIR *opendir(const char *name) { (void)name; return NULL; }
static struct dirent *readdir(DIR *d) { (void)d; return NULL; }
static int closedir(DIR *d) { (void)d; return -1; }

static struct tm *localtime_r(const time_t *t, struct tm *tm) {
    return localtime_s(tm, t) == 0 ? tm : NULL;
}
#endif

/* ============================================================= */
/* Static lookup tables                                          */
/* ============================================================= */

static const char *SESSION_TYPE_NAMES[] = {
    [KDE_SESSION_PLASMA_X11]     = "plasma-x11",
    [KDE_SESSION_PLASMA_WAYLAND] = "plasma-wayland",
    [KDE_SESSION_PLASMA_MOBILE]  = "plasma-mobile",
    [KDE_SESSION_PLASMA_BIGD]    = "plasma-bigscreen",
    [KDE_SESSION_PLASMA_ACTIVE]  = "plasma-active",
};

static const char *PANEL_LOC_NAMES[] = {
    [KDE_PANEL_TOP]      = "top",
    [KDE_PANEL_BOTTOM]   = "bottom",
    [KDE_PANEL_LEFT]     = "left",
    [KDE_PANEL_RIGHT]    = "right",
    [KDE_PANEL_FLOATING] = "floating",
    [KDE_PANEL_DOCK]     = "dock",
};

static const char *WIDGET_TYPE_NAMES[] = {
    [KDE_WIDGET_DEFAULT]            = "org.kde.plasma.default",
    [KDE_WIDGET_APP_LAUNCHER]       = "org.kde.plasma.kickoff",
    [KDE_WIDGET_PAGER]              = "org.kde.plasma.pager",
    [KDE_WIDGET_TASKBAR]            = "org.kde.plasma.icontasks",
    [KDE_WIDGET_CLASSIC_TASKBAR]    = "org.kde.plasma.tasktasks",
    [KDE_WIDGET_SYSTEM_TRAY]        = "org.kde.plasma.systemtray",
    [KDE_WIDGET_DIGITAL_CLOCK]      = "org.kde.plasma.digitalclock",
    [KDE_WIDGET_ANALOG_CLOCK]       = "org.kde.plasma.analogclock",
    [KDE_WIDGET_BATTERY]           = "org.kde.plasma.battery",
    [KDE_WIDGET_VOLUME]            = "org.kde.plasma.volume",
    [KDE_WIDGET_BRIGHTNESS]        = "org.kde.plasma.brightness",
    [KDE_WIDGET_NETWORK]           = "org.kde.plasma.networkmanagement",
    [KDE_WIDGET_BLUETOOTH]         = "org.kde.plasma.bluetooth",
    [KDE_WIDGET_CLIPBOARD]         = "org.kde.plasma.clipboard",
    [KDE_WIDGET_NOTIFICATIONS]     = "org.kde.plasma.notifications",
    [KDE_WIDGET_NOTES]             = "org.kde.plasma.notes",
    [KDE_WIDGET_CALENDAR]          = "org.kde.plasma.calendar",
    [KDE_WIDGET_CALCULATOR]        = "org.kde.plasma.calculator",
    [KDE_WIDGET_WEATHER]          = "org.kde.kdeplasma-addons.weather",
    [KDE_WIDGET_FOLDERVIEW]       = "org.kde.plasma.folders",
    [KDE_WIDGET_KONSOLE_PROFILE]  = "org.kde.plasma.konsoleprofiles",
    [KDE_WIDGET_MEDIA_CONTROLLER] = "org.kde.plasma.mediacontroller",
    [KDE_WIDGET_ACTIVITY_SWITCHER] = "org.kde.plasma.activitymanager",
    [KDE_WIDGET_SYSTEM_LOAD]      = "org.kde.plasma.systemmonitor",
    [KDE_WIDGET_HARDWARE_MONITOR] = "org.kde.plasma.hardwaremonitor",
    [KDE_WIDGET_LOCK_SCREEN]      = "org.kde.plasma.lockscreen",
    [KDE_WIDGET_LEAVE]            = "org.kde.plasma.leave",
    [KDE_WIDGET_USER_SWITCHER]    = "org.kde.plasma.userswitcher",
    [KDE_WIDGET_SCREEN_CORNER_BUTTON] = "org.kde.plasma.screencorner",
    [KDE_WIDGET_WINDOW_LIST]      = "org.kde.plasma.windowlist",
    [KDE_WIDGET_SEARCH]           = "org.kde.milou",
    [KDE_WIDGET_PLACES]           = "org.kde.plasma.places",
    [KDE_WIDGET_TRASH]            = "org.kde.plasma.trash",
    [KDE_WIDGET_DESKTOP]          = "org.kde.plasma.desktop",
};

static const char *WINDOW_DECOR_NAMES[] = {
    [KDE_DECOR_BREEZE]     = "Breeze",
    [KDE_DECOR_BREEZE_DARK]= "BreezeDark",
    [KDE_DECOR_OXYGEN]     = "Oxygen",
    [KDE_DECOR_KLASSY]     = "Klassy",
    [KDE_DECOR_LIGHTLY]    = "Lightly",
    [KDE_DECOR_SWEET]      = "Sweet",
    [KDE_DECOR_MATERIA]    = "Materia",
};

static const char *TILING_MODE_NAMES[] = {
    [KDE_TILING_OFF]            = "off",
    [KDE_TILING_FLOATING_DEFAULT] = "floating",
    [KDE_TILING_TILED_DEFAULT]  = "tiled",
    [KDE_TILING_BSP]            = "bsp",
    [KDE_TILING_MASTER_STACK]   = "master-stack",
    [KDE_TILING_TABULAR]        = "tabular",
};

static const char *MENU_TYPE_NAMES[] = {
    [KDE_MENU_KICKOFF]   = "kickoff",
    [KDE_MENU_KICKER]    = "kicker",
    [KDE_MENU_DASHBOARD] = "dashboard",
    [KDE_MENU_HOMERUN]   = "homerun",
    [KDE_MENU_TILED]     = "tiled",
};

static const char *DESKTOP_LAYOUT_NAMES[] = {
    [KDE_DESKTOP_FOLDER]          = "folder",
    [KDE_DESKTOP_GRID]            = "grid",
    [KDE_DESKTOP_NEWSPAPER]       = "newspaper",
    [KDE_DESKTOP_SEARCH_AND_LAUNCH] = "search-and-launch",
};

static const char *WALLPAPER_MODE_NAMES[] = {
    [KDE_WALLPAPER_SINGLE]    = "single",
    [KDE_WALLPAPER_SLIDESHOW] = "slideshow",
    [KDE_WALLPAPER_COLOR]     = "color",
    [KDE_WALLPAPER_GRADIENT]  = "gradient",
    [KDE_WALLPAPER_PLASMA]   = "plasma",
    [KDE_WALLPAPER_VIDEO]    = "video",
};

static const char *SCREEN_EDGE_NAMES[] = {
    [KDE_EDGE_NONE]                    = "none",
    [KDE_EDGE_OVERVIEW]                = "overview",
    [KDE_EDGE_PRESENT_WINDOWS]         = "present-windows",
    [KDE_EDGE_DESKTOP_GRID]            = "desktop-grid",
    [KDE_EDGE_SHOW_DESKTOP]            = "show-desktop",
    [KDE_EDGE_SHOW_DASHBOARD]          = "show-dashboard",
    [KDE_EDGE_SHOW_SPOTLIGHT]          = "show-spotlight",
    [KDE_EDGE_SHOW_DESKTOP_GRID_PREV]  = "desktop-grid-prev",
    [KDE_EDGE_SHOW_DESKTOP_GRID_NEXT]  = "desktop-grid-next",
    [KDE_EDGE_LOCK_SCREEN]             = "lock-screen",
    [KDE_EDGE_RUN_KCM]                 = "run-kcm",
    [KDE_EDGE_EXECUTE_SCRIPT]          = "execute-script",
};

/* Standard autostart / application search paths */
static const char *APP_SEARCH_DIRS[] = {
    "/usr/share/applications",
    "/usr/local/share/applications",
    "/var/lib/flatpak/exports/share/applications",
    "/home/.local/share/applications",
    NULL,
};

static const char *KCM_SEARCH_DIRS[] = {
    "/usr/lib/qt6/plugins/kcms",
    "/usr/lib/qt5/plugins/kcms",
    "/usr/local/lib/qt6/plugins/kcms",
    NULL,
};

static const char *COLOR_SCHEME_SEARCH_DIRS[] = {
    "/usr/share/color-schemes",
    "/usr/local/share/color-schemes",
    "/home/.local/share/color-schemes",
    NULL,
};

/* ============================================================= */
/* Static helpers                                                */
/* ============================================================= */

static const char *session_type_name(KdeSessionType t) {
    if (t >= sizeof(SESSION_TYPE_NAMES)/sizeof(SESSION_TYPE_NAMES[0]))
        return SESSION_TYPE_NAMES[0];
    return SESSION_TYPE_NAMES[t] ? SESSION_TYPE_NAMES[t] : "plasma-x11";
}

static KdeSessionType session_type_from_str(const char *s) {
    for (size_t i = 0; i < sizeof(SESSION_TYPE_NAMES)/sizeof(SESSION_TYPE_NAMES[0]); i++)
        if (SESSION_TYPE_NAMES[i] && strcmp(s, SESSION_TYPE_NAMES[i]) == 0)
            return (KdeSessionType)i;
    return KDE_SESSION_PLASMA_X11;
}

static const char *panel_loc_name(KdePanelLocation l) {
    if (l >= sizeof(PANEL_LOC_NAMES)/sizeof(PANEL_LOC_NAMES[0]))
        return PANEL_LOC_NAMES[0];
    return PANEL_LOC_NAMES[l] ? PANEL_LOC_NAMES[l] : "top";
}

static const char *widget_type_name(KdeWidgetType w) {
    if (w >= sizeof(WIDGET_TYPE_NAMES)/sizeof(WIDGET_TYPE_NAMES[0]))
        return WIDGET_TYPE_NAMES[0];
    return WIDGET_TYPE_NAMES[w] ? WIDGET_TYPE_NAMES[w] : "default";
}

static KdeWidgetType widget_type_from_str(const char *s) {
    for (size_t i = 0; i < sizeof(WIDGET_TYPE_NAMES)/sizeof(WIDGET_TYPE_NAMES[0]); i++)
        if (WIDGET_TYPE_NAMES[i] && strcmp(s, WIDGET_TYPE_NAMES[i]) == 0)
            return (KdeWidgetType)i;
    return KDE_WIDGET_DEFAULT;
}

static const char *window_decor_name(KdeWindowDecoration d) {
    if (d >= sizeof(WINDOW_DECOR_NAMES)/sizeof(WINDOW_DECOR_NAMES[0]))
        return WINDOW_DECOR_NAMES[0];
    return WINDOW_DECOR_NAMES[d] ? WINDOW_DECOR_NAMES[d] : "Breeze";
}

static const char *tiling_mode_name(KdeTilingMode m) {
    if (m >= sizeof(TILING_MODE_NAMES)/sizeof(TILING_MODE_NAMES[0]))
        return TILING_MODE_NAMES[0];
    return TILING_MODE_NAMES[m] ? TILING_MODE_NAMES[m] : "off";
}

static const char *menu_type_name(KdeMenuType m) {
    if (m >= sizeof(MENU_TYPE_NAMES)/sizeof(MENU_TYPE_NAMES[0]))
        return MENU_TYPE_NAMES[0];
    return MENU_TYPE_NAMES[m] ? MENU_TYPE_NAMES[m] : "kickoff";
}

static const char *wallpaper_mode_name(KdeWallpaperMode m) {
    if (m >= sizeof(WALLPAPER_MODE_NAMES)/sizeof(WALLPAPER_MODE_NAMES[0]))
        return WALLPAPER_MODE_NAMES[0];
    return WALLPAPER_MODE_NAMES[m] ? WALLPAPER_MODE_NAMES[m] : "single";
}

static const char *screen_edge_name(KdeScreenEdgeAction e) {
    if (e >= sizeof(SCREEN_EDGE_NAMES)/sizeof(SCREEN_EDGE_NAMES[0]))
        return SCREEN_EDGE_NAMES[0];
    return SCREEN_EDGE_NAMES[e] ? SCREEN_EDGE_NAMES[e] : "none";
}

static const char *desktop_layout_name(KdeDesktopLayout l) {
    if (l >= sizeof(DESKTOP_LAYOUT_NAMES)/sizeof(DESKTOP_LAYOUT_NAMES[0]))
        return DESKTOP_LAYOUT_NAMES[0];
    return DESKTOP_LAYOUT_NAMES[l] ? DESKTOP_LAYOUT_NAMES[l] : "folder";
}

/* Trim leading/trailing whitespace in place; returns start pointer */
static char *str_trim(char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) { *end = 0; end--; }
    return s;
}

/* Split a key=value style INI line into key/value (in place) */
static int ini_split_kv(char *line, char **key, char **val) {
    char *eq = strchr(line, '=');
    if (!eq) return -1;
    *eq = 0;
    *key = str_trim(line);
    *val = str_trim(eq + 1);
    /* strip surrounding quotes */
    if ((*val)[0] == '"' || (*val)[0] == '\'') {
        size_t l = strlen(*val);
        if (l >= 2 && (*val)[l-1] == (*val)[0]) {
            (*val)[l-1] = 0;
            (*val)++;
        }
    }
    return 0;
}

/* strcasestr is non-portable; provide a tiny fallback */
static const char *kde_stristr(const char *hay, const char *needle) {
    if (!hay || !needle) return NULL;
    if (!*needle) return hay;
    for (; *hay; hay++) {
        const char *h = hay; const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
        if (!*n) return hay;
    }
    return NULL;
}

/* Append a notification id ring counter */
static uint32_t next_notification_id(KdeState *s) {
    static uint32_t counter = 0;
    (void)s;
    counter++;
    if (counter == 0) counter = 1;
    return counter;
}

/* ============================================================= */
/* 1) kde_init / kde_cleanup                                     */
/* ============================================================= */

void kde_init(KdeState *s) {
    memset(s, 0, sizeof(*s));

    s->session_type = KDE_SESSION_PLASMA_X11;

    /* Environment defaults */
    strcpy(s->xdg_runtime_dir, "/run/user/0");
    strcpy(s->xdg_data_dirs,
           "/usr/share:/usr/local/share:/var/lib/flatpak/exports/share");
    strcpy(s->xdg_config_dirs, "/etc/xdg");
    strcpy(s->kde_home,        "/root/.kde");
    strcpy(s->kde_config_dir,  "/root/.config");
    strcpy(s->kde_data_dir,    "/root/.local/share");
    strcpy(s->kdebugrc,        "/root/.config/kdebugrc");

    /* Display */
    strcpy(s->display, ":0");
    strcpy(s->wayland_display, "wayland-0");
    s->display_number = 0;
    s->headless = 0;

    /* Session/login - filled in by probe_hardware typically */
    s->uid = getuid();
    s->gid = getgid();
    s->pid = getpid();
    {
        const char *u = getenv("USER");  if (!u) u = getenv("LOGNAME"); if (!u) u = "root";
        strncpy(s->username, u, sizeof(s->username) - 1);
        if (gethostname(s->hostname, sizeof(s->hostname) - 1) != 0)
            strcpy(s->hostname, "kenuxk");
        const char *sh = getenv("SHELL"); if (!sh) sh = "/bin/bash";
        strncpy(s->shell, sh, sizeof(s->shell) - 1);
        const char *home = getenv("HOME"); if (!home) home = "/root";
        strncpy(s->home, home, sizeof(s->home) - 1);
    }

    /* Locale */
    {
        const char *lc = getenv("LANG"); if (!lc) lc = "en_US.UTF-8";
        strncpy(s->language,  lc, sizeof(s->language) - 1);
        strncpy(s->region,    lc, sizeof(s->region) - 1);
        strncpy(s->formats_locale, lc, sizeof(s->formats_locale) - 1);
        strncpy(s->time_locale,    lc, sizeof(s->time_locale) - 1);
        strncpy(s->monetary_locale,lc, sizeof(s->monetary_locale) - 1);
        strncpy(s->paper_locale,   lc, sizeof(s->paper_locale) - 1);
        strncpy(s->measurement_locale, lc, sizeof(s->measurement_locale) - 1);
        strncpy(s->collation_locale,   lc, sizeof(s->collation_locale) - 1);
        strncpy(s->charset, "UTF-8", sizeof(s->charset) - 1);
    }
    s->use_24h_clock = 1;
    s->long_date_style  = 0;
    s->short_date_style = 0;

    /* Global theme / Look-and-Feel */
    strcpy(s->lookandfeel_package, "org.kde.breeze.desktop");
    strcpy(s->global_theme_display, "Breeze");
    s->prefer_dark = 0;
    strcpy(s->widget_style, "Breeze");
    s->kvantum_theme[0] = 0;
    strcpy(s->color_scheme, "BreezeLight");
    strcpy(s->icon_theme, "breeze");
    strcpy(s->fallback_icon_theme, "breeze-dark");
    strcpy(s->cursor_theme, "Breeze");
    s->cursor_size = 24;

    /* Fonts */
    strcpy(s->general_font,   "Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1");
    strcpy(s->fixed_font,     "Noto Sans Mono,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,0,1");
    strcpy(s->titlebar_font,  "Noto Sans,10,-1,5,600,0,0,0,0,0,0,0,0,0,0,0,1");
    strcpy(s->toolbar_font,   "Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,0,1");
    strcpy(s->menu_font,      "Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,0,1");
    strcpy(s->window_title_font, "Noto Sans,12,-1,5,600,0,0,0,0,0,0,0,0,0,0,0,1");
    strcpy(s->taskbar_font,   "Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,0,1");
    strcpy(s->desktop_font,   "Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,0,1");
    s->use_antialiasing = 1;
    s->hinting = 2;            /* medium */
    strcpy(s->subpixel, "rgb");
    s->force_font_dpi = 0;
    s->font_dpi = 96;

    strcpy(s->sound_theme, "freedesktop");
    s->enable_sounds = 1;
    s->enable_notification_sounds = 1;
    s->enable_feedback_sounds = 1;

    /* Window decoration */
    s->decoration = KDE_DECOR_BREEZE;
    strcpy(s->window_decoration_theme, "Breeze");
    /* Default buttons: left=A (keep-above) ; right: I (min) A (max) X (close) */
    s->titlebar_buttons_left[0]  = 'A';
    s->titlebar_buttons_right[0] = 'I';
    s->titlebar_buttons_right[1] = 'A';
    s->titlebar_buttons_right[2] = 'X';
    s->titlebar_height_px = 30;
    s->window_border_size_px = 1;
    s->draw_titlebar_shadow = 1;
    s->blur_behind_decorations = 1;

    /* Compositor */
    s->compositor_enabled = 1;
    s->compositor_vsync = 1;
    s->compositor_scale_method = 1;       /* smooth */
    s->compositor_animation_speed_pct = 100;
    s->compositor_window_opacity = 100;
    s->effects_mask =
          KDE_EFFECT_BLUR | KDE_EFFECT_TRANSLUCENCY
        | KDE_EFFECT_DIM_INACTIVE | KDE_EFFECT_SLIDING_POPUPS
        | KDE_EFFECT_SCREEN_EDGE | KDE_EFFECT_OVERVIEW;

    /* KWin behaviour */
    s->tiling_mode = KDE_TILING_FLOATING_DEFAULT;
    s->focus_policy = 0;                   /* click to focus */
    s->focus_stealing_prevention_level = 1;
    s->auto_raise = 0;
    s->click_raise = 1;
    s->click_to_focus = 1;
    s->window_placement = 0;               /* smart */
    s->windows_fade_in = 1;
    s->windows_fade_out = 1;
    s->window_opacity_active = 100;
    s->window_opacity_inactive = 90;
    s->snap_zone_enabled = 1;
    s->snap_zone_ratio = 50;
    s->reverse_layout = 0;

    /* Workspaces / virtual desktops */
    s->desktop_count = 2;
    s->desktop_rows = 1;
    s->desktop_cols = 2;
    s->current_desktop = 0;
    for (int i = 0; i < KDE_MAX_DESKTOPS; i++) {
        snprintf(s->desktops[i].name, sizeof(s->desktops[i].name), "Desktop %d", i + 1);
        s->desktops[i].wallpaper_path[0] = 0;
    }

    /* Activities */
    s->activity_count = 1;
    s->current_activity = 0;
    strcpy(s->activities[0].id, "00000000-0000-0000-0000-000000000000");
    strcpy(s->activities[0].name, "Default");
    strcpy(s->activities[0].description, "Default Activity");
    strcpy(s->activities[0].icon, "user-home");
    s->activities[0].wallpaper[0] = 0;
    s->activities[0].enabled = 1;
    s->activities[0].keep_state = 0;

    /* Outputs/screens - probed later */
    s->screen_count = 1;
    s->primary_screen = 0;
    s->screens[0].width = 1920;
    s->screens[0].height = 1080;
    s->screens[0].refresh_mhz = 60000;
    s->screens[0].scale = 100;
    s->screens[0].rotation = 0;
    s->screens[0].x = 0; s->screens[0].y = 0;
    strcpy(s->screens[0].name, "Default");
    strcpy(s->screens[0].connector, "eDP-1");

    /* Panels / desktop containments */
    s->panel_count = 0;
    s->desktop_layout = KDE_DESKTOP_FOLDER;

    /* Plasmoids */
    s->plasmoid_count = 0;

    /* Menu */
    s->menu_style = KDE_MENU_KICKOFF;
    s->menu_show_all_applications = 1;
    s->menu_show_recent = 1;
    s->menu_search = 1;
    s->menu_favorites_count = 0;
    s->app_menu_in_titlebar = 0;
    s->show_window_decoration = 1;

    /* Wallpaper (global) */
    s->global_wallpaper.mode = KDE_WALLPAPER_SINGLE;
    s->global_wallpaper.path[0] = 0;
    s->global_wallpaper.fill_mode = 0;
    s->global_wallpaper.blur_radius = 0;
    s->global_wallpaper.dim_pct = 0;
    s->global_wallpaper.per_output = 0;
    s->wallpaper_per_activity = 0;
    s->wallpaper_per_output = 0;

    /* Task manager */
    s->taskbar_grouping = 0;
    s->taskbar_icon_only = 0;
    s->taskbar_show_tooltips = 1;
    s->taskbar_middle_click = 4;           /* launch */

    /* System tray */
    s->tray_show_all = 1;
    s->tray_hidden_items_count = 0;
    s->tray_icons_size = 22;

    /* Clock */
    s->clock_show_seconds = 0;
    s->clock_use_24h = 1;
    s->clock_custom_format[0] = 0;
    s->calendar_show_week_numbers = 0;
    s->calendar_first_day_of_week = 0;     /* Sunday */

    /* Notifications */
    s->notify_badge_count = 0;
    s->notify_history = 1;
    s->notify_do_not_disturb = 0;
    s->notify_inhibit_apps = 0;
    s->notify_show_on_lockscreen = 0;
    s->notify_critical_fullscreen = 1;
    s->notify_popup_timeout_ms = 5000;
    s->notification_count = 0;

    /* Apps / KIO defaults */
    s->app_count = 0;
    strcpy(s->default_browser,       "firefox.desktop");
    strcpy(s->default_email,         "org.kde.kmail.desktop");
    strcpy(s->default_file_manager,  "org.kde.dolphin.desktop");
    strcpy(s->default_terminal,       "org.kde.konsole.desktop");
    strcpy(s->default_music,         "org.kde.elisa.desktop");
    strcpy(s->default_video,         "org.kde.dragon.desktop");
    strcpy(s->default_image,         "org.kde.gwenview.desktop");

    /* KWin rules / hotkeys */
    s->kwin_rule_count = 0;
    s->hotkey_count = 0;

    /* Screen edges */
    s->edge_top_left = KDE_EDGE_NONE;
    s->edge_top = KDE_EDGE_NONE;
    s->edge_top_right = KDE_EDGE_NONE;
    s->edge_left = KDE_EDGE_NONE;
    s->edge_right = KDE_EDGE_NONE;
    s->edge_bottom_left = KDE_EDGE_NONE;
    s->edge_bottom = KDE_EDGE_SHOW_DESKTOP;
    s->edge_bottom_right = KDE_EDGE_NONE;
    s->edge_delay_ms = 300;
    s->edge_reactivate_ms = 600;

    /* Sessions */
    s->login_shutdown_type = 0;
    strcpy(s->autostart_dirs,
           "/etc/xdg/autostart:/usr/share/autostart:/root/.config/autostart");
    memset(s->start_kcm_after_login, 0, sizeof(s->start_kcm_after_login));

    /* Power management */
    s->power_dim_on_battery_min = 5;
    s->power_sleep_on_battery_min = 10;
    s->power_dim_on_ac_min = 10;
    s->power_sleep_on_ac_min = 0;
    s->power_button_action = 4;            /* ask */
    s->lid_switch_action_battery = 1;      /* suspend */
    s->lid_switch_action_ac = 0;            /* nothing */
    s->critical_battery_action = 0;        /* hibernate */
    s->suspend_then_hibernate = 0;
    s->brightness_on_battery_pct = 50;
    s->brightness_on_ac_pct = 100;
    s->battery_icon_pct = 0;

    /* Network */
    s->nm_enabled = 1;
    s->bluetooth_enabled = 0;
    s->airplane_mode = 0;

    /* Input - keyboard */
    strcpy(s->keyboard.model, "pc105");
    strcpy(s->keyboard.layout, "us");
    s->keyboard.variant[0] = 0;
    s->keyboard.options[0] = 0;
    strcpy(s->keyboard.switch_shortcut, "Ctrl+Alt+K");
    s->keyboard.repeat_delay_ms = 600;
    s->keyboard.repeat_rate_cps = 25;
    s->keyboard.numlock_on_boot = 1;
    s->keyboard.caps_behavior = 0;

    /* Input - touchpad */
    s->touchpad.accel_profile = 1;
    s->touchpad.accel = 0.0;
    s->touchpad.left_handed = 0;
    s->touchpad.tap_to_click = 1;
    s->touchpad.tap_and_drag = 1;
    s->touchpad.tap_and_drag_lock = 0;
    s->touchpad.natural_scroll = 1;
    s->touchpad.two_finger_scroll = 1;
    s->touchpad.middle_emulation = 0;
    s->touchpad.scroll_method = 1;
    s->touchpad.scroll_direction = 0;

    /* Input - pointer */
    s->pointer.enabled = 1;
    s->pointer.accel = 0.0;
    s->pointer.button_map[0] = 1;
    s->pointer.button_map[1] = 2;
    s->pointer.button_map[2] = 3;
    for (int i = 3; i < 16; i++) s->pointer.button_map[i] = 0;

    /* Input - touchscreen */
    s->touchscreen.enabled = 0;
    s->touchscreen.rotation_ccw = 0;
    s->touchscreen.invert_x = 0;
    s->touchscreen.invert_y = 0;

    /* Accessibility */
    s->assistive_tech_enabled = 0;
    s->kmag = 0; s->kmousetool = 0; s->ktts = 0; s->orca = 0;
    s->screen_reader_enabled = 0;
    s->invert_colors = 0;
    s->screen_magnifier = 0;
    s->magnifier_factor_pct = 200;
    s->magnifier_lens = 0;
    s->color_blind_mode = 0;
    s->single_click = 0;
    s->sticky_keys = 0;
    s->slow_keys = 0;
    s->bounce_keys = 0;
    s->mouse_keys = 0;
    s->simulated_secondary_click = 0;
    s->dwell_click = 0;

    /* Security / SDDM / lock screen */
    strcpy(s->greeter_theme, "breeze");
    s->greeter_numlock = 1;
    s->auto_login = 0;
    s->auto_login_user[0] = 0;
    s->auto_relogin = 0;
    s->passwordless_login = 0;
    s->lock_on_idle_s = 300;
    s->lock_on_resume = 1;
    s->lock_on_lid = 0;
    s->show_users_in_greeter = 1;
    s->show_manual_login = 1;
    s->lock_screen_wallpaper[0] = 0;
    s->lock_screen_blank_pct = 30;

    /* Dolphin / file manager */
    s->dolphin_default_view = 'i';         /* icons */
    s->dolphin_show_hidden = 0;
    s->dolphin_show_previews = 1;
    s->dolphin_show_folders_first = 1;
    s->dolphin_sort_case_sensitive = 0;
    s->dolphin_breadcrumb = 1;
    s->dolphin_split_view_default = 0;
    s->dolphin_terminal_panel_default = 0;
    strcpy(s->dolphin_start_path, "/root");

    /* Session state */
    s->running = 0;
    s->exit_code = 0;
    s->reload_config = 0;
    s->started_at = 0;
    s->verbose = 0;
    s->debug = 0;
    s->startup_finished = 0;
    s->pid_kwin = 0;
    s->pid_plasmashell = 0;
    s->pid_krunner = 0;
    s->pid_polkit = 0;
    s->pid_xdg_porthole = 0;
    s->pid_kded = 0;
    s->pid_ksmserver = 0;

    /* Register a few default hotkeys */
    {
        KdeHotkey hk;
        memset(&hk, 0, sizeof(hk));
        strcpy(hk.component, "krunner");
        strcpy(hk.action_id, "_launch_krunner");
        strcpy(hk.action_label, "Launch KRunner");
        strcpy(hk.key_seq, "Alt+Space");
        strcpy(hk.exec, "krunner");
        hk.enabled = 1;
        kde_hotkey_register(s, &hk);

        memset(&hk, 0, sizeof(hk));
        strcpy(hk.component, "konsole");
        strcpy(hk.action_id, "_open_terminal");
        strcpy(hk.action_label, "Open Konsole");
        strcpy(hk.key_seq, "Ctrl+Alt+T");
        strcpy(hk.exec, "konsole");
        hk.enabled = 1;
        kde_hotkey_register(s, &hk);

        memset(&hk, 0, sizeof(hk));
        strcpy(hk.component, "plasmashell");
        strcpy(hk.action_id, "_lock_screen");
        strcpy(hk.action_label, "Lock session");
        strcpy(hk.key_seq, "Meta+L");
        strcpy(hk.exec, "loginctl lock-session");
        hk.enabled = 1;
        kde_hotkey_register(s, &hk);
    }
}

void kde_cleanup(KdeState *s) {
    if (!s) return;
    /* If we are still flagged running, do an end_session first */
    if (s->running) {
        kde_end_session(s, 0, 1);
    }
    s->notification_count = 0;
    s->app_count = 0;
    s->panel_count = 0;
    s->plasmoid_count = 0;
    s->hotkey_count = 0;
    s->kwin_rule_count = 0;
    s->activity_count = 0;
    s->color_scheme_count = 0;
    s->loaded_color_scheme = NULL;
    s->pid_kwin = 0;
    s->pid_plasmashell = 0;
    s->pid_krunner = 0;
    s->pid_polkit = 0;
    s->pid_xdg_porthole = 0;
    s->pid_kded = 0;
    s->pid_ksmserver = 0;
    s->running = 0;
    s->startup_finished = 0;
}

/* ============================================================= */
/* 2) kde_parse_arguments                                       */
/* ============================================================= */

int kde_parse_arguments(KdeState *s, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i]; if (!a) continue;

        if (strcmp(a, "--session") == 0 && i + 1 < argc) {
            s->session_type = session_type_from_str(argv[++i]);
        } else if (strcmp(a, "--wayland") == 0) {
            s->session_type = KDE_SESSION_PLASMA_WAYLAND;
        } else if (strcmp(a, "--x11") == 0) {
            s->session_type = KDE_SESSION_PLASMA_X11;
        } else if (strcmp(a, "--mobile") == 0) {
            s->session_type = KDE_SESSION_PLASMA_MOBILE;
        } else if (strcmp(a, "--bigscreen") == 0) {
            s->session_type = KDE_SESSION_PLASMA_BIGD;
        } else if (strcmp(a, "--display") == 0 && i + 1 < argc) {
            strncpy(s->display, argv[++i], sizeof(s->display) - 1);
        } else if (strcmp(a, "--wayland-display") == 0 && i + 1 < argc) {
            strncpy(s->wayland_display, argv[++i], sizeof(s->wayland_display) - 1);
        } else if (strcmp(a, "--headless") == 0) {
            s->headless = 1;
        } else if (strcmp(a, "--user") == 0 && i + 1 < argc) {
            strncpy(s->username, argv[++i], sizeof(s->username) - 1);
        } else if (strcmp(a, "--home") == 0 && i + 1 < argc) {
            strncpy(s->home, argv[++i], sizeof(s->home) - 1);
            snprintf(s->kde_home, sizeof(s->kde_home), "%s/.kde", s->home);
            snprintf(s->kde_config_dir, sizeof(s->kde_config_dir), "%s/.config", s->home);
            snprintf(s->kde_data_dir, sizeof(s->kde_data_dir), "%s/.local/share", s->home);
        } else if (strcmp(a, "--shell") == 0 && i + 1 < argc) {
            strncpy(s->shell, argv[++i], sizeof(s->shell) - 1);
        } else if (strcmp(a, "--lang") == 0 && i + 1 < argc) {
            const char *lc = argv[++i];
            strncpy(s->language, lc, sizeof(s->language) - 1);
            strncpy(s->region, lc, sizeof(s->region) - 1);
            strncpy(s->formats_locale, lc, sizeof(s->formats_locale) - 1);
            strncpy(s->time_locale, lc, sizeof(s->time_locale) - 1);
        } else if (strcmp(a, "--look-and-feel") == 0 && i + 1 < argc) {
            kde_apply_lookandfeel(s, argv[++i]);
        } else if (strcmp(a, "--theme") == 0 && i + 1 < argc) {
            strncpy(s->global_theme_display, argv[++i], sizeof(s->global_theme_display) - 1);
        } else if (strcmp(a, "--color-scheme") == 0 && i + 1 < argc) {
            kde_color_scheme_set(s, argv[++i]);
        } else if (strcmp(a, "--icon-theme") == 0 && i + 1 < argc) {
            strncpy(s->icon_theme, argv[++i], sizeof(s->icon_theme) - 1);
        } else if (strcmp(a, "--cursor-theme") == 0 && i + 1 < argc) {
            strncpy(s->cursor_theme, argv[++i], sizeof(s->cursor_theme) - 1);
        } else if (strcmp(a, "--cursor-size") == 0 && i + 1 < argc) {
            s->cursor_size = atoi(argv[++i]);
        } else if (strcmp(a, "--widget-style") == 0 && i + 1 < argc) {
            strncpy(s->widget_style, argv[++i], sizeof(s->widget_style) - 1);
        } else if (strcmp(a, "--font") == 0 && i + 1 < argc) {
            strncpy(s->general_font, argv[++i], sizeof(s->general_font) - 1);
        } else if (strcmp(a, "--fixed-font") == 0 && i + 1 < argc) {
            strncpy(s->fixed_font, argv[++i], sizeof(s->fixed_font) - 1);
        } else if (strcmp(a, "--font-dpi") == 0 && i + 1 < argc) {
            s->font_dpi = atoi(argv[++i]);
            s->force_font_dpi = (s->font_dpi > 0);
        } else if (strcmp(a, "--dark") == 0) {
            s->prefer_dark = 1;
            kde_color_scheme_set(s, "BreezeDark");
        } else if (strcmp(a, "--light") == 0) {
            s->prefer_dark = 0;
            kde_color_scheme_set(s, "BreezeLight");
        } else if (strcmp(a, "--desktops") == 0 && i + 1 < argc) {
            int n = atoi(argv[++i]);
            if (n > 0 && n <= KDE_MAX_DESKTOPS) {
                s->desktop_count = n;
                if (s->desktop_cols * s->desktop_rows < n) {
                    s->desktop_cols = n; s->desktop_rows = 1;
                }
            }
        } else if (strcmp(a, "--menu") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            for (size_t k = 0; k < sizeof(MENU_TYPE_NAMES)/sizeof(MENU_TYPE_NAMES[0]); k++)
                if (MENU_TYPE_NAMES[k] && strcmp(v, MENU_TYPE_NAMES[k]) == 0)
                    s->menu_style = (KdeMenuType)k;
        } else if (strcmp(a, "--tiling") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            for (size_t k = 0; k < sizeof(TILING_MODE_NAMES)/sizeof(TILING_MODE_NAMES[0]); k++)
                if (TILING_MODE_NAMES[k] && strcmp(v, TILING_MODE_NAMES[k]) == 0)
                    s->tiling_mode = (KdeTilingMode)k;
        } else if (strcmp(a, "--autologin") == 0 && i + 1 < argc) {
            s->auto_login = 1;
            strncpy(s->auto_login_user, argv[++i], sizeof(s->auto_login_user) - 1);
        } else if (strcmp(a, "--no-autologin") == 0) {
            s->auto_login = 0;
        } else if (strcmp(a, "--airplane") == 0) {
            s->airplane_mode = 1;
            s->nm_enabled = 0;
            s->bluetooth_enabled = 0;
        } else if (strcmp(a, "--preset") == 0 && i + 1 < argc) {
            const char *p = argv[++i];
            if (strcmp(p, "default") == 0)        kde_apply_default_preset(s);
            else if (strcmp(p, "minimal") == 0)   kde_apply_minimal_preset(s);
            else if (strcmp(p, "mobile") == 0)    kde_apply_mobile_preset(s);
        } else if (strcmp(a, "--probe-hardware") == 0) {
            kde_probe_hardware(s);
        } else if (strcmp(a, "--load-config") == 0) {
            kde_load_configuration(s);
        } else if (strcmp(a, "--save-config") == 0) {
            kde_save_configuration(s);
        } else if (strcmp(a, "--scan-apps") == 0) {
            kde_apps_scan(s);
        } else if (strcmp(a, "--scan-kcms") == 0) {
            kde_kcm_scan(s);
        } else if (strcmp(a, "--kcm") == 0 && i + 1 < argc) {
            kde_kcm_launch(s, argv[++i]);
        } else if (strcmp(a, "--notify") == 0 && i + 2 < argc) {
            KdeNotification n; memset(&n, 0, sizeof(n));
            strncpy(n.summary, argv[++i], sizeof(n.summary) - 1);
            strncpy(n.body, argv[++i], sizeof(n.body) - 1);
            n.urgency = 1; n.expire_timeout_ms = 5000;
            kde_notify(s, &n);
        } else if (strcmp(a, "--run") == 0 && i + 1 < argc) {
            kde_krunner_run(s, argv[++i]);
        } else if (strcmp(a, "--verbose") == 0 || strcmp(a, "-v") == 0) {
            s->verbose++;
        } else if (strcmp(a, "--debug") == 0) {
            s->debug = 1; s->verbose = s->verbose ? s->verbose : 1;
        } else if (strcmp(a, "--dry-run") == 0) {
            s->reload_config = 1; /* misuse flag as dry-run marker */
        } else if (strcmp(a, "--version") == 0 || strcmp(a, "-V") == 0) {
            kde_print_version();
            return 1;
        } else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            kde_print_help();
            return 1;
        } else {
            /* Unknown flag - ignore but report in verbose */
            if (s->verbose)
                fprintf(stderr, "kde: unknown option '%s' ignored\n", a);
        }
    }
    return 0;
}

/* ============================================================= */
/* 3) kde_probe_hardware / kde_load_configuration               */
/* ============================================================= */

int kde_probe_hardware(KdeState *s) {
    /* Discover screen count via /sys/class/drm */
    int screen_count = 0;
    DIR *drm = opendir("/sys/class/drm");
    if (drm) {
        struct dirent *de;
        while ((de = readdir(drm)) != NULL) {
            if (strncmp(de->d_name, "card-", 5) != 0) continue;
            const char *conn = de->d_name + 5;
            /* Skip loose card0 with no dash connector */
            if (!strchr(conn, '-')) continue;
            if (screen_count >= KDE_MAX_SCREENS) break;
            strncpy(s->screens[screen_count].connector, conn,
                    sizeof(s->screens[screen_count].connector) - 1);
            strncpy(s->screens[screen_count].name, conn,
                    sizeof(s->screens[screen_count].name) - 1);
            screen_count++;
        }
        closedir(drm);
    }
    if (screen_count == 0) {
        /* Fallback: assume a single 1920x1080 panel */
        strncpy(s->screens[0].connector, "eDP-1", sizeof(s->screens[0].connector) - 1);
        strncpy(s->screens[0].name, "Default", sizeof(s->screens[0].name) - 1);
        screen_count = 1;
    }
    s->screen_count = screen_count;
    s->primary_screen = 0;

    /* Touchpad presence via /proc/bus/input/devices heuristics */
    {
        FILE *f = fopen("/proc/bus/input/devices", "r");
        if (f) {
            char line[1024]; int saw_touchpad = 0;
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "Touchpad") || strstr(line, "Synaptics") || strstr(line, "ELAN")) {
                    saw_touchpad = 1; break;
                }
            }
            fclose(f);
            if (saw_touchpad) {
                strcpy(s->touchpad.name, "SynPS/2 Synaptics TouchPad");
            }
        }
    }

    /* Battery presence */
    {
        struct stat st;
        if (stat("/sys/class/power_supply/BAT0", &st) == 0
         || stat("/sys/class/power_supply/BAT1", &st) == 0)
            s->battery_icon_pct = 100;
    }

    if (s->verbose)
        fprintf(stderr, "kde: probed %d screen(s), touchpad=%d, battery=%d\n",
                s->screen_count, s->touchpad.name[0] != 0, s->battery_icon_pct);
    return 0;
}

/* Apply a single kdeglobals-style INI key to the state */
static int apply_kdeglobals_kv(KdeState *s, const char *group,
                               const char *key, const char *val) {
    (void)group;
    if (!key || !val) return -1;
    if (strcmp(key, "LookAndFeel") == 0)
        strncpy(s->lookandfeel_package, val, sizeof(s->lookandfeel_package) - 1);
    else if (strcmp(key, "WidgetStyle") == 0)
        strncpy(s->widget_style, val, sizeof(s->widget_style) - 1);
    else if (strcmp(key, "ColorScheme") == 0)
        strncpy(s->color_scheme, val, sizeof(s->color_scheme) - 1);
    else if (strcmp(key, "IconTheme") == 0)
        strncpy(s->icon_theme, val, sizeof(s->icon_theme) - 1);
    else if (strcmp(key, "cursorTheme") == 0)
        strncpy(s->cursor_theme, val, sizeof(s->cursor_theme) - 1);
    else if (strcmp(key, "cursorSize") == 0)
        s->cursor_size = atoi(val);
    else if (strcmp(key, "font") == 0)
        strncpy(s->general_font, val, sizeof(s->general_font) - 1);
    else if (strcmp(key, "fixed") == 0)
        strncpy(s->fixed_font, val, sizeof(s->fixed_font) - 1);
    else if (strcmp(key, "toolBarFont") == 0)
        strncpy(s->toolbar_font, val, sizeof(s->toolbar_font) - 1);
    else if (strcmp(key, "menuFont") == 0)
        strncpy(s->menu_font, val, sizeof(s->menu_font) - 1);
    else if (strcmp(key, "TitleFont") == 0)
        strncpy(s->titlebar_font, val, sizeof(s->titlebar_font) - 1);
    else if (strcmp(key, "dpi") == 0) {
        s->font_dpi = atoi(val); s->force_font_dpi = (s->font_dpi > 0);
    }
    else if (strcmp(key, "PreferDark") == 0)
        s->prefer_dark = atoi(val);
    return 0;
}

static int load_ini_file(const char *path,
                         int (*cb)(const char *g, const char *k, const char *v, void *),
                         void *user) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[KDE_MAX_STR_LEN];
    char group[256] = "";
    int lc = 0;
    while (fgets(line, sizeof(line), f)) {
        lc++;
        /* Strip newline */
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
        char *t = str_trim(line);
        if (*t == 0 || *t == ';' || *t == '#') continue;
        if (*t == '[') {
            char *end = strchr(t, ']');
            if (!end) continue;
            *end = 0;
            strncpy(group, t + 1, sizeof(group) - 1);
            group[sizeof(group) - 1] = 0;
            continue;
        }
        char *k = NULL, *v = NULL;
        if (ini_split_kv(t, &k, &v) == 0)
            cb(group, k, v, user);
    }
    fclose(f);
    (void)lc;
    (void)user;
    return 0;
}

static int kdeglobals_cb(const char *g, const char *k, const char *v, void *u) {
    return apply_kdeglobals_kv((KdeState *)u, g, k, v);
}

int kde_load_configuration(KdeState *s) {
    char path[8192];

    /* kdeglobals */
    snprintf(path, sizeof(path), "%s/kdeglobals", s->kde_config_dir);
    if (s->verbose) fprintf(stderr, "kde: loading %s\n", path);
    load_ini_file(path, kdeglobals_cb, s);

    /* kwinrc - KWin behaviour */
    snprintf(path, sizeof(path), "%s/kwinrc", s->kde_config_dir);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[KDE_MAX_STR_LEN]; char group[256] = "";
        while (fgets(line, sizeof(line), f)) {
            size_t l = strlen(line);
            while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
            char *t = str_trim(line);
            if (*t == 0 || *t == ';' || *t == '#') continue;
            if (*t == '[') {
                char *end = strchr(t, ']'); if (!end) continue; *end = 0;
                strncpy(group, t + 1, sizeof(group) - 1);
                continue;
            }
            char *k = NULL, *v = NULL;
            if (ini_split_kv(t, &k, &v) != 0) continue;
            if (strcmp(group, "Xwayland") == 0) { /* skip */ }
            else if (strcmp(group, "Windows") == 0) {
                if (strcmp(k, "FocusPolicy") == 0)
                    s->focus_policy = atoi(v);
                else if (strcmp(k, "FocusStealingPreventionLevel") == 0)
                    s->focus_stealing_prevention_level = atoi(v);
                else if (strcmp(k, "AutoRaise") == 0)
                    s->auto_raise = atoi(v);
                else if (strcmp(k, "ClickRaise") == 0)
                    s->click_raise = atoi(v);
                else if (strcmp(k, "Placement") == 0)
                    s->window_placement = atoi(v);
                else if (strcmp(k, "ReverseLayout") == 0)
                    s->reverse_layout = atoi(v);
            }
            else if (strcmp(group, "Compositing") == 0) {
                if (strcmp(k, "Enabled") == 0)
                    s->compositor_enabled = atoi(v);
                else if (strcmp(k, "VSync") == 0)
                    s->compositor_vsync = atoi(v);
                else if (strcmp(k, "GLCore") == 0) { /* ignore */ }
                else if (strcmp(k, "AnimationSpeed") == 0)
                    s->compositor_animation_speed_pct = atoi(v);
                else if (strcmp(k, "WindowOpacity") == 0)
                    s->compositor_window_opacity = atoi(v);
            }
            else if (strcmp(group, "Tiling") == 0) {
                if (strcmp(k, "Mode") == 0)
                    for (size_t x = 0; x < sizeof(TILING_MODE_NAMES)/sizeof(TILING_MODE_NAMES[0]); x++)
                        if (TILING_MODE_NAMES[x] && strcmp(v, TILING_MODE_NAMES[x]) == 0)
                            s->tiling_mode = (KdeTilingMode)x;
            }
            else if (strcmp(group, "Desktops") == 0) {
                if (strcmp(k, "Number") == 0) {
                    int n = atoi(v);
                    if (n > 0 && n <= KDE_MAX_DESKTOPS) s->desktop_count = n;
                } else if (strcmp(k, "Rows") == 0)
                    s->desktop_rows = atoi(v);
                else if (strcmp(k, "Columns") == 0)
                    s->desktop_cols = atoi(v);
            }
            else if (strcmp(group, "Edges") == 0) {
                KdeScreenEdgeAction *target = NULL;
                if      (strcmp(k, "TopLeft") == 0)     target = &s->edge_top_left;
                else if (strcmp(k, "Top") == 0)         target = &s->edge_top;
                else if (strcmp(k, "TopRight") == 0)    target = &s->edge_top_right;
                else if (strcmp(k, "Left") == 0)        target = &s->edge_left;
                else if (strcmp(k, "Right") == 0)       target = &s->edge_right;
                else if (strcmp(k, "BottomLeft") == 0)  target = &s->edge_bottom_left;
                else if (strcmp(k, "Bottom") == 0)      target = &s->edge_bottom;
                else if (strcmp(k, "BottomRight") == 0)target = &s->edge_bottom_right;
                if (target) {
                    for (size_t x = 0; x < sizeof(SCREEN_EDGE_NAMES)/sizeof(SCREEN_EDGE_NAMES[0]); x++)
                        if (SCREEN_EDGE_NAMES[x] && strcmp(v, SCREEN_EDGE_NAMES[x]) == 0)
                            *target = (KdeScreenEdgeAction)x;
                }
            }
        }
        fclose(f);
    }

    /* plasmarc / plasma-localerc */
    snprintf(path, sizeof(path), "%s/plasmarc", s->kde_config_dir);
    load_ini_file(path, kdeglobals_cb, s);

    /* baloofilerc, ksmserverrc, ksplashrc - too many to enumerate, skeleton only */

    /* Reload default color scheme object pointer */
    s->loaded_color_scheme = NULL;
    for (int i = 0; i < s->color_scheme_count; i++) {
        if (strcmp(s->available_color_schemes[i].name, s->color_scheme) == 0) {
            s->loaded_color_scheme = &s->available_color_schemes[i];
            break;
        }
    }
    return 0;
}

int kde_save_configuration(KdeState *s) {
    char path[8192];

    /* Write kdeglobals */
    snprintf(path, sizeof(path), "%s/kdeglobals", s->kde_config_dir);
    FILE *f = fopen(path, "w");
    if (!f) {
        if (s->verbose) fprintf(stderr, "kde: cannot write %s\n", path);
        return -1;
    }
    fprintf(f, "[$Version]\n");
    fprintf(f, "update_info=kdeglobals.upd:make-use-of-new-theme\n\n");

    fprintf(f, "[General]\n");
    fprintf(f, "ColorScheme=%s\n", s->color_scheme);
    fprintf(f, "LookAndFeel=%s\n", s->lookandfeel_package);
    fprintf(f, "WidgetStyle=%s\n", s->widget_style);
    fprintf(f, "IconTheme=%s\n", s->icon_theme);
    fprintf(f, "cursorTheme=%s\n", s->cursor_theme);
    fprintf(f, "cursorSize=%d\n", s->cursor_size);
    fprintf(f, "PreferDark=%d\n", s->prefer_dark);
    fprintf(f, "dpi=%d\n", s->font_dpi);

    fprintf(f, "\n[KDE]\n");
    fprintf(f, "font=%s\n", s->general_font);
    fprintf(f, "fixed=%s\n", s->fixed_font);
    fprintf(f, "toolBarFont=%s\n", s->toolbar_font);
    fprintf(f, "menuFont=%s\n", s->menu_font);
    fprintf(f, "TitleFont=%s\n", s->titlebar_font);

    fclose(f);

    /* Write kwinrc */
    snprintf(path, sizeof(path), "%s/kwinrc", s->kde_config_dir);
    f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "[$Version]\n2\n\n");
    fprintf(f, "[Compositing]\n");
    fprintf(f, "Enabled=%d\n", s->compositor_enabled);
    fprintf(f, "VSync=%d\n", s->compositor_vsync);
    fprintf(f, "AnimationSpeed=%d\n", s->compositor_animation_speed_pct);
    fprintf(f, "WindowOpacity=%d\n", s->compositor_window_opacity);
    fprintf(f, "\n[Windows]\n");
    fprintf(f, "FocusPolicy=%d\n", s->focus_policy);
    fprintf(f, "FocusStealingPreventionLevel=%d\n", s->focus_stealing_prevention_level);
    fprintf(f, "AutoRaise=%d\n", s->auto_raise);
    fprintf(f, "ClickRaise=%d\n", s->click_raise);
    fprintf(f, "Placement=%d\n", s->window_placement);
    fprintf(f, "ReverseLayout=%d\n", s->reverse_layout);
    fprintf(f, "\n[Tiling]\n");
    fprintf(f, "Mode=%s\n", tiling_mode_name(s->tiling_mode));
    fprintf(f, "\n[Desktops]\n");
    fprintf(f, "Number=%d\n", s->desktop_count);
    fprintf(f, "Rows=%d\n", s->desktop_rows);
    fprintf(f, "Columns=%d\n", s->desktop_cols);
    fprintf(f, "\n[Edges]\n");
    fprintf(f, "TopLeft=%s\n",     screen_edge_name(s->edge_top_left));
    fprintf(f, "Top=%s\n",         screen_edge_name(s->edge_top));
    fprintf(f, "TopRight=%s\n",    screen_edge_name(s->edge_top_right));
    fprintf(f, "Left=%s\n",        screen_edge_name(s->edge_left));
    fprintf(f, "Right=%s\n",      screen_edge_name(s->edge_right));
    fprintf(f, "BottomLeft=%s\n", screen_edge_name(s->edge_bottom_left));
    fprintf(f, "Bottom=%s\n",     screen_edge_name(s->edge_bottom));
    fprintf(f, "BottomRight=%s\n",screen_edge_name(s->edge_bottom_right));
    fclose(f);

    /* Write plasmarc */
    snprintf(path, sizeof(path), "%s/plasmarc", s->kde_config_dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "[PlasmaViews][Panel Defaults]\n");
        fprintf(f, "HorizontalAlignment=center\n");
        fprintf(f, "\n[Org.kde.plasma.desktopcontainment]\n");
        fprintf(f, "DefaultDesktopLayout=%s\n", desktop_layout_name(s->desktop_layout));
        fclose(f);
    }
    return 0;
}

/* ============================================================= */
/* 4) Presets                                                    */
/* ============================================================= */

int kde_apply_default_preset(KdeState *s) {
    strcpy(s->lookandfeel_package, "org.kde.breeze.desktop");
    strcpy(s->global_theme_display, "Breeze");
    s->prefer_dark = 0;
    strcpy(s->widget_style, "Breeze");
    strcpy(s->color_scheme, "BreezeLight");
    strcpy(s->icon_theme, "breeze");
    strcpy(s->cursor_theme, "Breeze");
    s->cursor_size = 24;
    s->decoration = KDE_DECOR_BREEZE;
    strcpy(s->window_decoration_theme, "Breeze");

    /* Panels: top + bottom by default */
    if (s->panel_count == 0) {
        int idx = -1;
        kde_panel_add(s, KDE_PANEL_TOP, -1, &idx);
        if (idx >= 0) {
            s->panels[idx].size_h = 32;
            s->panels[idx].opacity = 100;
            s->panels[idx].auto_hide = 0;
            kde_widget_add(s, idx, KDE_WIDGET_APP_LAUNCHER);
            kde_widget_add(s, idx, KDE_WIDGET_TASKBAR);
            kde_widget_add(s, idx, KDE_WIDGET_SYSTEM_TRAY);
            kde_widget_add(s, idx, KDE_WIDGET_DIGITAL_CLOCK);
        }
        kde_panel_add(s, KDE_PANEL_BOTTOM, -1, &idx);
        if (idx >= 0) {
            s->panels[idx].size_h = 44;
            kde_widget_add(s, idx, KDE_WIDGET_APP_LAUNCHER);
        }
    }

    /* Default desktops */
    s->desktop_count = 2;
    s->desktop_rows = 1; s->desktop_cols = 2;

    /* Default hotkeys */
    s->hotkey_count = 0;
    {
        KdeHotkey hk; memset(&hk, 0, sizeof(hk));
        strcpy(hk.component, "krunner");
        strcpy(hk.action_id, "_launch_krunner");
        strcpy(hk.action_label, "Launch KRunner");
        strcpy(hk.key_seq, "Alt+Space");
        strcpy(hk.exec, "krunner");
        hk.enabled = 1;
        kde_hotkey_register(s, &hk);

        memset(&hk, 0, sizeof(hk));
        strcpy(hk.component, "konsole");
        strcpy(hk.action_id, "_open_terminal");
        strcpy(hk.action_label, "Open Konsole");
        strcpy(hk.key_seq, "Ctrl+Alt+T");
        strcpy(hk.exec, "konsole");
        hk.enabled = 1;
        kde_hotkey_register(s, &hk);
    }
    return 0;
}

int kde_apply_minimal_preset(KdeState *s) {
    kde_apply_default_preset(s);
    s->effects_mask = KDE_EFFECT_BLUR | KDE_EFFECT_SCREEN_EDGE;
    s->compositor_animation_speed_pct = 50;
    s->compositor_window_opacity = 100;
    s->desktop_count = 1;
    s->desktop_rows = 1; s->desktop_cols = 1;
    s->menu_style = KDE_MENU_KICKER;          /* compact */
    s->taskbar_icon_only = 1;
    s->enable_sounds = 0;
    s->enable_notification_sounds = 0;
    s->enable_feedback_sounds = 0;
    /* Remove all but one panel */
    while (s->panel_count > 1) {
        s->panel_count--;
        s->panels[s->panel_count].widget_count = 0;
    }
    /* No screen edges */
    s->edge_top = s->edge_top_left = s->edge_top_right = KDE_EDGE_NONE;
    s->edge_left = s->edge_right = KDE_EDGE_NONE;
    s->edge_bottom = s->edge_bottom_left = s->edge_bottom_right = KDE_EDGE_NONE;
    return 0;
}

int kde_apply_mobile_preset(KdeState *s) {
    s->session_type = KDE_SESSION_PLASMA_MOBILE;
    strcpy(s->lookandfeel_package, "org.kde.plasma.mobile.shell");
    strcpy(s->global_theme_display, "Plasma Mobile");
    s->prefer_dark = 1;
    strcpy(s->color_scheme, "BreezeDark");
    strcpy(s->widget_style, "Breeze");
    strcpy(s->icon_theme, "breeze-dark");
    s->cursor_size = 18;
    s->font_dpi = 120; s->force_font_dpi = 1;
    s->compositor_enabled = 1;
    s->desktop_count = 1;
    s->desktop_rows = 1; s->desktop_cols = 1;
    s->tiling_mode = KDE_TILING_MASTER_STACK;
    /* Single bottom-of-screen panel with mobile app launcher */
    while (s->panel_count > 0) {
        s->panel_count--;
        s->panels[s->panel_count].widget_count = 0;
    }
    int idx = -1;
    kde_panel_add(s, KDE_PANEL_BOTTOM, 0, &idx);
    if (idx >= 0) {
        s->panels[idx].size_h = 56;
        s->panels[idx].opacity = 80;
        s->panels[idx].position_pct = 50;
        s->panels[idx].span_pct = 100;
        kde_widget_add(s, idx, KDE_WIDGET_APP_LAUNCHER);
        kde_widget_add(s, idx, KDE_WIDGET_SYSTEM_TRAY);
        kde_widget_add(s, idx, KDE_WIDGET_BATTERY);
        kde_widget_add(s, idx, KDE_WIDGET_NETWORK);
    }
    /* Touchpad on by default for mobile */
    s->touchpad.tap_to_click = 1;
    s->touchpad.natural_scroll = 1;
    s->touchscreen.enabled = 1;
    return 0;
}

/* ============================================================= */
/* 5) Session management                                         */
/* ============================================================= */

static pid_t spawn_child(const char *path, char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("kde: fork");
        return -1;
    }
    if (pid == 0) {
        /* child */
        execvp(path, argv);
        perror(path);
        _exit(127);
    }
    return pid;
}

static pid_t spawn_simple(const char *cmd) {
    char *argv[] = { (char *)cmd, NULL };
    return spawn_child(cmd, argv);
}

int kde_start_session(KdeState *s) {
    if (s->running) {
        if (s->verbose) fprintf(stderr, "kde: session already running\n");
        return -1;
    }

    if (s->verbose)
        fprintf(stderr, "kde: starting session %s as %s@%s\n",
                session_type_name(s->session_type), s->username, s->hostname);

    /* Make sure runtime dirs exist */
    mkdir(s->xdg_runtime_dir, 0700);
    mkdir(s->kde_config_dir, 0755);
    mkdir(s->kde_data_dir,   0755);

    /* Set environment variables for child processes */
    setenv("XDG_RUNTIME_DIR",  s->xdg_runtime_dir,   1);
    setenv("XDG_DATA_DIRS",    s->xdg_data_dirs,     1);
    setenv("XDG_CONFIG_DIRS",  s->xdg_config_dirs,  1);
    setenv("KDEHOME",          s->kde_home,          1);
    setenv("DISPLAY",          s->display,           1);
    if (s->session_type == KDE_SESSION_PLASMA_WAYLAND
     || s->session_type == KDE_SESSION_PLASMA_MOBILE
     || s->session_type == KDE_SESSION_PLASMA_BIGD)
        setenv("WAYLAND_DISPLAY", s->wayland_display, 1);
    setenv("XDG_SESSION_TYPE",
           (s->session_type == KDE_SESSION_PLASMA_WAYLAND) ? "wayland" : "x11", 1);
    setenv("XDG_CURRENT_DESKTOP", "KDE", 1);
    setenv("LANG", s->language, 1);

    /* Start KDED daemon */
    s->pid_kded = spawn_simple("kded");
    /* Start KScreen / kglobalaccel before KWin */
    spawn_simple("kglobalaccel");

    /* Start KWin / KSmserver */
    s->pid_ksmserver = spawn_simple("ksmserver");
    s->pid_kwin = spawn_simple("kwin_x11");
    if (s->session_type == KDE_SESSION_PLASMA_WAYLAND)
        s->pid_kwin = spawn_simple("kwin_wayland");

    /* Start Plasmashell */
    s->pid_plasmashell = spawn_simple("plasmashell");

    /* Start KRunner */
    s->pid_krunner = spawn_simple("krunner");

    /* Polkit */
    s->pid_polkit = spawn_simple("polkit-kde-authentication-agent-1");

    s->running = 1;
    s->started_at = time(NULL);
    s->exit_code = 0;
    s->startup_finished = 1;
    return 0;
}

int kde_run_session_loop(KdeState *s) {
    if (!s->running) return -1;
    /* Skeleton: wait for any child to exit, then return. */
    int status = 0;
    pid_t w = waitpid(s->pid_plasmashell, &status, 0);
    if (w < 0 && errno == EINTR) return 0;
    if (w < 0) {
        if (s->verbose) fprintf(stderr, "kde: waitpid: %s\n", strerror(errno));
        return -1;
    }
    if (WIFEXITED(status))      s->exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) s->exit_code = 128 + WTERMSIG(status);
    return s->exit_code;
}

void kde_end_session(KdeState *s, int exit_code, int kill_daemons) {
    if (!s) return;
    s->exit_code = exit_code;

    if (kill_daemons) {
        struct { const char *name; pid_t *pid; } procs[] = {
            { "krunner",   &s->pid_krunner },
            { "plasmashell", &s->pid_plasmashell },
            { "kwin",      &s->pid_kwin },
            { "ksmserver", &s->pid_ksmserver },
            { "kded",      &s->pid_kded },
            { "polkit",    &s->pid_polkit },
        };
        for (size_t i = 0; i < sizeof(procs)/sizeof(procs[0]); i++) {
            if (*procs[i].pid > 0) {
                if (s->verbose)
                    fprintf(stderr, "kde: terminating %s (pid=%d)\n",
                            procs[i].name, (int)*procs[i].pid);
                kill(*procs[i].pid, SIGTERM);
            }
        }
        usleep(150000);
        for (size_t i = 0; i < sizeof(procs)/sizeof(procs[0]); i++) {
            if (*procs[i].pid > 0)
                kill(*procs[i].pid, SIGKILL);
            *procs[i].pid = 0;
        }
    }

    s->running = 0;
    s->startup_finished = 0;
}

/* ============================================================= */
/* 6) Panels / widgets / workspaces                             */
/* ============================================================= */

int kde_panel_add(KdeState *s, KdePanelLocation loc, int screen, int *out_idx) {
    if (s->panel_count >= KDE_MAX_PANELS) {
        if (out_idx) *out_idx = -1;
        return -1;
    }
    int idx = s->panel_count++;
    KdePanel *p = &s->panels[idx];
    memset(p, 0, sizeof(*p));
    snprintf(p->id, sizeof(p->id), "panel-%d", idx);
    strcpy(p->plugin, "org.kde.panel");
    snprintf(p->name, sizeof(p->name), "Panel %d", idx + 1);
    p->location = loc;
    p->size_h = (loc == KDE_PANEL_LEFT || loc == KDE_PANEL_RIGHT) ? 36 : 32;
    p->position_pct = 50;
    p->span_pct = 100;
    p->floating_margin = 0;
    p->opacity = 100;
    p->auto_hide = 0;
    p->visible_on_all_desktops = 1;
    p->visible_on_all_activities = 1;
    p->screen = screen;
    p->widget_count = 0;
    if (out_idx) *out_idx = idx;
    if (s->verbose)
        fprintf(stderr, "kde: added panel '%s' loc=%s screen=%d\n",
                p->id, panel_loc_name(loc), screen);
    return 0;
}

int kde_widget_add(KdeState *s, int panel_id, KdeWidgetType type) {
    if (panel_id < 0 || panel_id >= s->panel_count) {
        if (s->verbose)
            fprintf(stderr, "kde: invalid panel_id %d\n", panel_id);
        return -1;
    }
    KdePanel *p = &s->panels[panel_id];
    if (p->widget_count >= KDE_MAX_WIDGETS) return -1;
    if (s->plasmoid_count >= KDE_MAX_PLASMOIDS) return -1;

    int pid = s->plasmoid_count++;
    KdePlasmoid *w = &s->plasmoids[pid];
    memset(w, 0, sizeof(*w));
    w->id = pid;
    strncpy(w->plugin, widget_type_name(type), sizeof(w->plugin) - 1);
    snprintf(w->title, sizeof(w->title), "%s #%d", w->plugin, pid);
    w->type = type;
    w->panel_id = panel_id;
    w->containment_desktop = -1;
    w->config_json[0] = 0;

    p->widget_ids[p->widget_count++] = pid;
    if (s->verbose)
        fprintf(stderr, "kde: added widget %s to panel %d\n", w->plugin, panel_id);
    return pid;
}

int kde_workspace_switch(KdeState *s, int idx) {
    if (idx < 0 || idx >= s->desktop_count) return -1;
    s->current_desktop = idx;
    if (s->verbose)
        fprintf(stderr, "kde: switched to desktop %d (%s)\n",
                idx + 1, s->desktops[idx].name);
    /* In real impl, would call org.kde.KWin setCurrentDesktop */
    return 0;
}

/* ============================================================= */
/* 7) App scanning / launching                                   */
/* ============================================================= */

static int parse_desktop_file(const char *path, KdeAppEntry *app) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    memset(app, 0, sizeof(*app));
    strncpy(app->desktop_file, path, sizeof(app->desktop_file) - 1);

    char line[KDE_MAX_STR_LEN];
    char group[64] = "";
    int in_desktop_entry = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
        char *t = str_trim(line);
        if (*t == 0 || *t == '#') continue;
        if (*t == '[') {
            char *end = strchr(t, ']');
            if (!end) continue;
            *end = 0;
            strncpy(group, t + 1, sizeof(group) - 1);
            in_desktop_entry = (strcmp(group, "Desktop Entry") == 0);
            continue;
        }
        if (!in_desktop_entry) continue;
        char *k = NULL, *v = NULL;
        if (ini_split_kv(t, &k, &v) != 0) continue;
        if      (strcmp(k, "Name") == 0)           strncpy(app->name, v, sizeof(app->name) - 1);
        else if (strcmp(k, "GenericName") == 0)    strncpy(app->generic_name, v, sizeof(app->generic_name) - 1);
        else if (strcmp(k, "Comment") == 0)        strncpy(app->comment, v, sizeof(app->comment) - 1);
        else if (strcmp(k, "Keywords") == 0)       strncpy(app->keywords, v, sizeof(app->keywords) - 1);
        else if (strcmp(k, "Categories") == 0)    strncpy(app->categories, v, sizeof(app->categories) - 1);
        else if (strcmp(k, "Exec") == 0)           strncpy(app->exec, v, sizeof(app->exec) - 1);
        else if (strcmp(k, "TryExec") == 0)        strncpy(app->tryexec, v, sizeof(app->tryexec) - 1);
        else if (strcmp(k, "Icon") == 0)          strncpy(app->icon, v, sizeof(app->icon) - 1);
        else if (strcmp(k, "Path") == 0)          strncpy(app->path, v, sizeof(app->path) - 1);
        else if (strcmp(k, "Terminal") == 0)       app->is_terminal = (strcmp(v, "true") == 0);
        else if (strcmp(k, "NoDisplay") == 0)     app->nodisplay = (strcmp(v, "true") == 0);
        else if (strcmp(k, "Hidden") == 0)         app->hidden = (strcmp(v, "true") == 0);
        else if (strcmp(k, "StartupNotify") == 0) app->startup_notify = (strcmp(v, "true") == 0);
        else if (strcmp(k, "StartupWMClass") == 0) strncpy(app->startup_wm_class, v, sizeof(app->startup_wm_class) - 1);
        else if (strcmp(k, "MimeType") == 0)      strncpy(app->mime_types, v, sizeof(app->mime_types) - 1);
    }
    fclose(f);
    return 0;
}

int kde_apps_scan(KdeState *s) {
    s->app_count = 0;
    for (int d = 0; APP_SEARCH_DIRS[d]; d++) {
        DIR *dir = opendir(APP_SEARCH_DIRS[d]);
        if (!dir) continue;
        struct dirent *de;
        while ((de = readdir(dir)) != NULL) {
            size_t nl = strlen(de->d_name);
            if (nl < 8 || strcmp(de->d_name + nl - 8, ".desktop") != 0)
                continue;
            if (s->app_count >= KDE_MAX_APPS) break;
            char path[8192];
            snprintf(path, sizeof(path), "%s/%s", APP_SEARCH_DIRS[d], de->d_name);
            KdeAppEntry *app = &s->apps[s->app_count];
            if (parse_desktop_file(path, app) == 0
                && !app->nodisplay && !app->hidden && app->name[0]) {
                app->preselect_score = 0;
                s->app_count++;
            }
        }
        closedir(dir);
    }
    if (s->verbose)
        fprintf(stderr, "kde: scanned %d applications\n", s->app_count);
    return s->app_count;
}

int kde_apps_search(KdeState *s, const char *query, int *indices, int max) {
    if (!query || !indices || max <= 0) return 0;
    int found = 0;
    for (int i = 0; i < s->app_count && found < max; i++) {
        KdeAppEntry *a = &s->apps[i];
        if (kde_stristr(a->name, query)
         || kde_stristr(a->generic_name, query)
         || kde_stristr(a->comment, query)
         || kde_stristr(a->keywords, query)
         || kde_stristr(a->categories, query)) {
            indices[found++] = i;
        }
    }
    return found;
}

/* Very small Exec parser: extracts the binary name; ignores %f/%u etc. */
static int build_exec_argv(const KdeAppEntry *app, char **argv_extra,
                           char **out_argv, int out_max) {
    static char exec_buf[4096];
    char *tok, *saveptr = NULL;
    strncpy(exec_buf, app->exec, sizeof(exec_buf) - 1);
    exec_buf[sizeof(exec_buf) - 1] = 0;
    int ai = 0;
    if (out_max <= 0) return 0;
    tok = strtok_r(exec_buf, " \t", &saveptr);
    while (tok && ai < out_max - 1) {
        /* Strip field codes like %f %u %d %U etc. */
        if (tok[0] == '%') { tok = strtok_r(NULL, " \t", &saveptr); continue; }
        out_argv[ai++] = tok;
        tok = strtok_r(NULL, " \t", &saveptr);
    }
    if (argv_extra) {
        for (int i = 0; argv_extra[i] && ai < out_max - 1; i++)
            out_argv[ai++] = argv_extra[i];
    }
    out_argv[ai] = NULL;
    return ai;
}

int kde_app_launch(const KdeAppEntry *app, char **argv_extra) {
    if (!app || !app->exec[0]) return -1;
    char *argv[64];
    int argc = build_exec_argv(app, argv_extra, argv, 64);
    if (argc == 0) return -1;
    /* Honour TryExec if present and binary not found */
    if (app->tryexec[0]) {
        if (access(app->tryexec, X_OK) != 0)
            return -1;
    }
    pid_t pid = fork();
    if (pid < 0) { perror("kde: fork"); return -1; }
    if (pid == 0) {
        if (app->path[0]) chdir(app->path);
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    return (int)pid;
}

int kde_krunner_run(KdeState *s, const char *query) {
    if (!query) return -1;
    /* Search apps first */
    int idx[KDE_MAX_APPS];
    int n = kde_apps_search(s, query, idx, KDE_MAX_APPS);
    if (n > 0) {
        if (s->verbose)
            fprintf(stderr, "kde: krunner launching '%s'\n", s->apps[idx[0]].name);
        return kde_app_launch(&s->apps[idx[0]], NULL);
    }
    /* Fall back to running the query as a shell command */
    char *argv[] = { (char *)"sh", (char *)"-c", (char *)query, NULL };
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) { execvp("sh", argv); _exit(127); }
    return (int)pid;
}

/* ============================================================= */
/* 8) Notifications                                              */
/* ============================================================= */

uint32_t kde_notify(KdeState *s, const KdeNotification *in) {
    if (!s || !in) return 0;
    if (s->notify_do_not_disturb && in->urgency < 2) {
        /* Suppressed by DND unless critical */
        return 0;
    }
    if (s->notification_count >= KDE_MAX_NOTIFICATIONS) {
        /* Drop oldest */
        memmove(&s->notifications[0], &s->notifications[1],
                (KDE_MAX_NOTIFICATIONS - 1) * sizeof(KdeNotification));
        s->notification_count = KDE_MAX_NOTIFICATIONS - 1;
    }
    KdeNotification *n = &s->notifications[s->notification_count++];
    *n = *in;
    n->id = next_notification_id(s);
    if (n->created_at == 0) n->created_at = time(NULL);
    if (n->expire_timeout_ms == 0) n->expire_timeout_ms = s->notify_popup_timeout_ms;
    if (s->verbose)
        fprintf(stderr, "kde: notify id=%u [%s] %s: %s\n",
                n->id, n->app_name, n->summary, n->body);
    s->notify_badge_count++;
    return n->id;
}

int kde_notify_close(KdeState *s, uint32_t id) {
    if (!s || id == 0) return -1;
    for (int i = 0; i < s->notification_count; i++) {
        if (s->notifications[i].id == id) {
            s->notifications[i].dismissed = 1;
            /* compact the list */
            memmove(&s->notifications[i], &s->notifications[i + 1],
                    (s->notification_count - i - 1) * sizeof(KdeNotification));
            s->notification_count--;
            if (s->notify_badge_count > 0) s->notify_badge_count--;
            return 0;
        }
    }
    return -1;
}

/* ============================================================= */
/* 9) Hotkeys / KWin rules                                      */
/* ============================================================= */

int kde_hotkey_register(KdeState *s, const KdeHotkey *hk) {
    if (!s || !hk) return -1;
    /* If same component+action_id exists, replace */
    for (int i = 0; i < s->hotkey_count; i++) {
        if (strcmp(s->hotkeys[i].component, hk->component) == 0
         && strcmp(s->hotkeys[i].action_id, hk->action_id) == 0) {
            s->hotkeys[i] = *hk;
            return i;
        }
    }
    if (s->hotkey_count >= KDE_MAX_HOTKEYS) return -1;
    s->hotkeys[s->hotkey_count++] = *hk;
    return s->hotkey_count - 1;
}

int kde_hotkey_trigger(KdeState *s, const char *key_seq) {
    if (!s || !key_seq) return -1;
    for (int i = 0; i < s->hotkey_count; i++) {
        KdeHotkey *h = &s->hotkeys[i];
        if (!h->enabled) continue;
        if (strcmp(h->key_seq, key_seq) == 0 || strcmp(h->alt_seq, key_seq) == 0) {
            if (h->exec[0]) {
                pid_t pid = fork();
                if (pid == 0) {
                    char *argv[] = { (char *)"sh", (char *)"-c", (char *)h->exec, NULL };
                    execvp("sh", argv);
                    _exit(127);
                }
                return (int)pid;
            }
            /* DBus invocation would go here - skeleton only */
            if (h->dbus_service[0]) {
                if (s->verbose)
                    fprintf(stderr, "kde: dbus call %s %s on %s (skipped)\n",
                            h->dbus_iface, h->dbus_method, h->dbus_service);
                return 0;
            }
        }
    }
    return -1;
}

int kde_kwin_rule_add(KdeState *s, const KdeKwinRule *r) {
    if (!s || !r) return -1;
    if (s->kwin_rule_count >= KDE_MAX_KWIN_RULES) return -1;
    s->kwin_rules[s->kwin_rule_count++] = *r;
    if (s->verbose)
        fprintf(stderr, "kde: added kwin rule for wmclass='%s'\n",
                r->wm_class[0][0] ? r->wm_class[0] : "*");
    return s->kwin_rule_count - 1;
}

/* ============================================================= */
/* 10) KCM modules                                              */
/* ============================================================= */

int kde_kcm_launch(KdeState *s, const char *kcm_id) {
    if (!kcm_id) return -1;
    /* Prefer systemsettings, fall back to kcmshell5/6 */
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "systemsettings kcm_%s", kcm_id);
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char *argv[] = { (char *)"sh", (char *)"-c", cmd, NULL };
        execvp("sh", argv);
        _exit(127);
    }
    if (s->verbose)
        fprintf(stderr, "kde: launched kcm '%s' pid=%d\n", kcm_id, (int)pid);
    return (int)pid;
}

static int is_kcm_file(const struct dirent *de) {
    if (!de) return 0;
    size_t l = strlen(de->d_name);
    if (l < 7) return 0;
    return strncmp(de->d_name, "kcm_", 4) == 0
        && strcmp(de->d_name + l - 3, ".so") == 0;
}

int kde_kcm_scan(KdeState *s) {
    /* Skeleton KCM scan: we just register a handful of well-known KCMs */
    static const struct { const char *id, *name, *cat, *icon; } well_known[] = {
        { "kcm_lookandfeel",    "Global Theme",        "appearance",  "look-and-feel" },
        { "kcm_colors",         "Colors",              "appearance",  "preferences-desktop-color" },
        { "kcm_fonts",          "Fonts",               "appearance",  "preferences-desktop-font" },
        { "kcm_icons",          "Icons",               "appearance",  "preferences-desktop-icons" },
        { "kcm_cursors",        "Cursors",             "appearance",  "preferences-desktop-cursors" },
        { "kcm_style",          "Application Style",   "appearance",  "preferences-desktop-style" },
        { "kcm_desktoptheme",   "Plasma Style",        "appearance",  "preferences-desktop-theme" },
        { "kcm_wallpaper",      "Wallpaper",           "appearance",  "preferences-desktop-wallpaper" },
        { "kcm_splashscreen",   "Splash Screen",       "appearance",  "preferences-system-splash" },
        { "kcm_keys",           "Shortcuts",           "input",       "preferences-desktop-keyboard" },
        { "kcm_mouse",          "Mouse",               "input",       "preferences-desktop-mouse" },
        { "kcm_touchpad",       "Touchpad",            "input",       "preferences-desktop-touchpad" },
        { "kcm_keyboard",       "Keyboard",           "input",       "preferences-desktop-keyboard" },
        { "kcm_kded",           "Background Services", "kde",         "preferences-system-windows" },
        { "kcm_sddm",           "Login Screen (SDDM)", "workspace",   "preferences-system-windows" },
        { "kcm_powerdevil",     "Power Management",    "hardware",    "preferences-system-power-management" },
        { "kcm_audio",          "Audio",               "hardware",    "preferences-desktop-sound" },
        { "kcm_joystick",       "Joystick",            "hardware",    "input-joystick" },
        { "kcm_bluetooth",      "Bluetooth",           "hardware",    "preferences-system-bluetooth" },
        { "kcm_networkmanagement", "Network",          "network",     "preferences-system-network" },
        { "kcm_filetypes",      "File Associations",  "workspace",   "preferences-desktop-filetype-association" },
        { "kcm_componentchooser", "Default Applications","workspace", "preferences-desktop-default-applications" },
        { "kcm_nightcolor",     "Night Color",         "display",     "preferences-desktop-display-nightcolor" },
        { "kcm_kwin",           "Window Management",  "workspace",   "preferences-system-windows" },
        { "kcm_kwinscreenedges","Screen Edges",        "workspace",   "preferences-system-windows-effect" },
        { "kcm_display",        "Display",            "display",     "preferences-desktop-display" },
        { "kcm_drives",         "Storage Devices",    "hardware",    "preferences-devices" },
        { "kcm_users",          "User Account",       "workspace",   "preferences-system-user" },
        { "kcm_clock",          "Date & Time",        "system",      "preferences-system-time" },
        { "kcm_locale",         "Region & Language",  "system",      "preferences-desktop-locale" },
        { "kcm_smserver",       "Session",            "workspace",   "preferences-system-windows" },
        { "kcm_notifications",  "Notifications",      "workspace",   "preferences-desktop-notification" },
        { "kcm_kwintiling",     "Tiling",             "workspace",   "preferences-system-windows" },
        { "kcm_baloo",          "File Search",        "workspace",   "preferences-system-search" },
        { "kcm_grub",           "Boot Loader",        "system",      "preferences-system-bootloader" },
        { "kcm_pulseaudio",     "Audio Volume",       "hardware",    "preferences-desktop-multimedia" },
    };

    int count = 0;
    for (size_t i = 0; i < sizeof(well_known)/sizeof(well_known[0]); i++) {
        /* also try a real .so file under KCM search dirs */
        char so_path[8192]; int found = 0;
        for (int d = 0; KCM_SEARCH_DIRS[d] && !found; d++) {
            DIR *dir = opendir(KCM_SEARCH_DIRS[d]);
            if (!dir) continue;
            struct dirent *de;
            while ((de = readdir(dir)) != NULL) {
                if (is_kcm_file(de) && strcmp(de->d_name, well_known[i].id) == 0) {
                    snprintf(so_path, sizeof(so_path), "%s/%s",
                             KCM_SEARCH_DIRS[d], de->d_name);
                    found = 1; break;
                }
            }
            closedir(dir);
        }
        /* Register KCM entry (skeleton: we don't store the list in state, but
           the caller can iterate the well_known table via kde_kcm_launch) */
        if (count < KDE_MAX_KCM_MODULES) {
            s->start_kcm_after_login[count] = (int)i; /* reuse as a presence marker */
            count++;
        }
        if (s->verbose && found)
            fprintf(stderr, "kde: kcm %s found at %s\n", well_known[i].id, so_path);
        (void)found;
    }
    if (s->verbose) fprintf(stderr, "kde: scanned %d kcm modules\n", count);
    return count;
}

/* ============================================================= */
/* 11) Display configuration                                    */
/* ============================================================= */

int kde_screen_configure(KdeState *s, int screen_idx, int w, int h,
                        int scale, int rotate) {
    if (screen_idx < 0 || screen_idx >= s->screen_count) return -1;
    if (w > 0) s->screens[screen_idx].width = w;
    if (h > 0) s->screens[screen_idx].height = h;
    if (scale > 0) s->screens[screen_idx].scale = scale;
    if (rotate == 0 || rotate == 90 || rotate == 180 || rotate == 270)
        s->screens[screen_idx].rotation = rotate;
    if (s->verbose)
        fprintf(stderr, "kde: screen %d set to %dx%d scale=%d rotate=%d\n",
                screen_idx,
                s->screens[screen_idx].width, s->screens[screen_idx].height,
                s->screens[screen_idx].scale, s->screens[screen_idx].rotation);
    return 0;
}

int kde_color_scheme_set(KdeState *s, const char *scheme_name) {
    if (!scheme_name) return -1;
    strncpy(s->color_scheme, scheme_name, sizeof(s->color_scheme) - 1);

    /* Look it up in the available list */
    s->loaded_color_scheme = NULL;
    for (int i = 0; i < s->color_scheme_count; i++) {
        if (strcmp(s->available_color_schemes[i].name, scheme_name) == 0) {
            s->loaded_color_scheme = &s->available_color_schemes[i];
            s->prefer_dark = s->available_color_schemes[i].is_dark;
            break;
        }
    }
    /* Built-in defaults for very common schemes */
    if (!s->loaded_color_scheme) {
        if (strcmp(scheme_name, "BreezeDark") == 0) s->prefer_dark = 1;
        else if (strcmp(scheme_name, "BreezeLight") == 0) s->prefer_dark = 0;
        /* Adjust decoration to match */
        if (s->prefer_dark) s->decoration = KDE_DECOR_BREEZE_DARK;
        else                 s->decoration = KDE_DECOR_BREEZE;
        strcpy(s->window_decoration_theme, window_decor_name(s->decoration));
    }
    if (s->verbose)
        fprintf(stderr, "kde: color scheme set to '%s' (dark=%d)\n",
                s->color_scheme, s->prefer_dark);
    return 0;
}

/* ============================================================= */
/* 12) Helper functions                                          */
/* ============================================================= */

void kde_print_version(void) {
    printf("%s\n", KDE_VERSION_STR);
    printf("Session types:");
    for (size_t i = 0; i < sizeof(SESSION_TYPE_NAMES)/sizeof(SESSION_TYPE_NAMES[0]); i++)
        if (SESSION_TYPE_NAMES[i]) printf(" %s", SESSION_TYPE_NAMES[i]);
    printf("\n");
    printf("KenuxK KDE wrapper; delegates to native plasmashell / kwin binaries on host.\n");
}

void kde_print_help(void) {
    printf("%s - KenuxK KDE Plasma desktop launcher (minimal wrapper)\n\n", KDE_VERSION_STR);
    printf("USAGE: kde [opts]\n\n");
    printf("SESSION:\n");
    printf("  --session TYPE         plasma-x11 (default), plasma-wayland, plasma-mobile,\n");
    printf("                         plasma-bigscreen, plasma-active\n");
    printf("  --wayland / --x11 / --mobile / --bigscreen  Shortcuts for --session TYPE\n");
    printf("  --display :N           X11 display (default :0)\n");
    printf("  --wayland-display NAME Wayland socket name\n");
    printf("  --headless             Headless (no display)\n");
    printf("\nUSER / LOCALE:\n");
    printf("  --user NAME            Set session user (default $USER)\n");
    printf("  --home DIR             Override home directory\n");
    printf("  --shell PATH           Override login shell\n");
    printf("  --lang LOCALE          Set LANGUAGE/region (e.g. en_US.UTF-8)\n");
    printf("\nAPPEARANCE:\n");
    printf("  --look-and-feel PKG    Global theme package, e.g. org.kde.breeze.desktop\n");
    printf("  --theme NAME           Display name for global theme\n");
    printf("  --color-scheme NAME    Color scheme (BreezeLight / BreezeDark / ...)\n");
    printf("  --icon-theme NAME      Icon theme\n");
    printf("  --cursor-theme NAME    Cursor theme\n");
    printf("  --cursor-size N        Cursor size (px)\n");
    printf("  --widget-style NAME    Widget style (Breeze / Oxygen / Kvantum)\n");
    printf("  --font FONTDESC        General font\n");
    printf("  --fixed-font FONTDESC  Fixed-width font\n");
    printf("  --font-dpi N           Force font DPI (96/120/144)\n");
    printf("  --dark / --light       Prefer dark/light color scheme\n");
    printf("\nWORKSPACE:\n");
    printf("  --desktops N           Number of virtual desktops\n");
    printf("  --menu TYPE            kickoff / kicker / dashboard / homerun / tiled\n");
    printf("  --tiling MODE          off / floating / tiled / bsp / master-stack / tabular\n");
    printf("\nLOGIN:\n");
    printf("  --autologin USER       Auto-login as USER at SDDM\n");
    printf("  --no-autologin         Disable auto-login\n");
    printf("\nHARDWARE / NETWORK:\n");
    printf("  --airplane             Disable Wi-Fi and Bluetooth\n");
    printf("  --probe-hardware       Probe screens, touchpad, battery\n");
    printf("\nCONFIGURATION:\n");
    printf("  --load-config          Read kdeglobals/kwinrc\n");
    printf("  --save-config          Write kdeglobals/kwinrc\n");
    printf("  --scan-apps            Scan /usr/share/applications for .desktop files\n");
    printf("  --scan-kcms            Scan KCM modules\n");
    printf("  --kcm ID               Launch a KCM module (kcm_colors etc.)\n");
    printf("\nINTERACTIVE:\n");
    printf("  --notify SUMMARY BODY  Show a test notification\n");
    printf("  --run QUERY            Run query via KRunner\n");
    printf("\nPRESETS:\n");
    printf("  --preset default       Default Plasma desktop (Breeze, 2 panels)\n");
    printf("  --preset minimal       Minimal panels and effects\n");
    printf("  --preset mobile        Plasma Mobile shell\n");
    printf("\nMISC:\n");
    printf("  -v / --verbose         Verbose output\n");
    printf("  --debug                Enable debug output\n");
    printf("  --dry-run              Don't actually start anything\n");
    printf("  -V / --version         Print version and exit\n");
    printf("  -h / --help            This help\n");
}

int kde_apply_lookandfeel(KdeState *s, const char *package) {
    if (!package) return -1;
    strncpy(s->lookandfeel_package, package, sizeof(s->lookandfeel_package) - 1);
    /* A few well-known packages apply their associated defaults */
    if (strcmp(package, "org.kde.breeze.desktop") == 0) {
        kde_color_scheme_set(s, "BreezeLight");
        strcpy(s->widget_style, "Breeze");
        strcpy(s->icon_theme, "breeze");
        strcpy(s->cursor_theme, "Breeze");
        s->decoration = KDE_DECOR_BREEZE;
    } else if (strcmp(package, "org.kde.breezedark.desktop") == 0) {
        kde_color_scheme_set(s, "BreezeDark");
        strcpy(s->widget_style, "Breeze");
        strcpy(s->icon_theme, "breeze-dark");
        s->decoration = KDE_DECOR_BREEZE_DARK;
    } else if (strcmp(package, "org.kde.plasma.mobile.shell") == 0) {
        kde_apply_mobile_preset(s);
    } else if (strcmp(package, "org.kde.plasma.bigscreen.shell") == 0) {
        s->session_type = KDE_SESSION_PLASMA_BIGD;
        s->menu_style = KDE_MENU_DASHBOARD;
    } else if (strcmp(package, "org.kde.oxygen.desktop") == 0) {
        kde_color_scheme_set(s, "Oxygen");
        strcpy(s->widget_style, "Oxygen");
        strcpy(s->icon_theme, "oxygen");
        s->decoration = KDE_DECOR_OXYGEN;
    }
    strcpy(s->window_decoration_theme, window_decor_name(s->decoration));
    if (s->verbose)
        fprintf(stderr, "kde: applied look-and-feel '%s'\n", package);
    return 0;
}

int kde_get_default_preset_commands(char out_commands[8][4096]) {
    int i = 0;
    if (i < 8) { snprintf(out_commands[i++], 4096, "kde --preset default"); }
    if (i < 8) { snprintf(out_commands[i++], 4096, "kde --preset minimal"); }
    if (i < 8) { snprintf(out_commands[i++], 4096, "kde --preset mobile"); }
    if (i < 8) { snprintf(out_commands[i++], 4096, "kde --look-and-feel org.kde.breeze.desktop"); }
    if (i < 8) { snprintf(out_commands[i++], 4096, "kde --look-and-feel org.kde.breezedark.desktop"); }
    if (i < 8) { snprintf(out_commands[i++], 4096, "kde --color-scheme BreezeDark"); }
    if (i < 8) { snprintf(out_commands[i++], 4096, "kde --desktops 4"); }
    if (i < 8) { snprintf(out_commands[i++], 4096, "kde --probe-hardware --scan-apps"); }
    return i;
}

#ifndef KENUXK_NO_MAIN_KDE
int main(int argc, char **argv) {
    static KdeState state;
    kde_init(&state);

    if (kde_parse_arguments(&state, argc, argv) != 0) {
        kde_cleanup(&state);
        return 1;
    }
    if (kde_start_session(&state) < 0) {
        fprintf(stderr, "kde: failed to start session\n");
        kde_cleanup(&state);
        return 1;
    }
    int code = kde_run_session_loop(&state);
    kde_end_session(&state, code, 0);
    kde_cleanup(&state);
    return code;
}
#endif
