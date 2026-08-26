/*
 * Kenux OS - KDE Plasma Desktop Environment (Minimal)
 * Header file
 */

#ifndef _KDE_H
#define _KDE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
/* MinGW 兼容: 提供 uid_t/gid_t 定义 (pid_t 已由 MinGW 提供) */
typedef int uid_t;
typedef int gid_t;
#endif

#define KDE_VERSION_STR "KenuxK-Plasma 6.0.4 (Minimal Desktop)"

#define KDE_MAX_APPS 256
#define KDE_MAX_DESKTOPS 20
#define KDE_MAX_PANELS 16
#define KDE_MAX_WIDGETS 128
#define KDE_MAX_ICONS 4096
#define KDE_MAX_MENU_ENTRIES 4096
#define KDE_MAX_NOTIFICATIONS 256
#define KDE_MAX_STR_LEN 4096
#define KDE_MAX_RULES 1024
#define KDE_MAX_HOTKEYS 1024
#define KDE_MAX_KWIN_RULES 512
#define KDE_MAX_ACTIVITIES 8
#define KDE_MAX_COLOR_SCHEMES 64
#define KDE_MAX_ICON_THEMES 64
#define KDE_MAX_PLASMOIDS 256
#define KDE_MAX_LOOKANDFEEL 64
#define KDE_MAX_KCM_MODULES 256
#define KDE_MAX_WALLPAPERS 256
#define KDE_MAX_SCREENS 8

/* Plasma / shell session type */
typedef enum {
    KDE_SESSION_PLASMA_X11 = 0,     /* plasma-desktop on X11 */
    KDE_SESSION_PLASMA_WAYLAND,     /* plasma-desktop on Wayland */
    KDE_SESSION_PLASMA_MOBILE,      /* plasma-mobile */
    KDE_SESSION_PLASMA_BIGD,        /* plasma-bigscreen */
    KDE_SESSION_PLASMA_ACTIVE,      /* legacy, fallback */
} KdeSessionType;

/* Application launcher menu type */
typedef enum {
    KDE_MENU_KICKOFF = 0,           /* default Kickoff launcher */
    KDE_MENU_KICKER,                /* classic K menu (compact) */
    KDE_MENU_DASHBOARD,             /* full-screen launcher */
    KDE_MENU_HOMERUN,
    KDE_MENU_TILED,                 /* gnome-like */
} KdeMenuType;

/* KWin window manager tiling mode */
typedef enum {
    KDE_TILING_OFF = 0,
    KDE_TILING_FLOATING_DEFAULT,    /* default floating; can tile */
    KDE_TILING_TILED_DEFAULT,       /* default tiled */
    KDE_TILING_BSP,                 /* Binary Space Partitioning */
    KDE_TILING_MASTER_STACK,
    KDE_TILING_TABULAR,
} KdeTilingMode;

/* KWin decoration theme type */
typedef enum {
    KDE_DECOR_BREEZE = 0,
    KDE_DECOR_BREEZE_DARK,
    KDE_DECOR_OXYGEN,
    KDE_DECOR_KLASSY,
    KDE_DECOR_LIGHTLY,
    KDE_DECOR_SWEET,
    KDE_DECOR_MATERIA,
} KdeWindowDecoration;

/* Panel / containment type */
typedef enum {
    KDE_PANEL_TOP = 0,
    KDE_PANEL_BOTTOM,
    KDE_PANEL_LEFT,
    KDE_PANEL_RIGHT,
    KDE_PANEL_FLOATING,         /* floating panel */
    KDE_PANEL_DOCK,             /* dock-like (latte-dock style) */
} KdePanelLocation;

