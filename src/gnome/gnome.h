/*
 * Kenux OS - GNOME Desktop Environment (Minimal)
 * Header file
 */

#ifndef _GNOME_H
#define _GNOME_H

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

#define GNOME_VERSION_STR "KenuxK-GNOME 46.1 (Minimal Desktop)"

#define GNOME_MAX_APPS 1024
#define GNOME_MAX_WORKSPACES 36
#define GNOME_MAX_WINDOWS 8192
#define GNOME_MAX_MONITORS 8
#define GNOME_MAX_EXTENSIONS 256
#define GNOME_MAX_KEYBINDINGS 512
#define GNOME_MAX_NOTIFICATIONS 256
#define GNOME_MAX_MIME 4096
#define GNOME_MAX_SEARCH 64
#define GNOME_MAX_COLOR_SCHEMES 64
#define GNOME_MAX_THEMES 128
#define GNOME_MAX_FONTS 128
#define GNOME_MAX_GSETTINGS 2048
#define GNOME_MAX_PATH_LEN 4096

typedef enum {
    GNOME_SESSION_WAYLAND = 0,   /* gnome-shell on Wayland (default) */
    GNOME_SESSION_X11,           /* gnome-shell on Xorg */
    GNOME_SESSION_CLASSIC,       /* GNOME Classic mode */
    GNOME_SESSION_MOBILE,        /* Phosh/mobile */
} GnomeSessionType;

/* GSD (settings-daemon) plugin flags */
typedef enum {
    GNOME_GSD_A11Y_KEYBOARD     = 1 << 0,
    GNOME_GSD_A11Y_MAGNIFIER    = 1 << 1,
    GNOME_GSD_AUTORUN          = 1 << 2,
    GNOME_GSD_AUTOSTART        = 1 << 3,
    GNOME_GSD_BACKGROUND       = 1 << 4,
    GNOME_GSD_CLIPBOARD        = 1 << 5,
    GNOME_GSD_COLOR            = 1 << 6,
    GNOME_GSD_DATETIME         = 1 << 7,
    GNOME_GSD_DATETIME_FORMAT  = 1 << 8,
    GNOME_GSD_HOUSEKEEPING     = 1 << 9,
    GNOME_GSD_KEYBOARD         = 1 << 10,
    GNOME_GSD_MEDIA_KEYS       = 1 << 11,
    GNOME_GSD_MOUSE            = 1 << 12,
    GNOME_GSD_ORIENTATION      = 1 << 13,
    GNOME_GSD_POWER            = 1 << 14,
    GNOME_GSD_PRINT_NOTIFICATIONS = 1 << 15,
    GNOME_GSD_RFKILL           = 1 << 16,
    GNOME_GSD_SCREENSAVER_PROXY= 1 << 17,
    GNOME_GSD_SHARE            = 1 << 18,
    GNOME_GSD_SMARTCARD         = 1 << 19,
    GNOME_GSD_SOUND            = 1 << 20,
    GNOME_GSD_WACOM            = 1 << 21,
    GNOME_GSD_WWAN             = 1 << 22,
    GNOME_GSD_XSETTINGS        = 1 << 23,
    GNOME_GSD_ACCOUNT          = 1 << 24,
    GNOME_GSD_WACOM2            = 1 << 25,
} GnomeGsdPlugins;

/* Mutter window manager / compositor state */
typedef enum {
    GNOME_LAYOUT_MUTTER_DEFAULT = 0,
    GNOME_LAYOUT_TILING_MANUAL,
    GNOME_LAYOUT_POP_SHELL_TILING,
    GNOME_LAYOUT_FORGE,
    GNOME_LAYOUT_MATERIAL,
} GnomeTilingLayout;

typedef enum {
    GNOME_TITLEBAR_LEFT = 0,
    GNOME_TITLEBAR_RIGHT,
} GnomeTitlebarButtonSide;

