/*
 * Kenux OS - GNOME Desktop Environment (Minimal)
 * Implementation
 */

#include "gnome.h"

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#else
/* MinGW 兼容垫片 */
#include <ctype.h>
#include <errno.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#include <windows.h>
#include <signal.h>      /* MinGW 提供 signal()/SIGINT/SIGTERM */

/* POSIX 函数替代 */
#define fork()         (-1)
#define waitpid(p,s,o) (-1)
#define kill(p,s)      (-1)
#define sleep(s)       Sleep((s)*1000)
#define pause()        ((void)0)   /* MinGW 无 pause()，用 stub */

/* getuid/getgid 不存在 */
static inline int getuid(void) { return 0; }
static inline int getgid(void) { return 0; }
/* MinGW 已声明 gethostname，改用不同名称以避免 static 重声明冲突 */
static inline int kenuxk_gethostname(char *name, int len) {
    DWORD sz = (DWORD)len; return GetComputerNameA(name, &sz) ? 0 : -1;
}
#define gethostname kenuxk_gethostname

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

/* ===== Internal helpers ===== */

/* Copy src into dst ensuring NUL termination. */
static char *gs_strncpy(char *dst, const char *src, size_t n) {
    if (n == 0 || !dst) return dst;
    if (src) { strncpy(dst, src, n - 1); dst[n - 1] = '\0'; }
    else dst[0] = '\0';
    return dst;
}

static const char *gnome_session_type_name(GnomeSessionType t) {
    switch (t) {
        case GNOME_SESSION_WAYLAND: return "wayland";
        case GNOME_SESSION_X11:     return "x11";
        case GNOME_SESSION_CLASSIC:return "gnome-classic";
        case GNOME_SESSION_MOBILE:  return "phosh";
        default: return "wayland";
    }
}

static const char *gnome_color_style_name(GnomeColorStyle s) {
    switch (s) {
        case GNOME_COLOR_STYLE_DEFAULT: return "prefer-dark";
        case GNOME_COLOR_STYLE_LIGHT:   return "light";
        case GNOME_COLOR_STYLE_DARK:     return "dark";
        default: return "prefer-dark";
    }
}

/* Default GSD plugin mask (most plugins enabled). */
static void gnome_apply_gsd_default_mask(GnomeState *s) {
    s->gsd_plugin_mask =
          GNOME_GSD_A11Y_KEYBOARD
        | GNOME_GSD_AUTORUN
        | GNOME_GSD_AUTOSTART
        | GNOME_GSD_BACKGROUND
        | GNOME_GSD_CLIPBOARD
        | GNOME_GSD_COLOR
        | GNOME_GSD_DATETIME
        | GNOME_GSD_DATETIME_FORMAT
        | GNOME_GSD_HOUSEKEEPING
        | GNOME_GSD_KEYBOARD
        | GNOME_GSD_MEDIA_KEYS
        | GNOME_GSD_MOUSE
        | GNOME_GSD_ORIENTATION
        | GNOME_GSD_POWER
        | GNOME_GSD_PRINT_NOTIFICATIONS
        | GNOME_GSD_RFKILL
        | GNOME_GSD_SCREENSAVER_PROXY
        | GNOME_GSD_SHARE
        | GNOME_GSD_SOUND
        | GNOME_GSD_WACOM
        | GNOME_GSD_WWAN
        | GNOME_GSD_XSETTINGS
        | GNOME_GSD_ACCOUNT;
}

static void gnome_setup_env_defaults(GnomeState *s) {
    const char *e;
    if ((e = getenv("HOME")))         gs_strncpy(s->home, e, sizeof(s->home));
    if ((e = getenv("USER")))         gs_strncpy(s->username, e, sizeof(s->username));
    if ((e = getenv("SHELL")))        gs_strncpy(s->shell, e, sizeof(s->shell));
    if ((e = getenv("LANG")))         gs_strncpy(s->lang, e, sizeof(s->lang));
    if ((e = getenv("HOSTNAME")))     gs_strncpy(s->hostname, e, sizeof(s->hostname));
    if ((e = getenv("XDG_RUNTIME_DIR"))) gs_strncpy(s->xdg_runtime_dir, e, sizeof(s->xdg_runtime_dir));
    if ((e = getenv("XDG_DATA_DIRS")))    gs_strncpy(s->xdg_data_dirs, e, sizeof(s->xdg_data_dirs));
    if ((e = getenv("XDG_CONFIG_DIRS")))   gs_strncpy(s->xdg_config_dirs, e, sizeof(s->xdg_config_dirs));
    if ((e = getenv("XDG_CURRENT_DESKTOP"))) gs_strncpy(s->xdg_current_desktop, e, sizeof(s->xdg_current_desktop));
    if ((e = getenv("XDG_SEAT")))      gs_strncpy(s->xdg_seat, e, sizeof(s->xdg_seat));
    if ((e = getenv("XDG_VTNR")))      gs_strncpy(s->xdg_vtnr, e, sizeof(s->xdg_vtnr));
    if ((e = getenv("DISPLAY")))      gs_strncpy(s->display, e, sizeof(s->display));
    if ((e = getenv("WAYLAND_DISPLAY"))) gs_strncpy(s->wayland_display, e, sizeof(s->wayland_display));

    if (s->home[0]) {
        snprintf(s->user_config_dir, sizeof(s->user_config_dir), "%s/.config", s->home);
        snprintf(s->user_data_dir,   sizeof(s->user_data_dir),   "%s/.local/share", s->home);
        snprintf(s->user_cache_dir,  sizeof(s->user_cache_dir),  "%s/.cache", s->home);
        snprintf(s->user_state_dir,  sizeof(s->user_state_dir),  "%s/.local/state", s->home);
    }
    if (!s->xdg_current_desktop[0]) gs_strncpy(s->xdg_current_desktop, "GNOME", sizeof(s->xdg_current_desktop));
    if (!s->xdg_seat[0])            gs_strncpy(s->xdg_seat, "seat0", sizeof(s->xdg_seat));
    if (!s->xdg_vtnr[0])            gs_strncpy(s->xdg_vtnr, "1", sizeof(s->xdg_vtnr));
    if (!s->xdg_data_dirs[0])       gs_strncpy(s->xdg_data_dirs,
            "/usr/local/share:/usr/share", sizeof(s->xdg_data_dirs));
    if (!s->xdg_config_dirs[0])     gs_strncpy(s->xdg_config_dirs,
            "/etc/xdg", sizeof(s->xdg_config_dirs));
    if (!s->wayland_display[0])     gs_strncpy(s->wayland_display, "wayland-0", sizeof(s->wayland_display));
}

static void gnome_setup_default_workspaces(GnomeState *s) {
    s->dynamic_workspaces = 0;
    s->num_workspaces = 4;
    s->current_workspace = 0;
    s->workspace_only_on_primary = 0;
    s->workspace_wraps_around = 0;
    for (int i = 0; i < GNOME_MAX_WORKSPACES; i++) {
        s->workspaces[i].index = i;
        if (i < s->num_workspaces) {
            char tmp[128];
            snprintf(tmp, sizeof(tmp), "Workspace %d", i + 1);
            gs_strncpy(s->workspaces[i].name, tmp, sizeof(s->workspaces[i].name));
            gs_strncpy(s->workspace_names[i], tmp, sizeof(s->workspace_names[i]));
        }
        s->workspaces[i].window_ids = NULL;
        s->workspaces[i].win_count = 0;
        s->workspaces[i].win_cap = 0;
        s->workspaces[i].n_rows = 1;
        s->workspaces[i].n_cols = 1;
    }
}