/* Widget type */
typedef enum {
    KDE_WIDGET_DEFAULT = 0,
    KDE_WIDGET_APP_LAUNCHER,    /* Kickoff */
    KDE_WIDGET_PAGER,           /* pager / desktop indicator */
    KDE_WIDGET_TASKBAR,         /* icons-only task manager */
    KDE_WIDGET_CLASSIC_TASKBAR, /* classic task manager */
    KDE_WIDGET_SYSTEM_TRAY,
    KDE_WIDGET_DIGITAL_CLOCK,
    KDE_WIDGET_ANALOG_CLOCK,
    KDE_WIDGET_BATTERY,
    KDE_WIDGET_VOLUME,
    KDE_WIDGET_BRIGHTNESS,
    KDE_WIDGET_NETWORK,
    KDE_WIDGET_BLUETOOTH,
    KDE_WIDGET_CLIPBOARD,
    KDE_WIDGET_NOTIFICATIONS,
    KDE_WIDGET_NOTES,
    KDE_WIDGET_CALENDAR,
    KDE_WIDGET_CALCULATOR,
    KDE_WIDGET_WEATHER,
    KDE_WIDGET_FOLDERVIEW,
    KDE_WIDGET_KONSOLE_PROFILE,
    KDE_WIDGET_MEDIA_CONTROLLER,
    KDE_WIDGET_ACTIVITY_SWITCHER,
    KDE_WIDGET_SYSTEM_LOAD,
    KDE_WIDGET_HARDWARE_MONITOR,
    KDE_WIDGET_LOCK_SCREEN,
    KDE_WIDGET_LEAVE,
    KDE_WIDGET_USER_SWITCHER,
    KDE_WIDGET_SCREEN_CORNER_BUTTON,
    KDE_WIDGET_WINDOW_LIST,
    KDE_WIDGET_SEARCH,           /* Krunner-like */
    KDE_WIDGET_PLACES,
    KDE_WIDGET_TRASH,
    KDE_WIDGET_DESKTOP,
} KdeWidgetType;

/* SDDM / display-manager session entry */
typedef struct {
    char name[256];
    char exec_line[2048];
    char tryexec[2048];
    char desktop_names[256];
    char comment[512];
    char icon[256];
    char type[64];            /* "X11" or "Wayland" */
    int hidden;
} KdeSessionFile;

/* KWin Window rule */
typedef struct {
    char wm_class[2][256];
    char wm_role[128];
    char title[512];
    int title_match;  /* 0 substring, 1 exact, 2 regex */
    int match_whole_app_class;
    /* What to apply */
    int forced_desktop;
    int forced_activity;
    int forced_screen;
    int forced_state_fullscreen;
    int forced_state_maximized_vert;
    int forced_state_maximized_horiz;
    int forced_state_no_border;
    int forced_state_keep_above;
    int forced_state_keep_below;
    int forced_state_skip_taskbar;
    int forced_state_skip_switcher;
    int forced_state_demands_attention;
    int forced_closeable;
    int forced_minimizable;
    int forced_maximizable;
    int placed_x, placed_y;
    int size_w, size_h;
    int opacity;
    char decoration_theme[256];
    int block_compositing;
    int no_focus;
    int focus_stealing_prevention_level;
} KdeKwinRule;

/* Hotkey / KGlobalAccel shortcut */
typedef struct {
    char component[256];
    char action_id[256];
    char action_label[512];
    char key_seq[64];           /* "Ctrl+Alt+T" style */
    char alt_seq[64];
    int enabled;
    /* action when key triggers: exec or dbus */
    char exec[2048];
    char dbus_service[256];
    char dbus_path[512];
    char dbus_iface[256];
    char dbus_method[256];
    char dbus_args[1024];
} KdeHotkey;

/* App / .desktop file cache entry */
typedef struct {
    char name[256];
    char generic_name[256];
    char comment[512];
    char keywords[1024];
    char categories[1024];
    char exec[4096];
    char tryexec[4096];
    char icon[256];
    char path[2048];
    char terminal_cmd[2048];
    char mime_types[4096];
    char desktop_file[4096];
    int is_terminal;
    int nodisplay;
    int hidden;
    int startup_notify;
    char startup_wm_class[256];
    int preselect_score;       /* search / order */
} KdeAppEntry;