/* Application / desktop entry */
typedef struct {
    char id[512];              /* e.g. "org.gnome.Nautilus" */
    char filename[GNOME_MAX_PATH_LEN];
    char name[512];
    char generic_name[512];
    char comment[1024];
    char keywords[2048];
    char categories[2048];
    char only_show_in[512];
    char not_show_in[512];
    char exec[GNOME_MAX_PATH_LEN];
    char tryexec[GNOME_MAX_PATH_LEN];
    char icon[512];
    char path[GNOME_MAX_PATH_LEN];
    char mime_type[4096];
    char startup_wm_class[256];
    char startup_notify_wm_class[256];
    int terminal;
    int startup_notify;
    int nodisplay;
    int hidden;
    int dbus_activatable;
    int preselect_score;
} GnomeAppEntry;

/* Shell Search Provider */
typedef enum {
    GNOME_SEARCH_APPS = 0,
    GNOME_SEARCH_FILES,
    GNOME_SEARCH_CALCULATOR,
    GNOME_SEARCH_CLOCK,
    GNOME_SEARCH_TERMINAL,
    GNOME_SEARCH_CONTACTS,
    GNOME_SEARCH_SETTINGS,
    GNOME_SEARCH_SOFTWARE,
    GNOME_SEARCH_CLIPBOARD,
    GNOME_SEARCH_WEB,
} GnomeSearchProviderKind;

typedef struct {
    char id[256];
    char name[256];
    char bus_name[512];
    char obj_path[1024];
    char iface[256];
    int enabled;
    int has_results;
    int order;
    int remote;
} GnomeSearchProvider;

/* GNOME Shell Extension (meta) */
typedef struct {
    char uuid[256];             /* e.g. "blur-my-shell@aunetx" */
    char name[256];
    char description[1024];
    char version_str[64];
    char url[1024];
    char path[GNOME_MAX_PATH_LEN];
    char shell_version_min[64];
    char shell_version_max[64];
    char author[256];
    char metadata[8192];     /* JSON blob (minimal) */
    int enabled;
    int has_prefs;
    int can_disable;
    int has_update;
    int download_size;
} GnomeExtension;

/* Notification / OSD */
typedef enum {
    GNOME_URGENCY_LOW = 0,
    GNOME_URGENCY_NORMAL,
    GNOME_URGENCY_CRITICAL,
} GnomeUrgency;

typedef struct {
    uint32_t id;
    char sender_bus[512];
    char app_name[256];
    char app_icon[512];
    char summary[1024];
    char body[8192];
    char actions[2048];   /* null-separated pairs: "action1","label1","action2","label2" */
    char category[256];
    char desktop_entry[256];
    char sound_file[GNOME_MAX_PATH_LEN];
    char sound_name[256];
    GnomeUrgency urgency;
    int expire_timeout_ms;
    int transient;
    int resident;
    int suppress_sound;
    int suppress_popup;
    int hint_action_icons;
    time_t created_at;
    int read;
    int dismissed;
} GnomeNotification;

/* Keybinding / Settings */
typedef struct {
    char schema[256];           /* e.g. org.gnome.desktop.wm.keybindings */
    char key[256];              /* e.g. close */
    char value[1024];           /* e.g. "['<Alt>F4']" -- serialized GVariant array */
    int relocatable;
    char path[512];
    int writable;
} GnomeKeybinding;

/* GSettings entry (minimal in-memory store) */
typedef struct {
    char schema[256];
    char key[256];
    char value[4096];
    char type_signature[32];
    int path_len;
    char path[512];
} GnomeGSettingsItem;

/* Mutter workspace */
typedef struct {
    int index;
    char name[128];
    uint32_t *window_ids;
    int win_count;
    int win_cap;
    int n_rows;
    int n_cols;
} GnomeWorkspace;