static void gnome_setup_default_appearance(GnomeState *s) {
    s->color_style = GNOME_COLOR_STYLE_DEFAULT;
    s->accent_hex = 0x3584E4u;
    gs_strncpy(s->accent_style, "blue", sizeof(s->accent_style));
    gs_strncpy(s->icon_theme, "Adwaita", sizeof(s->icon_theme));
    gs_strncpy(s->cursor_theme, "Adwaita", sizeof(s->cursor_theme));
    s->cursor_size = 24;
    gs_strncpy(s->sound_theme, "freedesktop", sizeof(s->sound_theme));
    s->enable_sound = 1;
    s->enable_event_sounds = 0;
    s->enable_input_feedback_sounds = 0;
    gs_strncpy(s->font_name, "Cantarell 11", sizeof(s->font_name));
    gs_strncpy(s->doc_font_name, "Sans 11", sizeof(s->doc_font_name));
    gs_strncpy(s->titlebar_font, "Cantarell Bold 11", sizeof(s->titlebar_font));
    gs_strncpy(s->monospace_font_name, "Source Code Pro 10", sizeof(s->monospace_font_name));
    s->hinting = 1;
    s->antialiasing = 1;
    s->rgba_order = 1;
    s->dpi = 96;
    s->text_scaling_factor_pct = 100;
    gs_strncpy(s->theme_mode_gtk3, "Adwaita", sizeof(s->theme_mode_gtk3));
    gs_strncpy(s->theme_mode_gtk4, "Adwaita", sizeof(s->theme_mode_gtk4));
    gs_strncpy(s->mutter_dpi, "96", sizeof(s->mutter_dpi));
}

static void gnome_setup_default_wm(GnomeState *s) {
    s->focus_mode = 1;            /* sloppy */
    s->focus_new_windows = 0;     /* smart */
    s->titlebar_double_click = 0; /* toggle-maximize */
    s->titlebar_middle_click = 0; /* lower */
    s->titlebar_right_click = 0;  /* menu */
    s->edge_tiling = 1;
    s->attach_modal_dialogs = 1;
    s->auto_raise = 0;
    s->auto_raise_delay_ms = 500;
    s->raise_on_click = 1;
    s->button_layout_side = GNOME_TITLEBAR_RIGHT;
    gs_strncpy(s->button_layout, ":minimize,maximize,close", sizeof(s->button_layout));
    s->window_scaling_method = 0;
    s->compositor_enabled = 1;
    s->force_fullscreen_redraw = 0;
    s->experimental_blur = 0;
    s->experimental_rounded = 1;
    s->kms_modifiers = 1;
    s->layout = GNOME_LAYOUT_MUTTER_DEFAULT;
}

static void gnome_setup_default_topbar(GnomeState *s) {
    s->top_bar_show_activities_button = 1;
    s->top_bar_show_app_menu = 1;
    s->top_bar_show_date = 1;
    s->top_bar_show_seconds = 0;
    s->top_bar_show_weekday = 0;
    s->top_bar_show_week_number = 0;
    s->clock_format = GNOME_CLOCK_24H;
    s->top_bar_hot_corner = 1;
    s->hot_corner_delay_ms = 250;
    s->top_bar_opacity_pct = 100;
    s->top_bar_blur = 0;
}

static void gnome_setup_default_dash(GnomeState *s) {
    s->dash_show = 1;
    s->dash_position = 0;          /* left */
    s->dash_icon_size_px = 48;
    s->dash_max_icon_size = 96;
    s->dash_show_apps_at_bottom = 0;
    s->dash_show_favorites_only = 0;
    s->dash_show_running = 1;
    s->dash_dock_fixed = 0;
    s->dash_dock_extend_height = 0;
    s->dash_dock_shrink = 0;
    s->dash_dock_transparent_mode = 0;
    s->dash_dock_opacity = 0.0f;
    s->dash_autohide = 1;
    s->dash_dock_intellihide = 0;
    s->dash_click_action = 0;
    s->dash_scroll_action = 0;
    s->app_grid_columns = 6;
    s->app_grid_page_size = 24;
    s->app_grid_sort_mode = 0;
}

static void gnome_setup_default_input(GnomeState *s) {
    gs_strncpy(s->keyboard.xkb_model, "pc105", sizeof(s->keyboard.xkb_model));
    gs_strncpy(s->keyboard.xkb_layouts, "us", sizeof(s->keyboard.xkb_layouts));
    s->keyboard.xkb_variants[0] = '\0';
    s->keyboard.xkb_options[0] = '\0';
    s->keyboard.numlock_state = -1; /* remember */
    s->keyboard.delay_ms = 500;
    s->keyboard.repeat_interval_ms = 33;
    s->keyboard.input_sources_priority = 0;
    s->keyboard.show_all_sources = 0;
    s->keyboard.same_source_per_window = 0;

    s->mouse.left_handed = 0;
    s->mouse.accel_profile = 0;
    s->mouse.accel_speed = 0.0;
    s->mouse.natural_scroll = 0.0;
    s->mouse.scroll_factor = 1.0;

    s->touchpad.touchpad_enabled = 1;
    s->touchpad.tap_to_click = 0;
    s->touchpad.two_finger_scroll = 1;
    s->touchpad.edge_scroll = 0;
    s->touchpad.natural_scroll = 0;
    s->touchpad.disable_while_typing = 1;
    s->touchpad.tap_and_drag = 1;
    s->touchpad.tap_and_drag_lock = 0;
    s->touchpad.accel_profile = 0;
    s->touchpad.accel_speed = 0.0;
    s->touchpad.left_handed = 0;
    s->touchpad.click_method = 0;
    s->touchpad.middle_click_emulation = 0;
}

static void gnome_setup_default_power(GnomeState *s) {
    s->enable_lock_screen = 1;
    s->disable_lock_screen = 0;
    s->lock_disable_user_list = 0;
    s->disable_user_switch = 0;
    s->disable_user_list = 0;
    s->banner_message_text[0] = '\0';
    s->banner_message_text_enabled = 0;
    s->allowed_fails_delay = 300;
    s->screen_blank_delay_s_battery = 60;
    s->screen_blank_delay_s_ac = 300;
    s->idle_delay_battery_s = 120;
    s->idle_delay_ac_s = 600;
    s->idle_hint = 0;
    s->sleep_inactive_battery_type = GNOME_POWER_ACTION_SUSPEND;
    s->sleep_inactive_ac_type = GNOME_POWER_ACTION_NOTHING;
    s->lid_close_battery_action = GNOME_POWER_ACTION_SUSPEND;
    s->lid_close_ac_action = GNOME_POWER_ACTION_NOTHING;
    s->power_button_action = GNOME_POWER_ACTION_SUSPEND;
    s->suspend_then_hibernate = 0;
    s->show_battery_percentage = 1;
    s->low_battery_action = 0;
    s->percentage_critical = 5;
    s->percentage_action = 3;
    s->time_format = 0;

    s->nightlight.mode = GNOME_NIGHTLIGHT_MODE_OFF;
    s->nightlight.temperature_kelvin = 4500;
    s->nightlight.custom_from_hour = 20;
    s->nightlight.custom_from_minute = 0;
    s->nightlight.custom_to_hour = 6;
    s->nightlight.custom_to_minute = 0;
    s->nightlight.current_temp = 6500.0;
}

static void gnome_setup_default_notifications(GnomeState *s) {
    s->notification_count = 0;
    s->show_banners = 1;
    s->show_in_lock_screen = 0;
    s->do_not_disturb = 0;
    s->show_in_always_on_top = 0;
    s->banner_must_acknowledge = 0;
    s->notification_history_persist = 0;
    s->dnd_on_battery = 0;
    s->dnd_on_fullscreen = 1;
}

static void gnome_setup_default_wallpaper(GnomeState *s) {
    gs_strncpy(s->picture_uri,
            "file:///usr/share/backgrounds/gnome/adwaita-day.jpg",
            sizeof(s->picture_uri));
    gs_strncpy(s->picture_uri_dark,
            "file:///usr/share/backgrounds/gnome/adwaita-night.jpg",
            sizeof(s->picture_uri_dark));
    s->picture_options = 4;        /* zoom */
    s->picture_opacity_pct = 100;
    s->primary_color = 0x000000FFu;
    s->secondary_color = 0x000000FFu;
    s->color_shading_type = 2;     /* solid */
    s->show_desktop_icons = 0;
    s->desktop_icons_visible = 0;
    s->desktop_icon_size = 1;
    gs_strncpy(s->desktop_icon_layout, "grid", sizeof(s->desktop_icon_layout));
}