/* Notification */
typedef struct {
    uint32_t id;
    char app_name[256];
    char summary[512];
    char body[4096];
    char icon[512];
    char actions[1024];         /* "action1","label1","action2","label2" */
    char category[256];
    char desktop_entry[256];
    int expire_timeout_ms;
    int urgency;                /* 0 low, 1 normal, 2 critical */
    int transient;
    int resident;
    time_t created_at;
    int dismissed;
} KdeNotification;

/* Plasma containment (panel / desktop view) */
typedef struct {
    char id[256];
    char plugin[256];            /* "org.kde.panel" / "org.kde.desktopcontainment" */
    char name[256];
    KdePanelLocation location;
    int size_h;                  /* thickness in px for panel bar */
    int position_pct;            /* 0..100 alignment along edge */
    int span_pct;                /* 0..100 span length */
    int floating_margin;         /* for floating panels */
    int opacity;                 /* 0..100 */
    int auto_hide;               /* autohide behaviour */
    int visible_on_all_desktops;
    int visible_on_all_activities;
    int screen;                  /* 0..n, -1 = primary */
    /* Widget list */
    int widget_ids[KDE_MAX_WIDGETS];
    int widget_count;
} KdePanel;

/* Plasmoid / widget instance */
typedef struct {
    int id;
    char plugin[256];
    char title[256];
    KdeWidgetType type;
    int panel_id;                /* which panel, or -1 for desktop */
    int containment_desktop;
    char config_json[8192];
} KdePlasmoid;

/* Activity / Plasma Activity */
typedef struct {
    char id[64];                 /* UUID */
    char name[256];
    char description[512];
    char icon[128];
    char wallpaper[4096];
    int enabled;
    int keep_state;
} KdeActivity;

/* Color scheme / colors */
typedef struct {
    char name[256];
    char path[4096];
    char display[512];
    int is_dark;
    uint32_t background;
    uint32_t foreground;
    uint32_t selection_bg;
    uint32_t selection_fg;
    uint32_t button_bg;
    uint32_t button_fg;
    uint32_t view_bg;
    uint32_t view_fg;
    uint32_t tooltip_bg;
    uint32_t tooltip_fg;
    uint32_t accent;              /* highlight color */
    uint32_t positive;
    uint32_t negative;
    uint32_t neutral;
} KdeColorScheme;

/* Desktop wallpaper configuration */
typedef enum {
    KDE_WALLPAPER_SINGLE = 0,
    KDE_WALLPAPER_SLIDESHOW,
    KDE_WALLPAPER_COLOR,
    KDE_WALLPAPER_GRADIENT,
    KDE_WALLPAPER_PLASMA,     /* animated Plasma wallpaper */
    KDE_WALLPAPER_VIDEO,
} KdeWallpaperMode;

typedef struct {
    KdeWallpaperMode mode;
    char path[4096];
    char slideshow_dir[4096];
    int slideshow_random;
    int slideshow_change_minutes;
    uint32_t color_1;
    uint32_t color_2;
    int gradient_angle;
    int fill_mode;              /* 0=fill,1=fit,2=stretch,3=tile,4=center */
    int blur_radius;            /* for lockscreen */
    int dim_pct;                /* 0..100 */
    int per_output;             /* separate per monitor */
} KdeWallpaper;

/* Desktop layout */
typedef enum {
    KDE_DESKTOP_FOLDER = 0,
    KDE_DESKTOP_GRID,
    KDE_DESKTOP_NEWSPAPER,
    KDE_DESKTOP_SEARCH_AND_LAUNCH,
} KdeDesktopLayout;