/* Workspace window */
typedef struct GnomeWindow {
    uint32_t id;
    uint64_t xid;                 /* X11 window or Wayland surface id */
    uint32_t workspace;
    char title[2048];
    char wm_class[2][256];
    char app_id[256];
    char startup_id[512];
    int x, y;
    int w, h;
    int min_w, min_h;
    int max_w, max_h;
    int border_width;
    int focused;
    int maximized_vert;
    int maximized_horz;
    int tiled_left;
    int tiled_right;
    int tiled_top;
    int tiled_bottom;
    int fullscreen;
    int fullscreen_on_monitor;
    int minimized;
    int stickied;
    int above;
    int skip_taskbar;
    int skip_pager;
    int demands_attention;
    int decorated;
    int user_time_ms;
    int layer;              /* 0 below, 1 normal, 2 dock, 3 above, 4 notification */
    float opacity;
    /* Mutter window types */
    int wm_window_type;       /* 0 normal, 1 desktop, 2 dock, 3 toolbar, 4 menu, 5 dialog, 6 util, 7 splash, 8 dropdown */
    /* tile preview */
    int tile_preview_x, tile_preview_y, tile_preview_w, tile_preview_h;
    /* snap / quarter tiling via keybindings */
    int snap_orientation;  /* 0 none, 1 left,2 right,3 top,4 bottom, 5 tl,6 tr,7 bl,8 br,9 max */
    struct GnomeWindow *next;
} GnomeWindow;

/* Monitor / Logical monitor */
typedef struct {
    char vendor[16];
    char product[32];
    char serial[32];
    char connector[64];
    char display_name[128];
    int width;
    int height;
    int refresh_mhz;
    int scale_1000;       /* 1000 = 1x, 1250 = 1.25x, 1500 = 1.5x, 2000 = 2x */
    int global_scale_1000;
    int x, y;
    int transform;        /* 0 normal, 1 90, 2 180, 3 270, 4 flipped, 5 90+flipped, etc. */
    int primary;
    int builtin;
    int underscanning;
    int color_profile_id;
} GnomeMonitor;

/* Night light */
typedef enum {
    GNOME_NIGHTLIGHT_MODE_OFF = 0,
    GNOME_NIGHTLIGHT_MODE_ALWAYS,
    GNOME_NIGHTLIGHT_MODE_SCHEDULED_SUNSET,
    GNOME_NIGHTLIGHT_MODE_SCHEDULED_CUSTOM,
} GnomeNightlightMode;

typedef struct {
    GnomeNightlightMode mode;
    int temperature_kelvin;     /* 1000 (warm) .. 6500 (daylight) */
    int custom_from_hour;
    int custom_from_minute;
    int custom_to_hour;
    int custom_to_minute;
    double current_temp;        /* interpolated */
} GnomeNightlight;

/* Color scheme */
typedef enum {
    GNOME_COLOR_STYLE_DEFAULT = 0,  /* prefer-dark */
    GNOME_COLOR_STYLE_LIGHT,
    GNOME_COLOR_STYLE_DARK,
} GnomeColorStyle;

typedef struct {
    char name[128];
    char palette_name[128];
    GnomeColorStyle style;
    uint32_t accent_color;
    uint32_t window_bg;
    uint32_t window_fg;
    uint32_t headerbar_bg;
    uint32_t headerbar_fg;
    uint32_t card_bg;
    uint32_t card_fg;
    uint32_t popover_bg;
    uint32_t popover_fg;
    uint32_t dialog_bg;
    uint32_t dialog_fg;
    uint32_t sidebar_bg;
    uint32_t sidebar_fg;
    uint32_t destructive_bg;
    uint32_t accent_bg;
    uint32_t success_bg;
    uint32_t warning_bg;
    uint32_t error_bg;
    int is_legacy_gtk3;
} GnomeColorScheme;

/* Power */
typedef enum {
    GNOME_POWER_ACTION_BLANK = 0,
    GNOME_POWER_ACTION_SUSPEND,
    GNOME_POWER_ACTION_SHUTDOWN,
    GNOME_POWER_ACTION_HIBERNATE,
    GNOME_POWER_ACTION_INTERACTIVE,
    GNOME_POWER_ACTION_NOTHING,
} GnomePowerAction;

/* Clock format */
typedef enum {
    GNOME_CLOCK_24H = 0,
    GNOME_CLOCK_12H,
} GnomeClockFormat;