static void gnome_setup_default_search(GnomeState *s) {
    s->search_enabled = 1;
    s->search_show_first_result_hint = 1;
    s->search_type_ahead_prefix[0] = '\0';
    s->search_provider_count = 0;
    /* Register the built-in search providers in default order. */
    static const GnomeSearchProviderKind kinds[] = {
        GNOME_SEARCH_APPS, GNOME_SEARCH_FILES, GNOME_SEARCH_CALCULATOR,
        GNOME_SEARCH_CLOCK, GNOME_SEARCH_TERMINAL, GNOME_SEARCH_SETTINGS,
        GNOME_SEARCH_SOFTWARE, GNOME_SEARCH_CONTACTS, GNOME_SEARCH_WEB,
    };
    static const char *names[] = {
        "Applications", "Files", "Calculator", "Clock", "Terminal",
        "Settings", "Software", "Contacts", "Web",
    };
    int n = (int)(sizeof(kinds) / sizeof(kinds[0]));
    if (n > GNOME_MAX_SEARCH) n = GNOME_MAX_SEARCH;
    for (int i = 0; i < n; i++) {
        GnomeSearchProvider *p = &s->search_providers[i];
        memset(p, 0, sizeof(*p));
        snprintf(p->id, sizeof(p->id), "org.gnome.SearchProvider.%d", i);
        gs_strncpy(p->name, names[i], sizeof(p->name));
        p->enabled = 1;
        p->has_results = 0;
        p->order = i;
        p->remote = 1;
        s->search_provider_order[i] = i;
    }
    s->search_provider_count = n;
}

/* ===== Lifecycle ===== */

void gnome_init(GnomeState *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));

    s->session_type = GNOME_SESSION_WAYLAND;
    gs_strncpy(s->session_class, "user", sizeof(s->session_class));
    gs_strncpy(s->session_desktop, "gnome", sizeof(s->session_desktop));

    gnome_setup_env_defaults(s);

    s->uid = getuid();
    s->gid = getgid();
    s->pid = getpid();
    if (!s->hostname[0]) {
        if (gethostname(s->hostname, sizeof(s->hostname) - 1) != 0)
            gs_strncpy(s->hostname, "localhost", sizeof(s->hostname));
    }
    if (!s->lang[0]) gs_strncpy(s->lang, "en_US.UTF-8", sizeof(s->lang));
    if (!s->shell[0]) gs_strncpy(s->shell, "/bin/bash", sizeof(s->shell));

    /* Display backend */
    s->display_number = 0;
    s->headless = 0;
    s->nested = 0;
    s->unsafe_mode = 0;

    /* Mutter / GSD */
    gnome_apply_gsd_default_mask(s);
    gnome_setup_default_wm(s);
    gnome_setup_default_appearance(s);
    gnome_setup_default_workspaces(s);
    gnome_setup_default_topbar(s);
    gnome_setup_default_dash(s);
    gnome_setup_default_input(s);
    gnome_setup_default_power(s);
    gnome_setup_default_notifications(s);
    gnome_setup_default_wallpaper(s);
    gnome_setup_default_search(s);

    /* Overview */
    s->overview_show_workspaces_only_on_primary = 0;
    s->overview_workspace_switcher_only_on_primary = 0;
    s->overview_hot_corner = 1;
    s->gesture_activation_threshold = 0;
    s->overview_gap_size_px = 24;

    /* Monitors */
    s->monitor_count = 1;
    s->primary_monitor = 0;
    GnomeMonitor *m = &s->monitors[0];
    gs_strncpy(m->vendor, "KenuxK", sizeof(m->vendor));
    gs_strncpy(m->product, "Generic Display", sizeof(m->product));
    gs_strncpy(m->connector, "eDP-1", sizeof(m->connector));
    gs_strncpy(m->display_name, "Built-in display", sizeof(m->display_name));
    m->width = 1920;
    m->height = 1080;
    m->refresh_mhz = 60000;
    m->scale_1000 = 1000;
    m->global_scale_1000 = 1000;
    m->primary = 1;
    m->builtin = 1;
    m->transform = 0;
    m->underscanning = 0;
    m->color_profile_id = 0;

    /* Windows */
    s->window_count = 0;
    s->window_id_counter = 1;
    s->focus_window_id = 0;
    s->pointer_window_id = 0;

    /* Accessibility defaults */
    s->toolkit_accessibility = 1;
    s->a11y_zoom_factor_pct = 200;

    /* Nautilus */
    s->nautilus_default_view = 1;       /* icon */
    s->nautilus_click_policy = 1;       /* double */
    s->nautilus_show_hidden = 0;
    s->nautilus_show_deleted = 0;
    s->nautilus_thumbnail_limit_mb = 10;
    s->nautilus_use_exif = 1;
    s->nautilus_preview_text = 1;
    s->nautilus_sorting_order = 0;
    gs_strncpy(s->nautilus_date_format, "locale", sizeof(s->nautilus_date_format));

    /* GNOME Software */
    s->software_auto_update_frequency = 86400;
    s->software_auto_download_updates = 1;
    s->software_auto_install_updates = 0;
    s->software_show_updates_notification = 1;
    s->software_check_timestamp = 0;

    /* Default apps */
    gs_strncpy(s->default_app_browser,  "org.gnome.Epiphany", sizeof(s->default_app_browser));
    gs_strncpy(s->default_app_mail,     "org.gnome.Geary", sizeof(s->default_app_mail));
    gs_strncpy(s->default_app_calendar, "org.gnome.Calendar", sizeof(s->default_app_calendar));
    gs_strncpy(s->default_app_music,    "org.gnome.Music", sizeof(s->default_app_music));
    gs_strncpy(s->default_app_video,    "org.gnome.Totem", sizeof(s->default_app_video));
    gs_strncpy(s->default_app_images,   "org.gnome.eog", sizeof(s->default_app_images));
    gs_strncpy(s->default_app_files,    "org.gnome.Nautilus", sizeof(s->default_app_files));
    gs_strncpy(s->default_app_terminal, "org.gnome.Terminal", sizeof(s->default_app_terminal));
    gs_strncpy(s->terminal_exec, "gnome-terminal", sizeof(s->terminal_exec));

    /* Session state */
    s->running = 0;
    s->exit_code = 0;
    s->startup_phase = 0;
    s->started_at = time(NULL);
    s->startup_duration_s = 0;
    s->verbose = 0;
    s->debug = 0;
    s->trace = 0;
    s->test_mode = 0;
    s->record_mode = 0;

    s->pid_shell = 0;
    s->pid_mutter = 0;
    s->pid_settings_daemon = 0;
    s->pid_keyring = 0;
    s->pid_portal = 0;
    s->pid_polkit = 0;
    s->pid_screensaver = 0;
}

void gnome_cleanup(GnomeState *s) {
    if (!s) return;
    /* Free per-workspace window id arrays */
    for (int i = 0; i < GNOME_MAX_WORKSPACES; i++) {
        if (s->workspaces[i].window_ids) {
            free(s->workspaces[i].window_ids);
            s->workspaces[i].window_ids = NULL;
        }
        s->workspaces[i].win_count = 0;
        s->workspaces[i].win_cap = 0;
    }
    /* Reset transient bookkeeping */
    s->window_count = 0;
    s->notification_count = 0;
    s->running = 0;
}

/* ===== Argument parsing ===== */