/* KWin compositor effects list (stub) */
typedef enum {
    KDE_EFFECT_BLUR = 1 << 0,
    KDE_EFFECT_TRANSLUCENCY = 1 << 1,
    KDE_EFFECT_DIM_INACTIVE = 1 << 2,
    KDE_EFFECT_DARKEN_INACTIVE = 1 << 3,
    KDE_EFFECT_SLIDING_POPUPS = 1 << 4,
    KDE_EFFECT_WOBBLY = 1 << 5,
    KDE_EFFECT_CUBE = 1 << 6,
    KDE_EFFECT_COVER_SWITCH = 1 << 7,
    KDE_EFFECT_FLIP = 1 << 8,
    KDE_EFFECT_GLIDE = 1 << 9,
    KDE_EFFECT_SHEET = 1 << 10,
    KDE_EFFECT_MAGIC_LAMP = 1 << 11,
    KDE_EFFECT_FALL_APART = 1 << 12,
    KDE_EFFECT_LOGOUT = 1 << 13,
    KDE_EFFECT_INVERT = 1 << 14,
    KDE_EFFECT_COLORBLIND = 1 << 15,
    KDE_EFFECT_CONTRAST = 1 << 16,
    KDE_EFFECT_MOUSE_MARK = 1 << 17,
    KDE_EFFECT_SCREEN_EDGE = 1 << 18,
    KDE_EFFECT_TOUCH = 1 << 19,
    KDE_EFFECT_OVERVIEW = 1 << 20,
    KDE_EFFECT_PRESENT_WINDOWS = 1 << 21,
    KDE_EFFECT_DESKTOP_GRID = 1 << 22,
    KDE_EFFECT_DASHBOARD = 1 << 23,
    KDE_EFFECT_DND_FROM_TOP = 1 << 24,
} KdeEffectsMask;

/* Screen edge actions (hot corners) */
typedef enum {
    KDE_EDGE_NONE = 0,
    KDE_EDGE_OVERVIEW,
    KDE_EDGE_PRESENT_WINDOWS,
    KDE_EDGE_DESKTOP_GRID,
    KDE_EDGE_SHOW_DESKTOP,
    KDE_EDGE_SHOW_DASHBOARD,
    KDE_EDGE_SHOW_SPOTLIGHT,
    KDE_EDGE_SHOW_DESKTOP_GRID_PREV,
    KDE_EDGE_SHOW_DESKTOP_GRID_NEXT,
    KDE_EDGE_LOCK_SCREEN,
    KDE_EDGE_RUN_KCM,
    KDE_EDGE_EXECUTE_SCRIPT,
} KdeScreenEdgeAction;

/* KCM (system settings) module entry */
typedef struct {
    char id[256];
    char name[256];
    char description[512];
    char icon[256];
    char category[256];
    char exec[4096];
    int weight;
    int enabled;
} KdeKcmModule;