/* Main GNOME state */
typedef struct {
    GnomeSessionType session_type;
    char session_id[256];
    char session_class[256];
    char session_desktop[256];

    char xdg_runtime_dir[1024];
    char xdg_data_dirs[8192];
    char xdg_config_dirs[8192];
    char xdg_current_desktop[256];
    char xdg_seat[32];
    char xdg_vtnr[16];
    char home[GNOME_MAX_PATH_LEN];
    char user_config_dir[GNOME_MAX_PATH_LEN];   /* ~/.config */
    char user_data_dir[GNOME_MAX_PATH_LEN];     /* ~/.local/share */
    char user_cache_dir[GNOME_MAX_PATH_LEN];    /* ~/.cache */
    char user_state_dir[GNOME_MAX_PATH_LEN];    /* ~/.local/state */

    uid_t uid;
    gid_t gid;
    pid_t pid;
    char username[128];
    char real_name[256];
    char hostname[256];
    char shell[GNOME_MAX_PATH_LEN];
    char lang[64];

    /* Display backend */
    char display[64];
    char wayland_display[256];
    int display_number;
    int headless;
    int nested;                  /* nested mode, e.g. on X11 desktop */
    int unsafe_mode;             /* allow alt+F2+r restart */

    /* Mutter */
    char mutter_dpi[64];
    int mutter_experimental_features[16];
    char mutter_extra_args[4096];
    GnomeTilingLayout layout;
    int compositor_enabled;
    int force_fullscreen_redraw;
    int experimental_blur;
    int experimental_rounded;
    int kms_modifiers;
    uint64_t gsd_plugin_mask;
    char theme_mode_gtk3[128];  /* "Adwaita" / "Adwaita-dark" / "HighContrast" */
    char theme_mode_gtk4[128];

    /* Global Theming / appearance (GNOME 42+) */
    GnomeColorStyle color_style;        /* light/dark/prefer-dark */
    GnomeColorScheme loaded_scheme;
    GnomeColorScheme available_schemes[GNOME_MAX_COLOR_SCHEMES];
    int color_scheme_count;
    char accent_style[128];
    uint32_t accent_hex;
    /* Icon, cursor, sound */
    char icon_theme[256];
    char cursor_theme[256];
    int  cursor_size;
    char sound_theme[256];
    int  enable_sound;
    int  enable_event_sounds;
    int  enable_input_feedback_sounds;
    /* Fonts */
    char font_name[256];           /* "Cantarell 11" */
    char doc_font_name[256];       /* "Sans 11" */
    char titlebar_font[256];       /* "Cantarell Bold 11" */
    char monospace_font_name[256]; /* "Source Code Pro 10" */
    char legacy_window_title_font[256];
    int  hinting;
    int  antialiasing;
    int  rgba_order;              /* 0 none, 1 rgb, 2 bgr, 3 vrgb, 4 vbgr */
    int  dpi;
    int  text_scaling_factor_pct; /* 100 = 1.0 */

    /* Window manager */
    int focus_mode;                /* 0 click, 1 sloppy, 2 mouse */
    int focus_new_windows;         /* 0 smart, 1 strict, 2 none */
    int titlebar_double_click;     /* 0 toggle-max, 1 toggle-shade, 2 maximize-vert, 3 minimize, 4 none */
    int titlebar_middle_click;     /* 0 lower, 1 toggle-max, 2 none */
    int titlebar_right_click;      /* 0 menu, 1 none */
    int action_double_click_titlebar;
    int action_middle_click_titlebar;
    int action_right_click_titlebar;
    int window_scaling_method;
    int edge_tiling;
    int attach_modal_dialogs;
    int auto_raise;
    int auto_raise_delay_ms;
    int raise_on_click;
    GnomeTitlebarButtonSide button_layout_side;
    char button_layout[64];        /* ":minimize,maximize,close" or "close,minimize,maximize:" */

    /* Workspaces */
    int dynamic_workspaces;     /* true = auto add/remove */
    int num_workspaces;         /* if static */
    int current_workspace;
    int workspace_only_on_primary;
    int workspace_wraps_around;
    GnomeWorkspace workspaces[GNOME_MAX_WORKSPACES];
    char workspace_names[GNOME_MAX_WORKSPACES][128];

    /* Windows list */
    GnomeWindow windows[GNOME_MAX_WINDOWS];
    int window_count;
    uint32_t window_id_counter;
    uint32_t focus_window_id;
    uint32_t pointer_window_id;

    /* Monitors */
    GnomeMonitor monitors[GNOME_MAX_MONITORS];
    int monitor_count;
    int primary_monitor;
    char clone_mode;
    char remember_application_placements;
    char center_new_windows;

    /* Shell elements: Top bar */
    int top_bar_show_activities_button;
    int top_bar_show_app_menu;
    int top_bar_show_date;
    int top_bar_show_seconds;
    int top_bar_show_weekday;
    int top_bar_show_week_number;
    GnomeClockFormat clock_format;
    char clock_custom_format[256];
    int top_bar_hot_corner;
    int hot_corner_delay_ms;
    int top_bar_opacity_pct;
    int top_bar_blur;

    /* Dash to Dock / Dash (builtin in newer shell) */
    int dash_show;                 /* false hides, default true */
    int dash_position;             /* 0 left, 1 right, 2 bottom */
    int dash_icon_size_px;
    int dash_max_icon_size;
    int dash_show_apps_at_bottom;
    int dash_show_favorites_only;
    int dash_show_running;
    int dash_dock_fixed;           /* always visible */
    int dash_dock_extend_height;
    int dash_dock_shrink;
    int dash_dock_transparent_mode; /* 0 default, 1 fixed, 2 dynamic, 3 none */
    float dash_dock_opacity;
    int dash_autohide;
    int dash_dock_intellihide;
    int dash_click_action;          /* 0 min-or-prev, 1 cycle-windows, 2 focus-min-or-quit, 3 launch new */
    int dash_scroll_action;         /* 0 cycle windows, 1 switch workspace, 2 quit */

    /* App Folders in AppGrid */
    int folder_count;
    struct {
        char name[64];
        char translate[64];
        char apps[64][256];
        int app_count;
    } folders[32];
    int app_grid_columns;
    int app_grid_page_size;
    int app_grid_sort_mode;  /* 0 frequency, 1 name, 2 recent */

    /* Search */
    GnomeSearchProvider search_providers[GNOME_MAX_SEARCH];
    int search_provider_count;
    int search_provider_order[GNOME_MAX_SEARCH];
    int search_enabled;
    int search_show_first_result_hint;
    char search_type_ahead_prefix[16];

    /* Overview */
    int overview_show_workspaces_only_on_primary;
    int overview_workspace_switcher_only_on_primary;
    int overview_hot_corner;
    int gesture_activation_threshold;
    int overview_gap_size_px;

    /* Wallpaper */
    char picture_uri[GNOME_MAX_PATH_LEN];     /* "file://..." */
    char picture_uri_dark[GNOME_MAX_PATH_LEN];
    int picture_options;        /* 0 none,1 wallpaper,2 centered,3 stretched,4 zoom,5 scaled,5 spanned,6 tiled */
    int picture_opacity_pct;
    uint32_t primary_color;
    uint32_t secondary_color;
    int color_shading_type;     /* 0 horizontal,1 vertical,2 solid */
    int show_desktop_icons;
    int desktop_icons_visible;
    int desktop_icon_size;      /* small, standard, large */
    char desktop_icon_layout[64];  /* grid or free */

    /* Screen lock / GDM */
    int enable_lock_screen;
    int disable_lock_screen;
    int lock_disable_user_list;
    int disable_user_switch;
    int disable_user_list;
    char banner_message_text[1024];
    char banner_message_text_enabled;
    int allowed_fails_delay;
    int screen_blank_delay_s_battery;
    int screen_blank_delay_s_ac;
    int idle_delay_battery_s;
    int idle_delay_ac_s;
    int idle_hint;
    GnomePowerAction sleep_inactive_battery_type;
    GnomePowerAction sleep_inactive_ac_type;
    GnomePowerAction lid_close_battery_action;
    GnomePowerAction lid_close_ac_action;
    GnomePowerAction power_button_action;
    int suspend_then_hibernate;
    int show_battery_percentage;
    int low_battery_action;
    int percentage_critical;
    int percentage_action;
    int time_format;
    GnomeNightlight nightlight;

    /* Notifications */
    GnomeNotification notifications[GNOME_MAX_NOTIFICATIONS];
    int notification_count;
    int show_banners;
    int show_in_lock_screen;
    int do_not_disturb;
    int show_in_always_on_top;
    int banner_must_acknowledge;
    int notification_history_persist;
    int dnd_on_battery;
    int dnd_on_fullscreen;

    /* Keyboard & Input */
    struct {
        char xkb_model[64];
        char xkb_layouts[256];
        char xkb_variants[256];
        char xkb_options[512];
        int numlock_state;       /* -1 remember, 0 off, 1 on */
        int delay_ms;
        int repeat_interval_ms;
        int input_sources_priority;
        int show_all_sources;
        int same_source_per_window;
        char switch_source_next[64];
        char switch_source_prev[64];
        char compose_key[64];
        char terminate_server[64];
    } keyboard;
    struct {
        int left_handed;
        int accel_profile;
        double accel_speed;
        double natural_scroll;
        double scroll_factor;
        int speed_numeric_pad_mouse_keys;
    } mouse;
    struct {
        int touchpad_enabled;
        int tap_to_click;
        int two_finger_scroll;
        int edge_scroll;
        int natural_scroll;
        int disable_while_typing;
        int tap_and_drag;
        int tap_and_drag_lock;
        int accel_profile;
        double accel_speed;
        int left_handed;
        int click_method;       /* 0 fingers, 1 button areas, 2 gestures */
        int middle_click_emulation;
    } touchpad;

    /* Accessibility */
    int toolkit_accessibility;   /* enable ATK/AT-SPI */
    int a11y_sticky_keys;
    int a11y_slow_keys;
    int a11y_slow_keys_delay_ms;
    int a11y_bounce_keys;
    int a11y_bounce_keys_delay_ms;
    int a11y_mouse_keys;
    int a11y_toggle_keys;
    int a11y_simul_brk;
    int a11y_screen_magnifier;
    int a11y_screen_reader;
    int a11y_screen_keyboard;
    int a11y_zoom_factor_pct;
    int a11y_high_contrast;
    int a11y_large_text;
    int a11y_visual_alerts;
    int a11y_visual_alerts_type;
    int a11y_increase_text;
    int a11y_magnifier_caret_tracking;
    int a11y_magnifier_mouse_tracking;
    int a11y_magnifier_focus_tracking;
    int a11y_magnifier_lens_mode;
    int a11y_sound_keys;
    int a11y_sound_theme;
    int a11y_dwell;
    int a11y_dwell_time_ms;
    int a11y_dwell_threshold;
    int a11y_secondary_click_enabled;
    int a11y_secondary_click_time_ms;
    int a11y_locate_pointer;

    /* Keybindings */
    GnomeKeybinding keybindings[GNOME_MAX_KEYBINDINGS];
    int keybinding_count;
    char custom_keybindings[256][256];    /* list of dirs in /custom-keybindings */
    int custom_keybinding_count;

    /* Extensions */
    GnomeExtension extensions[GNOME_MAX_EXTENSIONS];
    int extension_count;
    char enabled_extensions[GNOME_MAX_EXTENSIONS][256];
    int enabled_extension_count;
    int extension_disable_user_version_validation;
    int extension_allow_installation;
    int extension_auto_update_check;

    /* Applications / DBusActivatable / Startup */
    GnomeAppEntry apps[GNOME_MAX_APPS];
    int app_count;
    char favorite_apps[32][256];
    int favorite_apps_count;
    char autostart_cond[128][GNOME_MAX_PATH_LEN];
    int autostart_cond_count;
    char default_app_browser[256];
    char default_app_mail[256];
    char default_app_calendar[256];
    char default_app_music[256];
    char default_app_video[256];
    char default_app_images[256];
    char default_app_files[256];
    char default_app_terminal[256];
    char default_app_geometry[256];
    char terminal_exec[256];

    /* GSettings store */
    GnomeGSettingsItem settings[GNOME_MAX_GSETTINGS];
    int setting_count;

    /* Nautilus / Files */
    int nautilus_default_view;   /* 0 list,1 icon,2 compact */
    int nautilus_click_policy;  /* 0 single, 1 double */
    int nautilus_show_hidden;
    int nautilus_show_deleted;
    int nautilus_thumbnail_limit_mb;
    int nautilus_use_exif;
    int nautilus_preview_text;
    int nautilus_sorting_order;
    char nautilus_date_format[64];

    /* GNOME Software / PackageKit */
    int software_auto_update_frequency;
    int software_auto_download_updates;
    int software_auto_install_updates;
    int software_show_updates_notification;
    int software_check_timestamp;

    /* Shell process PIDs */
    pid_t pid_shell;
    pid_t pid_mutter;
    pid_t pid_settings_daemon;
    pid_t pid_keyring;
    pid_t pid_portal;
    pid_t pid_polkit;
    pid_t pid_screensaver;

    /* Session state */
    int running;
    int exit_code;
    int startup_phase;          /* 0 early init, 1 gsd, 2 shell, 3 ready */
    time_t started_at;
    int startup_duration_s;
    int verbose;
    int debug;
    int trace;
    int test_mode;
    int record_mode;

} GnomeState;