int gnome_parse_arguments(GnomeState *s, int argc, char **argv) {
    if (!s) return -1;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!a) continue;

        if (strcmp(a, "--version") == 0) {
            gnome_print_version();
            return 1;
        } else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            gnome_print_help();
            return 1;
        } else if (strcmp(a, "--verbose") == 0 || strcmp(a, "-v") == 0) {
            s->verbose++;
        } else if (strcmp(a, "--debug") == 0 || strcmp(a, "-d") == 0) {
            s->debug = 1;
        } else if (strcmp(a, "--trace") == 0) {
            s->trace = 1;
        } else if (strcmp(a, "--test-mode") == 0 || strcmp(a, "--test") == 0) {
            s->test_mode = 1;
        } else if (strcmp(a, "--record") == 0) {
            s->record_mode = 1;
        } else if (strcmp(a, "--wayland") == 0) {
            s->session_type = GNOME_SESSION_WAYLAND;
        } else if (strcmp(a, "--x11") == 0 || strcmp(a, "--xorg") == 0) {
            s->session_type = GNOME_SESSION_X11;
        } else if (strcmp(a, "--classic") == 0) {
            s->session_type = GNOME_SESSION_CLASSIC;
        } else if (strcmp(a, "--mobile") == 0 || strcmp(a, "--phosh") == 0) {
            s->session_type = GNOME_SESSION_MOBILE;
        } else if (strcmp(a, "--nested") == 0) {
            s->nested = 1;
        } else if (strcmp(a, "--headless") == 0) {
            s->headless = 1;
        } else if (strcmp(a, "--unsafe-mode") == 0) {
            s->unsafe_mode = 1;
        } else if (strcmp(a, "--replace") == 0) {
            /* replace existing shell -- ignored in skeleton */
        } else if (strcmp(a, "--sm-disable") == 0) {
            /* disable session management */
        } else if (strcmp(a, "--disable-extensions") == 0) {
            for (int j = 0; j < s->extension_count; j++) s->extensions[j].enabled = 0;
            s->enabled_extension_count = 0;
        } else if (strcmp(a, "--list-extensions") == 0) {
            gnome_extension_scan(s);
            for (int j = 0; j < s->extension_count; j++) {
                printf("%s\t%s\t%s\n",
                       s->extensions[j].uuid,
                       s->extensions[j].enabled ? "enabled" : "disabled",
                       s->extensions[j].name);
            }
            return 1;
        } else if (strcmp(a, "--enable-extension") == 0 && i + 1 < argc) {
            gnome_extension_enable(s, argv[++i], 1);
        } else if (strcmp(a, "--disable-extension") == 0 && i + 1 < argc) {
            gnome_extension_enable(s, argv[++i], 0);
        } else if (strcmp(a, "--session") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "wayland") == 0) s->session_type = GNOME_SESSION_WAYLAND;
            else if (strcmp(v, "x11") == 0) s->session_type = GNOME_SESSION_X11;
            else if (strcmp(v, "gnome-classic") == 0) s->session_type = GNOME_SESSION_CLASSIC;
            else if (strcmp(v, "phosh") == 0) s->session_type = GNOME_SESSION_MOBILE;
        } else if (strncmp(a, "--session=", 10) == 0) {
            const char *v = a + 10;
            if (strcmp(v, "x11") == 0) s->session_type = GNOME_SESSION_X11;
            else if (strcmp(v, "gnome-classic") == 0) s->session_type = GNOME_SESSION_CLASSIC;
            else if (strcmp(v, "phosh") == 0) s->session_type = GNOME_SESSION_MOBILE;
            else s->session_type = GNOME_SESSION_WAYLAND;
        } else if (strcmp(a, "--display") == 0 && i + 1 < argc) {
            gs_strncpy(s->display, argv[++i], sizeof(s->display));
        } else if (strncmp(a, "--display=", 10) == 0) {
            gs_strncpy(s->display, a + 10, sizeof(s->display));
        } else if (strcmp(a, "--wayland-display") == 0 && i + 1 < argc) {
            gs_strncpy(s->wayland_display, argv[++i], sizeof(s->wayland_display));
        } else if (strcmp(a, "--sm-client-id") == 0 && i + 1 < argc) {
            gs_strncpy(s->session_id, argv[++i], sizeof(s->session_id));
        } else if (strncmp(a, "--sm-client-id=", 15) == 0) {
            gs_strncpy(s->session_id, a + 15, sizeof(s->session_id));
        } else if (strcmp(a, "--mode") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "default") == 0) gnome_apply_default_preset(s);
            else if (strcmp(v, "classic") == 0) gnome_apply_classic_preset(s);
            else if (strcmp(v, "minimal") == 0) gnome_apply_minimal_preset(s);
        } else if (strncmp(a, "--mode=", 7) == 0) {
            const char *v = a + 7;
            if (strcmp(v, "default") == 0) gnome_apply_default_preset(s);
            else if (strcmp(v, "classic") == 0) gnome_apply_classic_preset(s);
            else if (strcmp(v, "minimal") == 0) gnome_apply_minimal_preset(s);
        } else if (strcmp(a, "--preset") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (strcmp(v, "default") == 0) gnome_apply_default_preset(s);
            else if (strcmp(v, "classic") == 0) gnome_apply_classic_preset(s);
            else if (strcmp(v, "minimal") == 0) gnome_apply_minimal_preset(s);
        } else if (strcmp(a, "--scan-extensions") == 0) {
            gnome_extension_scan(s);
        } else if (strcmp(a, "--scan-apps") == 0) {
            gnome_app_scan(s);
        } else if (strcmp(a, "--probe-hardware") == 0) {
            gnome_probe_hardware(s);
        } else if (strcmp(a, "--load-gsettings") == 0) {
            gnome_load_gsettings(s);
        } else if (strcmp(a, "--save-gsettings") == 0) {
            gnome_save_gsettings(s);
        }
        /* Unknown args are silently ignored */
    }
    return 0;
}

/* ===== GSettings load / save ===== */

/* Persist path for the in-memory GSettings cache. */
static void gnome_gsettings_path(const GnomeState *s, char *out, size_t n) {
    if (s->user_config_dir[0])
        snprintf(out, n, "%s/gnome-shell/gsettings.db", s->user_config_dir);
    else
        snprintf(out, n, "%s/.config/gnome-shell/gsettings.db", s->home[0] ? s->home : "/tmp");
}

int gnome_load_gsettings(GnomeState *s) {
    if (!s) return -1;
    char path[GNOME_MAX_PATH_LEN];
    gnome_gsettings_path(s, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) {
        if (s->debug) fprintf(stderr, "gnome: no gsettings file at %s\n", path);
        return 0;
    }
    char line[8192];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;
        char *nl = strpbrk(line, "\r\n"); if (nl) *nl = '\0';
        if (count >= GNOME_MAX_GSETTINGS) break;
        /* Format: schema\tkey\ttype\tpath\tvalue */
        char *fields[5] = {0};
        char *p = line;
        for (int fi = 0; fi < 5; fi++) {
            fields[fi] = p;
            char *tab = strchr(p, '\t');
            if (!tab) break;
            *tab = '\0';
            p = tab + 1;
        }
        if (!fields[0] || !fields[1]) continue;
        GnomeGSettingsItem *it = &s->settings[count++];
        memset(it, 0, sizeof(*it));
        gs_strncpy(it->schema, fields[0], sizeof(it->schema));
        gs_strncpy(it->key, fields[1], sizeof(it->key));
        if (fields[2]) gs_strncpy(it->type_signature, fields[2], sizeof(it->type_signature));
        if (fields[3]) {
            gs_strncpy(it->path, fields[3], sizeof(it->path));
            it->path_len = (int)strlen(it->path);
        }
        if (fields[4]) gs_strncpy(it->value, fields[4], sizeof(it->value));
    }
    s->setting_count = count;
    fclose(f);
    if (s->verbose) fprintf(stderr, "gnome: loaded %d gsettings entries from %s\n", count, path);
    return 0;
}

int gnome_save_gsettings(GnomeState *s) {
    if (!s) return -1;
    char path[GNOME_MAX_PATH_LEN];
    gnome_gsettings_path(s, path, sizeof(path));
    /* Best-effort directory creation */
    char dir[GNOME_MAX_PATH_LEN];
    snprintf(dir, sizeof(dir), "%s/gnome-shell", s->user_config_dir[0] ? s->user_config_dir : "/tmp");
    mkdir(dir, 0700);
    FILE *f = fopen(path, "w");
    if (!f) {
        if (s->debug) fprintf(stderr, "gnome: cannot write gsettings to %s: %s\n", path, strerror(errno));
        return -1;
    }
    fprintf(f, "# KenuxK GNOME GSettings cache (auto-generated)\n");
    for (int i = 0; i < s->setting_count; i++) {
        GnomeGSettingsItem *it = &s->settings[i];
        fprintf(f, "%s\t%s\t%s\t%s\t%s\n",
                it->schema, it->key,
                it->type_signature[0] ? it->type_signature : "s",
                it->path[0] ? it->path : "/",
                it->value[0] ? it->value : "");
    }
    fclose(f);
    if (s->verbose) fprintf(stderr, "gnome: saved %d gsettings entries to %s\n", s->setting_count, path);
    return 0;
}