/* Main KDE session state */
typedef struct {
    KdeSessionType session_type;

    char xdg_runtime_dir[512];
    char xdg_data_dirs[8192];
    char xdg_config_dirs[8192];
    char kde_home[4096];
    char kde_config_dir[4096];
    char kde_data_dir[4096];
    char kdebugrc[4096];

    /* Display / Wayland session */
    char display[64];
    char wayland_display[256];
    int display_number;
    int headless;

    /* Session / login */
    uid_t uid;
    gid_t gid;
    pid_t pid;
    char username[128];
    char hostname[256];
    char shell[512];
    char home[4096];

    /* Locale / language */
    char language[64];
    char region[64];
    char formats_locale[64];
    char time_locale[64];
    int use_24h_clock;
    int long_date_style;
    int short_date_style;
    char monetary_locale[64];
    char paper_locale[64];
    char measurement_locale[64];
    char collation_locale[64];
    char charset[64];

    /* Global Theme / Look and Feel */
    char lookandfeel_package[256];       /* global theme, e.g. "org.kde.breeze.desktop" */
    char global_theme_display[256];
    int  prefer_dark;
    /* Widget style */
    char widget_style[256];              /* Breeze / Oxygen / Kvantum ... */
    char kvantum_theme[256];
    /* Color scheme */
    char color_scheme[256];              /* "BreezeDark" */
    KdeColorScheme *loaded_color_scheme;
    int color_scheme_count;
    KdeColorScheme available_color_schemes[KDE_MAX_COLOR_SCHEMES];
    /* Icons / cursors / fonts / sounds */
    char icon_theme[256];
    char fallback_icon_theme[256];
    char cursor_theme[256];
    int  cursor_size;
    char general_font[128];
    char fixed_font[128];
    char titlebar_font[128];
    char toolbar_font[128];
    char menu_font[128];
    char window_title_font[128];
    char taskbar_font[128];
    char desktop_font[128];
    int  use_antialiasing;
    int  hinting;                /* 0 none, 1 slight, 2 medium, 3 full */
    char subpixel[8];            /* none/rgb/bgr/vrgb/vbgr */
    int  force_font_dpi;
    int  font_dpi;
    char sound_theme[256];
    int  enable_sounds;
    int  enable_notification_sounds;
    int  enable_feedback_sounds;
    /* Window decorations */
    KdeWindowDecoration decoration;
    char window_decoration_theme[256];
    int  titlebar_buttons_right[8];   /* :N - :M - C - I - etc. */
    int  titlebar_buttons_left[8];
    int  titlebar_height_px;
    int  window_border_size_px;
    int  draw_titlebar_shadow;
    int  blur_behind_decorations;

    /* Compositor */
    int compositor_enabled;
    int compositor_vsync;
    int compositor_scale_method;   /* 0 auto, 1 smooth, 2 crisp, 3 normal */
    int compositor_animation_speed_pct; /* 25..200 */
    int compositor_window_opacity;      /* 10..100 */
    KdeEffectsMask effects_mask;
    int animation_toggled[32];

    /* KWin behaviour */
    KdeTilingMode tiling_mode;
    int focus_policy;               /* 0=ClickToFocus, 1=FocusFollowsMouse, 2=FocusUnderMouse */
    int focus_stealing_prevention_level; /* 0..4 */
    int auto_raise;
    int click_raise;
    int click_to_focus;
    int window_placement;           /* 0=smart, 1=centered, 2=random, 3=corner */
    int windows_fade_in;
    int windows_fade_out;
    int window_opacity_active;     /* 0..100 */
    int window_opacity_inactive;
    int snap_zone_enabled;
    int snap_zone_ratio;
    int reverse_layout;              /* right-to-left */

    /* Workspaces / virtual desktops */
    int desktop_count;
    int desktop_rows;
    int desktop_cols;
    int current_desktop;
    struct {
        char name[256];
        char wallpaper_path[4096];
    } desktops[KDE_MAX_DESKTOPS];

    /* Activities */
    int activity_count;
    int current_activity;
    KdeActivity activities[KDE_MAX_ACTIVITIES];

    /* Outputs/screens */
    int screen_count;
    int primary_screen;
    struct {
        int width, height;
        int refresh_mhz;
        int scale;                 /* 1, 125, 150, 200 (%) */
        int rotation;              /* 0,90,180,270 */
        int x, y;
        char name[256];
        char connector[64];
        KdeWallpaper wallpaper;
    } screens[KDE_MAX_SCREENS];

    /* Panels / Desktop containments */
    int panel_count;
    KdePanel panels[KDE_MAX_PANELS];
    KdeDesktopLayout desktop_layout;

    /* Widgets / Plasmoids */
    int plasmoid_count;
    KdePlasmoid plasmoids[KDE_MAX_PLASMOIDS];

    /* Menu */
    KdeMenuType menu_style;
    int menu_show_all_applications;
    int menu_show_recent;
    int menu_search;
    int menu_favorites_count;
    char menu_favorites[64][256];
    /* App menu / hamburger in title bar? */
    int app_menu_in_titlebar;
    int show_window_decoration;

    /* Wallpaper */
    KdeWallpaper global_wallpaper;
    int wallpaper_per_activity;
    int wallpaper_per_output;

    /* Task manager */
    int taskbar_grouping;            /* 0=never,1=when_full,2=always */
    int taskbar_icon_only;           /* 0=text/icons, 1=icons-only */
    int taskbar_show_tooltips;
    int taskbar_middle_click;        /* 0=new instance,1=close,2=none,3=minimize,4=launch */

    /* System tray */
    int tray_show_all;
    int tray_hidden_items[128][128];
    int tray_hidden_items_count;
    int tray_icons_size;

    /* Clock */
    int clock_show_seconds;
    int clock_use_24h;
    char clock_custom_format[256];
    int calendar_show_week_numbers;
    int calendar_first_day_of_week;

    /* Notifications */
    int notify_badge_count;
    int notify_history;
    int notify_do_not_disturb;
    int notify_inhibit_apps;
    int notify_show_on_lockscreen;
    int notify_critical_fullscreen;
    int notify_popup_timeout_ms;
    int notify_show_in_dnd[KDE_MAX_NOTIFICATIONS];
    KdeNotification notifications[KDE_MAX_NOTIFICATIONS];
    int notification_count;

    /* Apps / KIO */
    int app_count;
    KdeAppEntry apps[KDE_MAX_APPS];
    char default_browser[2048];
    char default_email[2048];
    char default_file_manager[2048];
    char default_terminal[2048];
    char default_music[2048];
    char default_video[2048];
    char default_image[2048];

    /* File associations cache would be here... */

    /* KWin rules */
    int kwin_rule_count;
    KdeKwinRule kwin_rules[KDE_MAX_KWIN_RULES];

    /* Hotkeys / shortcuts */
    int hotkey_count;
    KdeHotkey hotkeys[KDE_MAX_HOTKEYS];

    /* Screen edges (corners) */
    KdeScreenEdgeAction edge_top_left;
    KdeScreenEdgeAction edge_top;
    KdeScreenEdgeAction edge_top_right;
    KdeScreenEdgeAction edge_left;
    KdeScreenEdgeAction edge_right;
    KdeScreenEdgeAction edge_bottom_left;
    KdeScreenEdgeAction edge_bottom;
    KdeScreenEdgeAction edge_bottom_right;
    int edge_delay_ms;
    int edge_reactivate_ms;

    /* Sessions */
    int login_shutdown_type;     /* 0 restore, 1 empty, 2 saved */
    char autostart_dirs[8192];
    int start_kwin_script[16][512];
    int start_kcm_after_login[KDE_MAX_KCM_MODULES];

    /* Power management */
    int power_dim_on_battery_min;
    int power_sleep_on_battery_min;
    int power_dim_on_ac_min;
    int power_sleep_on_ac_min;
    int power_button_action;      /* 0=shutdown,1=suspend,2=hibernate,3=lock,4=ask */
    int lid_switch_action_battery;
    int lid_switch_action_ac;
    int critical_battery_action;  /* 0=hibernate, 1=shutdown */
    int suspend_then_hibernate;
    int brightness_on_battery_pct;
    int brightness_on_ac_pct;
    int battery_icon_pct;

    /* Network */
    int nm_enabled;
    int bluetooth_enabled;
    int airplane_mode;

    /* Input */
    struct {
        char model[128];
        char layout[128];
        char variant[128];
        char options[128];
        char switch_shortcut[64];
        int repeat_delay_ms;
        int repeat_rate_cps;
        int numlock_on_boot;
        int caps_behavior;           /* 0 normal, 1 swap with Ctrl, 2 disabled, 3 Esc */
    } keyboard;
    struct {
        char name[128];
        int accel_profile;           /* 0 flat, 1 adaptive, 2 classic */
        double accel;                /* -1..1 */
        int left_handed;
        int tap_to_click;
        int tap_and_drag;
        int tap_and_drag_lock;
        int natural_scroll;
        int two_finger_scroll;
        int middle_emulation;
        int scroll_method;           /* 0 none, 1 two-finger, 2 edge, 3 button */
        int scroll_direction;        /* 0 standard, 1 inverted */
    } touchpad;
    struct {
        int enabled;
        double accel;
        int button_map[16];
    } pointer;
    struct {
        int enabled;
        int rotation_ccw;
        int invert_x;
        int invert_y;
    } touchscreen;

    /* Accessibility */
    int assistive_tech_enabled;
    int kmag;
    int kmousetool;
    int ktts;
    int orca;
    int screen_reader_enabled;
    int invert_colors;
    int screen_magnifier;
    int magnifier_factor_pct;
    int magnifier_lens;
    int color_blind_mode;         /* 0 off, 1 protanopia, 2 deuteranopia, 3 tritanopia */
    int single_click;             /* single-click to open */
    int sticky_keys;
    int slow_keys;
    int bounce_keys;
    int mouse_keys;
    int simulated_secondary_click;
    int dwell_click;

    /* Security / SDDM / Lock screen */
    char greeter_theme[256];
    int  greeter_numlock;
    int  auto_login;
    char auto_login_user[128];
    int  auto_relogin;
    int  passwordless_login;
    int  lock_on_idle_s;
    int  lock_on_resume;
    int  lock_on_lid;
    int  show_users_in_greeter;
    int  show_manual_login;
    char lock_screen_wallpaper[4096];
    int  lock_screen_blank_pct;

    /* Dolphin / file manager */
    char dolphin_default_view;    /* "icons", "compact", "details", "column" */
    int dolphin_show_hidden;
    int dolphin_show_previews;
    int dolphin_show_folders_first;
    int dolphin_sort_case_sensitive;
    int dolphin_breadcrumb;
    int dolphin_split_view_default;
    int dolphin_terminal_panel_default;
    char dolphin_start_path[4096];

    /* Session state */
    int running;
    int exit_code;
    int reload_config;
    time_t started_at;
    int verbose;
    int debug;
    int startup_finished;

    /* Plasmashell / KWin / Krunner pids */
    pid_t pid_kwin;
    pid_t pid_plasmashell;
    pid_t pid_krunner;
    pid_t pid_polkit;
    pid_t pid_xdg_porthole;
    pid_t pid_kded;
    pid_t pid_ksmserver;

} KdeState;