/* Public API */
void gnome_init(GnomeState *s);
void gnome_cleanup(GnomeState *s);
int gnome_parse_arguments(GnomeState *s, int argc, char **argv);
int gnome_load_gsettings(GnomeState *s);
int gnome_save_gsettings(GnomeState *s);
int gnome_probe_hardware(GnomeState *s);
int gnome_apply_default_preset(GnomeState *s);
int gnome_apply_classic_preset(GnomeState *s);
int gnome_apply_minimal_preset(GnomeState *s);
int gnome_start_session(GnomeState *s);
int gnome_run_session_loop(GnomeState *s);
void gnome_end_session(GnomeState *s, int code, int kill_daemons);

/* Shell / Mutters */
int gnome_window_register(GnomeState *s, const GnomeWindow *w, uint32_t *out_id);
int gnome_window_unregister(GnomeState *s, uint32_t id);
GnomeWindow *gnome_window_lookup(GnomeState *s, uint32_t id);
int gnome_workspace_switch(GnomeState *s, int target);
int gnome_workspace_move_window(GnomeState *s, uint32_t win, int target);

/* Notifications */
uint32_t gnome_notify(GnomeState *s, const GnomeNotification *in);
int gnome_notify_close(GnomeState *s, uint32_t id);

/* Extensions / Apps */
int gnome_extension_scan(GnomeState *s);
int gnome_extension_enable(GnomeState *s, const char *uuid, int enable);
int gnome_app_scan(GnomeState *s);
int gnome_app_launch(const GnomeAppEntry *app, char **argv);
int gnome_overview_search(GnomeState *s, const char *query, char out_apps[64][512], int *out_count);

/* Keybindings */
int gnome_keybinding_add(GnomeState *s, const GnomeKeybinding *k);
int gnome_keybinding_invoke(GnomeState *s, const char *schema, const char *key);

/* Power / session */
int gnome_logout(GnomeState *s);
int gnome_power_suspend(GnomeState *s);
int gnome_power_hibernate(GnomeState *s);
int gnome_power_reboot(GnomeState *s);
int gnome_power_shutdown(GnomeState *s);
int gnome_lock_screen(GnomeState *s);

/* Utilities */
void gnome_print_help(void);
void gnome_print_version(void);

#endif