/* ===== Hardware probe ===== */

int gnome_probe_hardware(GnomeState *s) {
    if (!s) return -1;
    /* Skeleton: ensure at least one monitor and sane defaults.
     * A real implementation would query DRM/KMS via udev. */
    if (s->monitor_count <= 0) {
        s->monitor_count = 1;
        s->primary_monitor = 0;
    }
    for (int i = 0; i < s->monitor_count && i < GNOME_MAX_MONITORS; i++) {
        GnomeMonitor *m = &s->monitors[i];
        if (m->width <= 0)  m->width = 1920;
        if (m->height <= 0) m->height = 1080;
        if (m->refresh_mhz <= 0) m->refresh_mhz = 60000;
        if (m->scale_1000 <= 0) m->scale_1000 = 1000;
        if (m->global_scale_1000 <= 0) m->global_scale_1000 = 1000;
        if (m->connector[0] == '\0') {
            char c[64];
            snprintf(c, sizeof(c), "eDP-%d", i + 1);
            gs_strncpy(m->connector, c, sizeof(m->connector));
        }
        if (m->vendor[0] == '\0') gs_strncpy(m->vendor, "KenuxK", sizeof(m->vendor));
    }
    s->dpi = 96;
    if (s->verbose) {
        fprintf(stderr, "gnome: probed %d monitor(s)\n", s->monitor_count);
        for (int i = 0; i < s->monitor_count; i++) {
            GnomeMonitor *m = &s->monitors[i];
            fprintf(stderr, "  [%d] %s %dx%d@%.2fHz scale=%.2f%s\n",
                    i, m->connector, m->width, m->height,
                    m->refresh_mhz / 1000.0, m->scale_1000 / 1000.0,
                    m->primary ? " (primary)" : "");
        }
    }
    return 0;
}

/* ===== Presets ===== */

int gnome_apply_default_preset(GnomeState *s) {
    if (!s) return -1;
    s->session_type = GNOME_SESSION_WAYLAND;
    gnome_apply_gsd_default_mask(s);
    gnome_setup_default_wm(s);
    gnome_setup_default_appearance(s);
    gnome_setup_default_topbar(s);
    gnome_setup_default_dash(s);
    s->color_style = GNOME_COLOR_STYLE_DEFAULT;
    gs_strncpy(s->theme_mode_gtk3, "Adwaita", sizeof(s->theme_mode_gtk3));
    gs_strncpy(s->theme_mode_gtk4, "Adwaita", sizeof(s->theme_mode_gtk4));
    gs_strncpy(s->icon_theme, "Adwaita", sizeof(s->icon_theme));
    s->layout = GNOME_LAYOUT_MUTTER_DEFAULT;
    s->compositor_enabled = 1;
    s->experimental_rounded = 1;
    return 0;
}

int gnome_apply_classic_preset(GnomeState *s) {
    if (!s) return -1;
    s->session_type = GNOME_SESSION_CLASSIC;
    gnome_apply_gsd_default_mask(s);
    /* Classic: static workspaces, light theme, no hot corner, bottom dash */
    s->dynamic_workspaces = 0;
    s->num_workspaces = 4;
    s->current_workspace = 0;
    s->color_style = GNOME_COLOR_STYLE_LIGHT;
    gs_strncpy(s->theme_mode_gtk3, "Adwaita", sizeof(s->theme_mode_gtk3));
    gs_strncpy(s->theme_mode_gtk4, "Adwaita", sizeof(s->theme_mode_gtk4));
    s->top_bar_hot_corner = 0;
    s->overview_hot_corner = 0;
    s->dash_position = 2;          /* bottom */
    s->dash_dock_fixed = 1;
    s->dash_dock_extend_height = 1;
    s->top_bar_show_activities_button = 1;
    s->top_bar_show_app_menu = 1;
    s->edge_tiling = 0;
    s->button_layout_side = GNOME_TITLEBAR_RIGHT;
    gs_strncpy(s->button_layout, ":minimize,maximize,close", sizeof(s->button_layout));
    s->layout = GNOME_LAYOUT_MUTTER_DEFAULT;
    return 0;
}

int gnome_apply_minimal_preset(GnomeState *s) {
    if (!s) return -1;
    s->session_type = GNOME_SESSION_WAYLAND;
    /* Minimal: fewest GSD plugins, single workspace, reduced shell chrome */
    s->gsd_plugin_mask =
          GNOME_GSD_BACKGROUND
        | GNOME_GSD_CLIPBOARD
        | GNOME_GSD_COLOR
        | GNOME_GSD_DATETIME
        | GNOME_GSD_KEYBOARD
        | GNOME_GSD_MEDIA_KEYS
        | GNOME_GSD_MOUSE
        | GNOME_GSD_POWER
        | GNOME_GSD_SOUND
        | GNOME_GSD_XSETTINGS;
    s->dynamic_workspaces = 0;
    s->num_workspaces = 1;
    s->current_workspace = 0;
    s->color_style = GNOME_COLOR_STYLE_DARK;
    gs_strncpy(s->theme_mode_gtk3, "Adwaita-dark", sizeof(s->theme_mode_gtk3));
    gs_strncpy(s->theme_mode_gtk4, "Adwaita-dark", sizeof(s->theme_mode_gtk4));
    s->top_bar_show_activities_button = 0;
    s->top_bar_show_app_menu = 0;
    s->top_bar_show_seconds = 0;
    s->top_bar_hot_corner = 0;
    s->overview_hot_corner = 0;
    s->dash_show = 0;
    s->show_desktop_icons = 0;
    s->edge_tiling = 1;
    s->attach_modal_dialogs = 1;
    s->experimental_blur = 0;
    s->experimental_rounded = 1;
    s->layout = GNOME_LAYOUT_MUTTER_DEFAULT;
    return 0;
}

/* ===== Session management ===== */

/* Signal handler entrypoint: flip running to 0 on common exit signals. */
static GnomeState *g_gnome_sig_state = NULL;
static void gnome_on_term_signal(int sig) {
    (void)sig;
    if (g_gnome_sig_state) {
        g_gnome_sig_state->running = 0;
        g_gnome_sig_state->exit_code = 0;
    }
}

int gnome_start_session(GnomeState *s) {
    if (!s) return -1;
    if (s->verbose)
        fprintf(stderr, "gnome: starting session (type=%s)\n",
                gnome_session_type_name(s->session_type));

    /* Generate a session id if none provided. */
    if (!s->session_id[0]) {
        snprintf(s->session_id, sizeof(s->session_id), "kenuxk-%d-%ld",
                 (int)getpid(), (long)time(NULL));
    }
    s->started_at = time(NULL);
    s->startup_phase = 0;
    s->exit_code = 0;

    /* Probe hardware if not already done. */
    if (s->monitor_count <= 0) gnome_probe_hardware(s);

    /* Load persisted settings. */
    gnome_load_gsettings(s);

    /* Scan for installed extensions and applications. */
    gnome_extension_scan(s);
    gnome_app_scan(s);

    /* Reset window/notification stores. */
    s->window_count = 0;
    s->notification_count = 0;
    s->window_id_counter = 1;

    /* Mark daemons as "started" (skeleton: record our own pid). */
    s->pid_shell = getpid();
    s->pid_mutter = 0;
    s->pid_settings_daemon = 0;
    s->pid_keyring = 0;
    s->pid_portal = 0;
    s->pid_polkit = 0;
    s->pid_screensaver = 0;

    /* Install signal handlers so the loop can exit cleanly. */
    g_gnome_sig_state = s;
    signal(SIGINT,  gnome_on_term_signal);
    signal(SIGTERM, gnome_on_term_signal);

    s->startup_phase = 3;        /* ready */
    s->running = 1;
    return 0;
}

int gnome_run_session_loop(GnomeState *s) {
    if (!s) return -1;
    if (!s->running) return s->exit_code;
    if (s->test_mode) {
        s->running = 0;
        return 0;
    }
    /* Skeleton: block until the running flag is cleared by a signal
     * or by gnome_end_session/gnome_logout/etc. */
    while (s->running) {
        pause();
    }
    return s->exit_code;
}