/* ------------------------- API ------------------------- */

void kde_init(KdeState *s);
void kde_cleanup(KdeState *s);
int kde_parse_arguments(KdeState *s, int argc, char **argv);
int kde_probe_hardware(KdeState *s);
int kde_load_configuration(KdeState *s);
int kde_save_configuration(KdeState *s);
int kde_apply_default_preset(KdeState *s);
int kde_apply_minimal_preset(KdeState *s);
int kde_apply_mobile_preset(KdeState *s);

/* session */
int kde_start_session(KdeState *s);
int kde_run_session_loop(KdeState *s);
void kde_end_session(KdeState *s, int exit_code, int kill_daemons);

/* panels / desktops */
int kde_panel_add(KdeState *s, KdePanelLocation loc, int screen, int *out_idx);
int kde_widget_add(KdeState *s, int panel_id, KdeWidgetType type);
int kde_workspace_switch(KdeState *s, int idx);

/* apps & menu */
int kde_apps_scan(KdeState *s);
int kde_apps_search(KdeState *s, const char *query, int *indices, int max);
int kde_app_launch(const KdeAppEntry *app, char **argv_extra);
int kde_krunner_run(KdeState *s, const char *query);

/* notifications */
uint32_t kde_notify(KdeState *s, const KdeNotification *in);
int kde_notify_close(KdeState *s, uint32_t id);

/* kwin rules / hotkeys */
int kde_hotkey_register(KdeState *s, const KdeHotkey *hk);
int kde_hotkey_trigger(KdeState *s, const char *key_seq);
int kde_kwin_rule_add(KdeState *s, const KdeKwinRule *r);

/* kcms */
int kde_kcm_launch(KdeState *s, const char *kcm_id);
int kde_kcm_scan(KdeState *s);

/* display */
int kde_screen_configure(KdeState *s, int screen_idx, int w, int h, int scale, int rotate);
int kde_color_scheme_set(KdeState *s, const char *scheme_name);

/* helpers */
void kde_print_help(void);
void kde_print_version(void);
int kde_apply_lookandfeel(KdeState *s, const char *package);
int kde_get_default_preset_commands(char out_commands[8][4096]);

#endif