void gnome_end_session(GnomeState *s, int code, int kill_daemons) {
    if (!s) return;
    s->exit_code = code;
    s->running = 0;
    s->startup_phase = 0;
    /* Persist settings before tearing down. */
    gnome_save_gsettings(s);
    /* Skeleton: no real child daemons were spawned. */
    if (kill_daemons) {
        s->pid_shell = 0;
        s->pid_mutter = 0;
        s->pid_settings_daemon = 0;
        s->pid_keyring = 0;
        s->pid_portal = 0;
        s->pid_polkit = 0;
        s->pid_screensaver = 0;
    }
    if (g_gnome_sig_state == s) g_gnome_sig_state = NULL;
}

/* ===== Windows ===== */

/* Locate the first free window slot (id == 0 means empty). */
static int gnome_window_find_slot(GnomeState *s) {
    for (int i = 0; i < GNOME_MAX_WINDOWS; i++)
        if (s->windows[i].id == 0) return i;
    return -1;
}

/* Workspace window-list maintenance. */
static int gnome_workspace_add_window(GnomeWorkspace *ws, uint32_t id) {
    if (!ws) return -1;
    if (ws->win_count >= ws->win_cap) {
        int newcap = ws->win_cap ? ws->win_cap * 2 : 16;
        uint32_t *nb = (uint32_t *)realloc(ws->window_ids, sizeof(uint32_t) * newcap);
        if (!nb) return -1;
        ws->window_ids = nb;
        ws->win_cap = newcap;
    }
    ws->window_ids[ws->win_count++] = id;
    return 0;
}

static int gnome_workspace_remove_window(GnomeWorkspace *ws, uint32_t id) {
    if (!ws || !ws->window_ids) return 0;
    int j = 0;
    for (int i = 0; i < ws->win_count; i++) {
        if (ws->window_ids[i] == id) continue;
        ws->window_ids[j++] = ws->window_ids[i];
    }
    ws->win_count = j;
    return 0;
}

int gnome_window_register(GnomeState *s, const GnomeWindow *w, uint32_t *out_id) {
    if (!s || !w) return -1;
    int slot = gnome_window_find_slot(s);
    if (slot < 0) return -1;
    uint32_t id = s->window_id_counter++;
    if (id == 0) id = s->window_id_counter++;     /* skip 0 */
    GnomeWindow *dst = &s->windows[slot];
    *dst = *w;
    dst->id = id;
    dst->next = NULL;             /* array is authoritative */
    if (dst->workspace >= (uint32_t)GNOME_MAX_WORKSPACES)
        dst->workspace = (uint32_t)s->current_workspace;
    gnome_workspace_add_window(&s->workspaces[dst->workspace], id);
    s->window_count++;
    if (out_id) *out_id = id;
    if (s->focus_window_id == 0) s->focus_window_id = id;
    return 0;
}

int gnome_window_unregister(GnomeState *s, uint32_t id) {
    if (!s || id == 0) return -1;
    for (int i = 0; i < GNOME_MAX_WINDOWS; i++) {
        if (s->windows[i].id == id) {
            uint32_t ws = s->windows[i].workspace;
            if (ws < (uint32_t)GNOME_MAX_WORKSPACES)
                gnome_workspace_remove_window(&s->workspaces[ws], id);
            memset(&s->windows[i], 0, sizeof(s->windows[i]));
            s->window_count--;
            if (s->focus_window_id == id) s->focus_window_id = 0;
            if (s->pointer_window_id == id) s->pointer_window_id = 0;
            return 0;
        }
    }
    return -1;
}

GnomeWindow *gnome_window_lookup(GnomeState *s, uint32_t id) {
    if (!s || id == 0) return NULL;
    for (int i = 0; i < GNOME_MAX_WINDOWS; i++)
        if (s->windows[i].id == id) return &s->windows[i];
    return NULL;
}

/* ===== Workspaces ===== */

int gnome_workspace_switch(GnomeState *s, int target) {
    if (!s) return -1;
    if (target < 0) {
        if (s->workspace_wraps_around)
            target = s->num_workspaces - 1;
        else
            return -1;
    }
    if (target >= s->num_workspaces) {
        if (s->workspace_wraps_around)
            target = 0;
        else
            return -1;
    }
    if (target == s->current_workspace) return 0;
    s->current_workspace = target;
    return 0;
}

int gnome_workspace_move_window(GnomeState *s, uint32_t win, int target) {
    if (!s) return -1;
    if (target < 0 || target >= GNOME_MAX_WORKSPACES) return -1;
    GnomeWindow *w = gnome_window_lookup(s, win);
    if (!w) return -1;
    if ((int)w->workspace == target) return 0;
    if (w->workspace < (uint32_t)GNOME_MAX_WORKSPACES)
        gnome_workspace_remove_window(&s->workspaces[w->workspace], win);
    w->workspace = (uint32_t)target;
    gnome_workspace_add_window(&s->workspaces[target], win);
    return 0;
}

/* ===== Notifications ===== */

uint32_t gnome_notify(GnomeState *s, const GnomeNotification *in) {
    if (!s || !in) return 0;
    int slot = -1;
    for (int i = 0; i < GNOME_MAX_NOTIFICATIONS; i++) {
        if (s->notifications[i].id == 0) { slot = i; break; }
    }
    if (slot < 0) {
        /* Evict the oldest to make room. */
        slot = 0;
        time_t oldest = s->notifications[0].created_at;
        for (int i = 1; i < GNOME_MAX_NOTIFICATIONS; i++) {
            if (s->notifications[i].created_at < oldest) {
                oldest = s->notifications[i].created_at;
                slot = i;
            }
        }
        s->notification_count--;
    }
    GnomeNotification *n = &s->notifications[slot];
    *n = *in;
    n->id = (uint32_t)(slot + 1);
    n->created_at = time(NULL);
    n->read = 0;
    n->dismissed = 0;
    s->notification_count++;
    return n->id;
}

int gnome_notify_close(GnomeState *s, uint32_t id) {
    if (!s || id == 0) return -1;
    int idx = (int)id - 1;
    if (idx < 0 || idx >= GNOME_MAX_NOTIFICATIONS) return -1;
    if (s->notifications[idx].id != id) {
        for (int i = 0; i < GNOME_MAX_NOTIFICATIONS; i++) {
            if (s->notifications[i].id == id) { idx = i; break; }
            if (i == GNOME_MAX_NOTIFICATIONS - 1) return -1;
        }
    }
    s->notifications[idx].dismissed = 1;
    memset(&s->notifications[idx], 0, sizeof(s->notifications[idx]));
    if (s->notification_count > 0) s->notification_count--;
    return 0;
}

/* ===== Extensions ===== */

static void gnome_extension_parse_metadata(GnomeExtension *e, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n"); if (nl) *nl = '\0';
        /* Very small JSON-ish parser: "key": "value", */
        char *q1 = strchr(line, '"');
        if (!q1) continue;
        char *q2 = strchr(q1 + 1, '"');
        if (!q2) continue;
        char key[128] = {0};
        size_t klen = q2 - q1 - 1;
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, q1 + 1, klen); key[klen] = '\0';
        char *c = strchr(q2 + 1, ':');
        if (!c) continue;
        char *q3 = strchr(c, '"');
        if (!q3) continue;
        char *q4 = strchr(q3 + 1, '"');
        if (!q4) continue;
        char val[1024] = {0};
        size_t vlen = q4 - q3 - 1;
        if (vlen >= sizeof(val)) vlen = sizeof(val) - 1;
        memcpy(val, q3 + 1, vlen); val[vlen] = '\0';

        if (strcmp(key, "name") == 0) gs_strncpy(e->name, val, sizeof(e->name));
        else if (strcmp(key, "description") == 0) gs_strncpy(e->description, val, sizeof(e->description));
        else if (strcmp(key, "version") == 0) gs_strncpy(e->version_str, val, sizeof(e->version_str));
        else if (strcmp(key, "url") == 0) gs_strncpy(e->url, val, sizeof(e->url));
        else if (strcmp(key, "shell-version") == 0) gs_strncpy(e->shell_version_min, val, sizeof(e->shell_version_min));
    }
    fclose(f);
}

int gnome_extension_scan(GnomeState *s) {
    if (!s) return -1;
    s->extension_count = 0;
    /* User-installed extensions. */
    char dir[GNOME_MAX_PATH_LEN];
    if (s->user_data_dir[0])
        snprintf(dir, sizeof(dir), "%s/gnome-shell/extensions", s->user_data_dir);
    else
        snprintf(dir, sizeof(dir), "%s/.local/share/gnome-shell/extensions", s->home[0] ? s->home : "");

    DIR *d = opendir(dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            if (s->extension_count >= GNOME_MAX_EXTENSIONS) break;
            char full[GNOME_MAX_PATH_LEN];
            snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
            struct stat st;
            if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            GnomeExtension *e = &s->extensions[s->extension_count++];
            memset(e, 0, sizeof(*e));
            gs_strncpy(e->uuid, de->d_name, sizeof(e->uuid));
            gs_strncpy(e->path, full, sizeof(e->path));
            e->enabled = 0;
            e->can_disable = 1;
            char meta[GNOME_MAX_PATH_LEN];
            snprintf(meta, sizeof(meta), "%s/metadata.json", full);
            gnome_extension_parse_metadata(e, meta);
        }
        closedir(d);
    }
    /* Apply the saved enabled-extensions list. */
    for (int i = 0; i < s->enabled_extension_count; i++) {
        const char *uuid = s->enabled_extensions[i];
        for (int j = 0; j < s->extension_count; j++) {
            if (strcmp(s->extensions[j].uuid, uuid) == 0) {
                s->extensions[j].enabled = 1;
                break;
            }
        }
    }
    if (s->verbose)
        fprintf(stderr, "gnome: scanned %d extension(s)\n", s->extension_count);
    return s->extension_count;
}

int gnome_extension_enable(GnomeState *s, const char *uuid, int enable) {
    if (!s || !uuid) return -1;
    /* Update in-memory list if present. */
    int found = 0;
    for (int i = 0; i < s->extension_count; i++) {
        if (strcmp(s->extensions[i].uuid, uuid) == 0) {
            s->extensions[i].enabled = enable ? 1 : 0;
            found = 1;
            break;
        }
    }
    /* Maintain the enabled-uuids list. */
    int already = 0;
    for (int i = 0; i < s->enabled_extension_count; i++) {
        if (strcmp(s->enabled_extensions[i], uuid) == 0) { already = 1; break; }
    }
    if (enable && !already && s->enabled_extension_count < GNOME_MAX_EXTENSIONS) {
        gs_strncpy(s->enabled_extensions[s->enabled_extension_count++],
                   uuid, sizeof(s->enabled_extensions[0]));
    } else if (!enable && already) {
        int j = 0;
        for (int i = 0; i < s->enabled_extension_count; i++) {
            if (strcmp(s->enabled_extensions[i], uuid) == 0) continue;
            if (j != i)
                strncpy(s->enabled_extensions[j], s->enabled_extensions[i],
                        sizeof(s->enabled_extensions[j]) - 1);
            j++;
        }
        s->enabled_extension_count = j;
    }
    if (!found && enable) return -1;
    return 0;
}

/* ===== Applications ===== */

static int gnome_parse_desktop_entry(GnomeAppEntry *a, const char *line) {
    char key[128] = {0};
    const char *eq = strchr(line, '=');
    if (!eq) return -1;
    size_t klen = (size_t)(eq - line);
    if (klen >= sizeof(key)) klen = sizeof(key) - 1;
    memcpy(key, line, klen); key[klen] = '\0';
    const char *v = eq + 1;
    if (strcmp(key, "Name") == 0)              gs_strncpy(a->name, v, sizeof(a->name));
    else if (strcmp(key, "GenericName") == 0)  gs_strncpy(a->generic_name, v, sizeof(a->generic_name));
    else if (strcmp(key, "Comment") == 0)     gs_strncpy(a->comment, v, sizeof(a->comment));
    else if (strcmp(key, "Keywords") == 0)    gs_strncpy(a->keywords, v, sizeof(a->keywords));
    else if (strcmp(key, "Categories") == 0)  gs_strncpy(a->categories, v, sizeof(a->categories));
    else if (strcmp(key, "OnlyShowIn") == 0)  gs_strncpy(a->only_show_in, v, sizeof(a->only_show_in));
    else if (strcmp(key, "NotShowIn") == 0)   gs_strncpy(a->not_show_in, v, sizeof(a->not_show_in));
    else if (strcmp(key, "Exec") == 0)        gs_strncpy(a->exec, v, sizeof(a->exec));
    else if (strcmp(key, "TryExec") == 0)     gs_strncpy(a->tryexec, v, sizeof(a->tryexec));
    else if (strcmp(key, "Icon") == 0)        gs_strncpy(a->icon, v, sizeof(a->icon));
    else if (strcmp(key, "Path") == 0)       gs_strncpy(a->path, v, sizeof(a->path));
    else if (strcmp(key, "MimeType") == 0)    gs_strncpy(a->mime_type, v, sizeof(a->mime_type));
    else if (strcmp(key, "StartupWMClass") == 0) gs_strncpy(a->startup_wm_class, v, sizeof(a->startup_wm_class));
    else if (strcmp(key, "StartupNotify") == 0)  a->startup_notify = atoi(v);
    else if (strcmp(key, "Terminal") == 0)        a->terminal = atoi(v);
    else if (strcmp(key, "NoDisplay") == 0)      a->nodisplay = atoi(v);
    else if (strcmp(key, "Hidden") == 0)         a->hidden = atoi(v);
    else if (strcmp(key, "DBusActivatable") == 0)a->dbus_activatable = atoi(v);
    return 0;
}

static int gnome_load_desktop_file(GnomeAppEntry *a, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    memset(a, 0, sizeof(*a));
    gs_strncpy(a->filename, path, sizeof(a->filename));
    /* Derive id from the filename. */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    gs_strncpy(a->id, base, sizeof(a->id));
    char line[4096];
    int in_entry = 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n"); if (nl) *nl = '\0';
        if (line[0] == '[') {
            in_entry = (strcmp(line, "[Desktop Entry]") == 0);
            continue;
        }
        if (!in_entry) continue;
        if (line[0] == '#' || line[0] == '\0') continue;
        gnome_parse_desktop_entry(a, line);
    }
    fclose(f);
    return 0;
}

static void gnome_scan_app_dir(GnomeState *s, const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        const char *name = de->d_name;
        size_t len = strlen(name);
        if (len < 8) continue;
        if (strcmp(name + len - 8, ".desktop") != 0) continue;
        if (s->app_count >= GNOME_MAX_APPS) break;
        char full[GNOME_MAX_PATH_LEN];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (gnome_load_desktop_file(&s->apps[s->app_count], full) == 0)
            s->app_count++;
    }
    closedir(d);
}

int gnome_app_scan(GnomeState *s) {
    if (!s) return -1;
    s->app_count = 0;
    /* System applications. */
    gnome_scan_app_dir(s, "/usr/share/applications");
    gnome_scan_app_dir(s, "/usr/local/share/applications");
    /* User applications. */
    if (s->user_data_dir[0]) {
        char dir[GNOME_MAX_PATH_LEN];
        snprintf(dir, sizeof(dir), "%s/applications", s->user_data_dir);
        gnome_scan_app_dir(s, dir);
    }
    if (s->verbose)
        fprintf(stderr, "gnome: scanned %d application(s)\n", s->app_count);
    return s->app_count;
}

int gnome_app_launch(const GnomeAppEntry *app, char **argv) {
    if (!app || !app->exec[0]) return -1;
    /* Fork and exec via /bin/sh -c to handle the freedesktop Exec syntax.
     * %f/%F/%u/%U/%d/%D placeholders are stripped for simplicity. */
    char cmd[GNOME_MAX_PATH_LEN];
    gs_strncpy(cmd, app->exec, sizeof(cmd));
    for (char *p = cmd; *p; p++) {
        if (*p == '%') {
            /* Remove the field code (single char + optional extra). */
            *p = ' ';
            if (p[1]) p[1] = ' ';
        }
    }
    /* If a terminal app, wrap with the configured terminal. */
    char final[GNOME_MAX_PATH_LEN + 256];
    if (app->terminal) {
        snprintf(final, sizeof(final), "gnome-terminal -- sh -c '%s; exec sh'", cmd);
    } else {
        gs_strncpy(final, cmd, sizeof(final));
    }

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* Child */
        (void)argv;             /* extra args ignored in skeleton */
        execl("/bin/sh", "sh", "-c", final, (char *)NULL);
        _exit(127);
    }
    /* Parent: do not wait; launcher returns immediately. */
    return 0;
}

/* ===== Search ===== */

static int gnome_strcasestr(const char *hay, const char *needle) {
    if (!hay || !needle) return 0;
    if (!*needle) return 1;
    for (const char *h = hay; *h; h++) {
        const char *hh = h;
        const char *n = needle;
        while (*hh && *n && tolower((unsigned char)*hh) == tolower((unsigned char)*n)) {
            hh++; n++;
        }
        if (!*n) return 1;
    }
    return 0;
}

int gnome_overview_search(GnomeState *s, const char *query,
                          char out_apps[64][512], int *out_count) {
    if (!s || !query || !out_count) return -1;
    int found = 0;
    /* Search applications first (Apps search provider). */
    for (int i = 0; i < s->app_count && found < 64; i++) {
        GnomeAppEntry *a = &s->apps[i];
        if (a->nodisplay || a->hidden) continue;
        if (gnome_strcasestr(a->name, query) ||
            gnome_strcasestr(a->generic_name, query) ||
            gnome_strcasestr(a->comment, query) ||
            gnome_strcasestr(a->keywords, query) ||
            gnome_strcasestr(a->categories, query)) {
            gs_strncpy(out_apps[found], a->id, 512);
            found++;
        }
    }
    /* Then settings (very limited skeleton). */
    if (found < 64 && gnome_strcasestr("settings", query)) {
        gs_strncpy(out_apps[found++], "gnome-settings.desktop", 512);
    }
    *out_count = found;
    return 0;
}

/* ===== Keybindings ===== */

int gnome_keybinding_add(GnomeState *s, const GnomeKeybinding *k) {
    if (!s || !k) return -1;
    /* Replace if same schema+key exists. */
    for (int i = 0; i < s->keybinding_count; i++) {
        if (strcmp(s->keybindings[i].schema, k->schema) == 0 &&
            strcmp(s->keybindings[i].key, k->key) == 0) {
            s->keybindings[i] = *k;
            return 0;
        }
    }
    if (s->keybinding_count >= GNOME_MAX_KEYBINDINGS) return -1;
    s->keybindings[s->keybinding_count++] = *k;
    return 0;
}

int gnome_keybinding_invoke(GnomeState *s, const char *schema, const char *key) {
    if (!s || !schema || !key) return -1;
    for (int i = 0; i < s->keybinding_count; i++) {
        if (strcmp(s->keybindings[i].schema, schema) == 0 &&
            strcmp(s->keybindings[i].key, key) == 0) {
            if (s->debug)
                fprintf(stderr, "gnome: invoke keybinding %s.%s = %s\n",
                        schema, key, s->keybindings[i].value);
            return 0;
        }
    }
    return -1;
}

/* ===== Power / session ===== */

int gnome_logout(GnomeState *s) {
    if (!s) return -1;
    if (s->verbose) fprintf(stderr, "gnome: logout requested\n");
    gnome_end_session(s, 0, 1);
    return 0;
}

int gnome_power_suspend(GnomeState *s) {
    if (!s) return -1;
    if (s->verbose) fprintf(stderr, "gnome: power suspend\n");
    /* Skeleton: would call systemd's suspend target via logind. */
    return system("systemctl suspend 2>/dev/null") == 0 ? 0 : 0;
}

int gnome_power_hibernate(GnomeState *s) {
    if (!s) return -1;
    if (s->verbose) fprintf(stderr, "gnome: power hibernate\n");
    return system("systemctl hibernate 2>/dev/null") == 0 ? 0 : 0;
}

int gnome_power_reboot(GnomeState *s) {
    if (!s) return -1;
    if (s->verbose) fprintf(stderr, "gnome: power reboot\n");
    gnome_end_session(s, 0, 1);
    return system("systemctl reboot 2>/dev/null") == 0 ? 0 : 0;
}

int gnome_power_shutdown(GnomeState *s) {
    if (!s) return -1;
    if (s->verbose) fprintf(stderr, "gnome: power shutdown\n");
    gnome_end_session(s, 0, 1);
    return system("systemctl poweroff 2>/dev/null") == 0 ? 0 : 0;
}

int gnome_lock_screen(GnomeState *s) {
    if (!s) return -1;
    if (s->verbose) fprintf(stderr, "gnome: lock screen\n");
    s->idle_hint = 1;
    /* Skeleton: invoke gnome-screensaver / gdm via loginctl. */
    return system("loginctl lock-session 2>/dev/null") == 0 ? 0 : 0;
}

/* ===== Utilities ===== */

void gnome_print_help(void) {
    printf("%s - KenuxK GNOME desktop session (minimal)\n\n", GNOME_VERSION_STR);
    printf("USAGE: gnome-shell [opts]\n\n");
    printf("SESSION:\n");
    printf("  --wayland               Use the Wayland session (default)\n");
    printf("  --x11 / --xorg          Use the X11 session\n");
    printf("  --classic               GNOME Classic mode\n");
    printf("  --mobile / --phosh      Phosh/mobile session\n");
    printf("  --session=TYPE          wayland|x11|gnome-classic|phosh\n");
    printf("  --mode=MODE             default|classic|minimal preset\n");
    printf("  --preset=MODE           Same as --mode\n");
    printf("DISPLAY:\n");
    printf("  --display=DISPLAY       X11 display\n");
    printf("  --wayland-display=NAME  Wayland display socket name\n");
    printf("  --nested                 Nested mode\n");
    printf("  --headless              No output\n");
    printf("  --unsafe-mode           Allow alt+F2 restart\n");
    printf("EXTENSIONS:\n");
    printf("  --scan-extensions       Refresh the extension list\n");
    printf("  --list-extensions       Print known extensions and exit\n");
    printf("  --enable-extension=UUID  Enable an extension\n");
    printf("  --disable-extension=UUID Disable an extension\n");
    printf("  --disable-extensions    Disable all extensions\n");
    printf("APPLICATIONS / SEARCH:\n");
    printf("  --scan-apps             Refresh the application list\n");
    printf("  --probe-hardware        Re-probe monitors / input\n");
    printf("SETTINGS:\n");
    printf("  --load-gsettings        Load the GSettings cache\n");
    printf("  --save-gsettings        Save the GSettings cache\n");
    printf("DEBUG:\n");
    printf("  -v / --verbose          Increase verbosity\n");
    printf("  -d / --debug            Enable debug output\n");
    printf("  --trace                 Enable trace output\n");
    printf("  --test-mode             Run without blocking\n");
    printf("  --record                Record mode\n");
    printf("  --version               Print version and exit\n");
    printf("  -h / --help             This help\n");
}

void gnome_print_version(void) {
    printf("%s\n", GNOME_VERSION_STR);
    printf("Session types: wayland (default), x11, gnome-classic, phosh\n");
    printf("KenuxK desktop session wrapper; delegates to native gnome-shell / mutter.\n");
}

#ifndef KENUXK_NO_MAIN_GNOME
int main(int argc, char **argv) {
    /* GnomeState is large; keep it in static storage to avoid stack overflow. */
    static GnomeState s;
    gnome_init(&s);

    int rc = gnome_parse_arguments(&s, argc, argv);
    if (rc != 0) return (rc > 0) ? 0 : rc;   /* --help/--version short-circuit */

    if (gnome_start_session(&s) != 0) {
        fprintf(stderr, "gnome: failed to start session\n");
        return 1;
    }
    rc = gnome_run_session_loop(&s);
    gnome_end_session(&s, rc, 1);
    return rc;
}
#endif
