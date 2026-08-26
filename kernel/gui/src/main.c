#include "types.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "font_data.h"
#include "window.h"
#include "widget.h"
#include "window_manager.h"
#include "desktop.h"
#include "icon.h"
#include "msf.h"
#include "kenux_render.h"
#include "taskmgr.h"
#include "thispc.h"
#include "sysinfo.h"
#include "filemgr.h"
#include "terminal.h"
#include "compat_center.h"
#include <arch/keyboard.h>
#include <arch/mouse.h>
#include <arch/acpi_pm.h>
#include <timer.h>

/* Quiet by default: GUI trace points stay available without noisy serial logs. */
static inline void gui_serial_putc(char c) {
    (void)c;
}

static widget_t* label_status;
static widget_t* progress_bar;
static widget_t* textbox_input;
static widget_t* info_label;
static uint32_t click_count = 0;
static bool test_pattern_active = false;  /* 颜色测试界面激活标志 */
static window_t* main_win = NULL;
static window_t* info_win = NULL;
static window_t* about_win = NULL;
static window_t* personalize_win = NULL;
static window_t* color_gallery_win = NULL;
static window_t* filemgr_win_ptr = NULL;
static window_t* terminal_win_ptr = NULL;
static widget_t* theme_label;

static void str_copy_simple(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void int_to_string(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int32_t i = 0;
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    int32_t j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

static int32_t iabs32(int32_t v) {
    return v < 0 ? -v : v;
}

static int32_t clamp32(int32_t v, int32_t min, int32_t max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static int32_t mouse_filter_delta(int32_t d) {
    int32_t a = iabs32(d);
    int32_t sign = d < 0 ? -1 : 1;
    int32_t out = a;

    if (a <= 1) {
        out = a;
    } else if (a <= 24) {
        out = a;
    } else if (a <= 48) {
        out = a + (a / 4);
    } else {
        out = 64;
    }
    return sign * out;
}

static void str_cat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void button_ok_click(widget_t* wgt, void* user_data) {
    click_count++;
    char buf[64];
    str_copy_simple(buf, "Clicks: ");
    char num[16];
    int_to_string(click_count, num);
    str_cat(buf, num);
    widget_set_text(label_status, buf);

    if (progress_bar) {
        progressbar_data_t* data = (progressbar_data_t*)progress_bar->user_data;
        uint32_t new_val = data->value + 10;
        if (new_val > data->max) new_val = data->min;
        widget_set_progress(progress_bar, new_val);
    }
}

static void button_clear_click(widget_t* wgt, void* user_data) {
    click_count = 0;
    widget_set_text(label_status, "Clicks: 0");
    if (progress_bar) {
        widget_set_progress(progress_bar, 0);
    }
}

static void button_about_click(widget_t* wgt, void* user_data) {
    if (about_win) {
        about_win->visible = true;
        about_win->state.minimized = false;
        wm_set_active(about_win);
    }
}

static void menu_open_info(void) {
    if (main_win) {
        main_win->visible = true;
        main_win->state.minimized = false;
        wm_set_active(main_win);
    }
}

static void menu_open_main(void) {
    if (main_win) {
        main_win->visible = true;
        main_win->state.minimized = false;
        wm_set_active(main_win);
    }
}

static void menu_open_taskmgr(void) {
    window_t* win = taskmgr_create();
    if (!win) return;
    win->visible = true;
    win->state.minimized = false;
    wm_set_active(win);
    taskmgr_refresh();
    wm_invalidate_window(win);
    wm_flush_dirty();
}

static void menu_open_thispc(void) {
    window_t* win = thispc_create();
    if (!win) return;
    win->visible = true;
    win->state.minimized = false;
    wm_set_active(win);
    thispc_on_show();
    wm_invalidate_window(win);
    wm_flush_dirty();
}

static void menu_open_sysinfo(void) {
    window_t* win = sysinfo_create();
    if (!win) return;
    win->visible = true;
    win->state.minimized = false;
    wm_set_active(win);
    sysinfo_on_show();
    wm_invalidate_window(win);
    wm_flush_dirty();
}

static void menu_open_filemgr(void) {
    filemgr_win_ptr = filemgr_create();
    if (!filemgr_win_ptr) return;
    filemgr_win_ptr->visible = true;
    filemgr_win_ptr->state.minimized = false;
    wm_set_active(filemgr_win_ptr);
    filemgr_on_show();
    wm_invalidate_window(filemgr_win_ptr);
    wm_flush_dirty();
}

static void menu_open_terminal(void) {
    terminal_win_ptr = terminal_create();
    if (!terminal_win_ptr) return;
    terminal_win_ptr->visible = true;
    terminal_win_ptr->state.minimized = false;
    wm_set_active(terminal_win_ptr);
    terminal_on_show();
    wm_invalidate_window(terminal_win_ptr);
    wm_flush_dirty();
}

static void menu_open_compat(void) {
    window_t* win = compat_center_create();
    if (!win) return;
    win->visible = true;
    win->state.minimized = false;
    wm_set_active(win);
    compat_center_on_show();
    wm_invalidate_window(win);
    wm_flush_dirty();
}

static void intro_thispc_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    menu_open_thispc();
}

static void intro_explorer_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    menu_open_filemgr();
}

static void intro_terminal_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    menu_open_terminal();
}

static void intro_sysinfo_click(widget_t* wgt, void* user_data) {
    (void)wgt; (void)user_data;
    menu_open_sysinfo();
}

static void menu_refresh(void) {
    click_count = 0;
    widget_set_text(label_status, "Refreshed!");
    if (progress_bar) {
        widget_set_progress(progress_bar, 50);
    }
}

static void menu_power_sleep(void) {
    int ret = acpi_pm_suspend();
    if (ret != ACPI_PM_OK) {
        ret = acpi_pm_hibernate();
    }
    if (ret != ACPI_PM_OK && label_status) {
        widget_set_text(label_status, "Sleep is not supported on this machine");
    }
}

static void menu_power_restart(void) {
    acpi_pm_reset_system();
}

static void menu_power_shutdown(void) {
    acpi_pm_shutdown();
}

static void apply_theme_to_windows(void) {
    if (main_win) {
        main_win->bg_color = msf_settings.window_bg;
        main_win->titlebar_color = msf_settings.titlebar_active;
        main_win->title_text_color = msf_settings.title_text;
    }
    if (info_win) {
        info_win->bg_color = msf_settings.window_bg;
        info_win->titlebar_color = msf_settings.titlebar_active;
        info_win->title_text_color = msf_settings.title_text;
    }
    if (about_win) {
        about_win->bg_color = msf_settings.window_bg;
        about_win->titlebar_color = msf_settings.titlebar_active;
        about_win->title_text_color = msf_settings.title_text;
    }
    if (personalize_win) {
        personalize_win->bg_color = msf_settings.window_bg;
        personalize_win->titlebar_color = msf_settings.titlebar_active;
        personalize_win->title_text_color = msf_settings.title_text;
    }
}

static void theme_light_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_LIGHT);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Light");
}

static void theme_dark_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_DARK);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Dark");
}

static void theme_blue_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_BLUE);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Blue");
}

static void theme_green_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_GREEN);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Green");
}

static void theme_sunset_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_SUNSET);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Sunset");
}

static void theme_ocean_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_OCEAN);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Ocean");
}

static void theme_forest_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_FOREST);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Forest");
}

static void theme_aurora_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_AURORA);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Aurora");
}

static void theme_rose_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_ROSE);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Rose");
}

static void theme_gold_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_GOLD);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Gold");
}

static void theme_midnight_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_MIDNIGHT);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Midnight");
}

static void theme_lavender_click(widget_t* wgt, void* user_data) {
    msf_set_theme(THEME_LAVENDER);
    apply_theme_to_windows();
    if (theme_label) widget_set_text(theme_label, "Theme: Lavender");
}

static void accent_blue_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0x00, 0x78, 0xD7));
    apply_theme_to_windows();
}

static void accent_red_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0xE5, 0x39, 0x35));
    apply_theme_to_windows();
}

static void accent_green_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0x2E, 0x7D, 0x32));
    apply_theme_to_windows();
}

static void accent_purple_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0x8E, 0x24, 0xAA));
    apply_theme_to_windows();
}

static void accent_orange_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0xFB, 0x8C, 0x00));
    apply_theme_to_windows();
}

static void accent_pink_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0xD8, 0x1B, 0x60));
    apply_theme_to_windows();
}

static void accent_teal_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0x00, 0x96, 0x88));
    apply_theme_to_windows();
}

static void accent_indigo_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0x3F, 0x51, 0xB5));
    apply_theme_to_windows();
}

static void accent_rose_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0xF4, 0x3F, 0x5E));
    apply_theme_to_windows();
}

static void accent_amber_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0xF5, 0x9E, 0x0B));
    apply_theme_to_windows();
}

static void accent_emerald_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0x10, 0xB9, 0x81));
    apply_theme_to_windows();
}

static void accent_violet_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0x8B, 0x5C, 0xF6));
    apply_theme_to_windows();
}

static void accent_fuchsia_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0xD9, 0x46, 0xEF));
    apply_theme_to_windows();
}

static void accent_sky_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0x0E, 0xA5, 0xE9));
    apply_theme_to_windows();
}

static void accent_lime_click(widget_t* wgt, void* user_data) {
    msf_set_accent(RGB(0x84, 0xCC, 0x16));
    apply_theme_to_windows();
}

static void wall_default_click(widget_t* wgt, void* user_data) {
    msf_set_wallpaper(RGB(0x6C, 0x5C, 0xE7), RGB(0x00, 0x96, 0xC7));
}

static void wall_sunset_click(widget_t* wgt, void* user_data) {
    msf_set_wallpaper(RGB(0xFF, 0x6B, 0x35), RGB(0x4A, 0x1A, 0x5E));
}

static void wall_forest_click(widget_t* wgt, void* user_data) {
    msf_set_wallpaper(RGB(0x1B, 0x5E, 0x20), RGB(0x05, 0x1E, 0x0A));
}

static void wall_night_click(widget_t* wgt, void* user_data) {
    msf_set_wallpaper(RGB(0x1A, 0x1A, 0x2E), RGB(0x0A, 0x0A, 0x1A));
}

static void wall_ocean_click(widget_t* wgt, void* user_data) {
    msf_set_wallpaper(RGB(0x00, 0xB4, 0xD8), RGB(0x00, 0x1E, 0x3C));
}

static void wall_aurora_click(widget_t* wgt, void* user_data) {
    msf_set_wallpaper(RGB(0x00, 0xD4, 0xAA), RGB(0x6C, 0x5C, 0xE7));
}

static void wall_rose_click(widget_t* wgt, void* user_data) {
    msf_set_wallpaper(RGB(0xFF, 0x37, 0x85), RGB(0x4A, 0x14, 0x4E));
}

static void wall_gold_click(widget_t* wgt, void* user_data) {
    msf_set_wallpaper(RGB(0xFF, 0xD6, 0x0A), RGB(0xE6, 0x5C, 0x00));
}

static void menu_personalize(void) {
    if (personalize_win) {
        personalize_win->visible = true;
        personalize_win->state.minimized = false;
        wm_set_active(personalize_win);
    }
}

static void menu_color_test(void) {
    /* Color test still available from start menu, but not auto-shown */
    fb_test_pattern();
    msleep(2000);
    wm_paint();
}

static void setup_desktop(void) {
    /* Windows-style clean desktop: taskbar plus application shortcuts only. */
    desktop_add_icon_with_callback(104, 42, "This PC", ICON_显示器, menu_open_thispc);
    desktop_add_icon_with_callback(104, 116, "Explorer", ICON_FOLDER, menu_open_filemgr);
    desktop_add_icon_with_callback(104, 190, "Terminal", ICON_代码, menu_open_terminal);
    desktop_add_icon_with_callback(104, 264, "Task Mgr", ICON_进度条, menu_open_taskmgr);
    desktop_add_icon_with_callback(104, 338, "Sys Info", ICON_INFO, menu_open_sysinfo);
    desktop_add_icon_with_callback(104, 412, "Compat", ICON_开关开启, menu_open_compat);
    desktop_add_icon_with_callback(104, 486, "KenuxOS", ICON_APP, menu_open_info);
    desktop_show_icons(true);

    /* === Pinned dock apps (left-side vertical taskbar) === */
    dock_add_app(ICON_显示器, "This PC", menu_open_thispc);
    dock_add_app(ICON_FOLDER, "Kenux Explorer", menu_open_filemgr);
    dock_add_app(ICON_代码, "Terminal", menu_open_terminal);
    dock_add_app(ICON_进度条, "Task Manager", menu_open_taskmgr);
    dock_add_app(ICON_INFO, "System Info", menu_open_sysinfo);
    dock_add_app(ICON_开关开启, "Compatibility Center", menu_open_compat);

    /* === Three-column start menu === */
    /* Column 0: Common apps */
    start_menu_add_item("This PC", ICON_显示器, menu_open_thispc, 0);
    start_menu_add_item("Kenux Explorer", ICON_FOLDER, menu_open_filemgr, 0);
    start_menu_add_item("Kenux Terminal", ICON_代码, menu_open_terminal, 0);
    start_menu_add_item("KenuxOS Intro", ICON_APP, menu_open_info, 0);

    /* Column 1: System apps (card style) */
    start_menu_add_item("Task Manager", ICON_进度条, menu_open_taskmgr, 1);
    start_menu_add_item("System Info", ICON_INFO, menu_open_sysinfo, 1);
    start_menu_add_item("Compatibility", ICON_开关开启, menu_open_compat, 1);
    start_menu_add_item("Personalize", ICON_SETTINGS, menu_personalize, 1);
    start_menu_add_item("Color Test", ICON_COLOR, menu_color_test, 1);

    /* Column 2: Power options + user */
    start_menu_add_item("Power", ICON_POWER, NULL, 2);
    start_menu_add_item("", ICON_NONE, NULL, 2);  /* separator */
    start_menu_add_item("Sleep", ICON_CLOCK, menu_power_sleep, 2);
    start_menu_add_item("Restart", ICON_REFRESH, menu_power_restart, 2);
    start_menu_add_item("Shut Down", ICON_POWER, menu_power_shutdown, 2);
}

static void setup_context_menu(void) {
    desktop.context_menu.item_count = 0;
    context_menu_add_item("Refresh", ICON_REFRESH, menu_refresh, false);
    context_menu_add_item("New Folder", ICON_FOLDER, NULL, false);
    context_menu_add_item("", ICON_NONE, NULL, true);
    context_menu_add_item("Personalize", ICON_SETTINGS, menu_personalize, false);
    context_menu_add_item("Properties", ICON_SETTINGS, NULL, false);
    context_menu_add_item("View", ICON_GRID, NULL, false);
    context_menu_add_item("Sort by", ICON_LIST, NULL, false);
}

static void demo_main(void) {
    msf_apply();

    wm_init(msf_settings.wallpaper_color1);
    setup_desktop();
    setup_context_menu();

    /* Pre-create windows but keep them hidden — clean desktop on boot.
     * Windows are opened on-demand via dock clicks or start menu. */

    main_win = window_create(100, 50, 440, 300, "KenuxOS Intro");
    main_win->titlebar_color = msf_settings.titlebar_active;
    main_win->visible = false;  /* hidden on boot */
    window_set_statusbar(main_win, "Start menu, apps, power and system tools are ready");

    widget_t* label1 = widget_create_label(10, 10, 300, 16,
        "Welcome to KenuxOS", msf_settings.accent_color);
    window_add_widget(main_win, label1);

    label_status = widget_create_label(10, 30, 390, 16,
        "Use Start for apps, restart and shut down.", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(main_win, label_status);

    widget_t* intro_line1 = widget_create_label(10, 56, 390, 16,
        "Available real apps:", msf_settings.font_color);
    window_add_widget(main_win, intro_line1);

    widget_t* btn_pc = widget_create_button(10, 82, 120, 30,
        "This PC", intro_thispc_click, NULL);
    window_add_widget(main_win, btn_pc);

    widget_t* btn_explorer = widget_create_button(142, 82, 130, 30,
        "Explorer", intro_explorer_click, NULL);
    window_add_widget(main_win, btn_explorer);

    widget_t* btn_terminal = widget_create_button(284, 82, 120, 30,
        "Terminal", intro_terminal_click, NULL);
    window_add_widget(main_win, btn_terminal);

    widget_t* btn_sysinfo = widget_create_button(10, 122, 120, 30,
        "System Info", intro_sysinfo_click, NULL);
    window_add_widget(main_win, btn_sysinfo);

    widget_t* btn_about = widget_create_button(142, 122, 130, 30,
        "About", button_about_click, NULL);
    window_add_widget(main_win, btn_about);

    widget_t* btn_ready = widget_create_button(284, 122, 120, 30,
        "Status", button_ok_click, NULL);
    window_add_widget(main_win, btn_ready);

    widget_t* intro_line2 = widget_create_label(10, 166, 390, 16,
        "Layering: Start menu is always above windows.", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(main_win, intro_line2);

    widget_t* intro_line3 = widget_create_label(10, 188, 390, 16,
        "Theme: Win10-like borders + curvo-CN icons.", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(main_win, intro_line3);

    progress_bar = widget_create_progressbar(10, 218, 394, 16, 0, 100, 70);
    window_add_widget(main_win, progress_bar);

    wm_add_window(main_win);

    info_win = window_create(520, 60, 280, 180, "System Info");
    info_win->titlebar_color = msf_settings.titlebar_active;
    info_win->visible = false;  /* hidden on boot */

    char res_buf[64];
    str_copy_simple(res_buf, "Resolution: ");
    char num[16];
    int_to_string(fb.width, num);
    str_cat(res_buf, num);
    str_cat(res_buf, "x");
    int_to_string(fb.height, num);
    str_cat(res_buf, num);

    widget_t* info1 = widget_create_label(10, 10, 250, 16,
        "Kenux Kernel GUI", msf_settings.accent_color);
    window_add_widget(info_win, info1);

    info_label = widget_create_label(10, 30, 250, 16, res_buf, msf_settings.font_color);
    window_add_widget(info_win, info_label);

    widget_t* info2 = widget_create_label(10, 50, 250, 16,
        "32-bit True Color (GOP)", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(info_win, info2);

    widget_t* info3 = widget_create_label(10, 70, 250, 16,
        "Font: 8x16 Bitmap", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(info_win, info3);

    widget_t* info4 = widget_create_label(10, 90, 250, 16,
        "Features:", msf_settings.font_color);
    window_add_widget(info_win, info4);

    widget_t* info5 = widget_create_label(10, 108, 250, 16,
        "  - Taskbar & Start Menu", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(info_win, info5);

    widget_t* info6 = widget_create_label(10, 124, 250, 16,
        "  - Desktop & Context Menu", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(info_win, info6);

    widget_t* info7 = widget_create_label(10, 140, 250, 16,
        "  - Window Controls", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(info_win, info7);

    window_set_statusbar(info_win, "System Information");
    wm_add_window(info_win);

    about_win = window_create(200, 100, 320, 160, "About");
    about_win->titlebar_color = msf_settings.titlebar_active;
    about_win->visible = false;

    widget_t* about_title = widget_create_label(10, 10, 280, 16,
        "Kenux GUI", msf_settings.accent_color);
    window_add_widget(about_win, about_title);

    widget_t* about_ver = widget_create_label(10, 30, 280, 16,
        "Version 4.0 - Full Color", msf_settings.font_color);
    window_add_widget(about_win, about_ver);

    widget_t* about_desc = widget_create_label(10, 50, 280, 16,
        "A complete GUI system running on", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(about_win, about_desc);

    widget_t* about_desc2 = widget_create_label(10, 66, 280, 16,
        "Kenux kernel via UEFI GOP.", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(about_win, about_desc2);

    widget_t* about_feat = widget_create_label(10, 90, 280, 16,
        "12 themes, 19 accent colors,", msf_settings.accent_color);
    window_add_widget(about_win, about_feat);

    widget_t* about_feat2 = widget_create_label(10, 106, 280, 16,
        "400+ colors, HSL, gradients", msf_settings.accent_color);
    window_add_widget(about_win, about_feat2);

    widget_t* about_btn = widget_create_button(110, 128, 80, 24,
        "OK", NULL, NULL);
    window_add_widget(about_win, about_btn);

    wm_add_window(about_win);

    /* Personalization Window - MSF Settings */
    personalize_win = window_create(300, 30, 400, 470, "Personalize");
    personalize_win->titlebar_color = msf_settings.titlebar_active;
    personalize_win->visible = false;
    window_set_statusbar(personalize_win, "MSF Settings");

    widget_t* pers_title = widget_create_label(10, 10, 370, 16,
        "Personalization (MSF)", msf_settings.accent_color);
    window_add_widget(personalize_win, pers_title);

    theme_label = widget_create_label(10, 30, 370, 16, "Theme: Light", msf_settings.font_color);
    window_add_widget(personalize_win, theme_label);

    /* Theme row 1 */
    widget_t* btn_light = widget_create_button(10, 50, 80, 24, "Light", theme_light_click, NULL);
    window_add_widget(personalize_win, btn_light);

    widget_t* btn_dark = widget_create_button(95, 50, 80, 24, "Dark", theme_dark_click, NULL);
    window_add_widget(personalize_win, btn_dark);

    widget_t* btn_blue = widget_create_button(180, 50, 80, 24, "Blue", theme_blue_click, NULL);
    window_add_widget(personalize_win, btn_blue);

    widget_t* btn_green = widget_create_button(265, 50, 80, 24, "Green", theme_green_click, NULL);
    window_add_widget(personalize_win, btn_green);

    /* Theme row 2 */
    widget_t* btn_sunset = widget_create_button(10, 78, 80, 24, "Sunset", theme_sunset_click, NULL);
    window_add_widget(personalize_win, btn_sunset);

    widget_t* btn_ocean = widget_create_button(95, 78, 80, 24, "Ocean", theme_ocean_click, NULL);
    window_add_widget(personalize_win, btn_ocean);

    widget_t* btn_forest = widget_create_button(180, 78, 80, 24, "Forest", theme_forest_click, NULL);
    window_add_widget(personalize_win, btn_forest);

    widget_t* btn_aurora = widget_create_button(265, 78, 80, 24, "Aurora", theme_aurora_click, NULL);
    window_add_widget(personalize_win, btn_aurora);

    /* Theme row 3 */
    widget_t* btn_rose = widget_create_button(10, 106, 80, 24, "Rose", theme_rose_click, NULL);
    window_add_widget(personalize_win, btn_rose);

    widget_t* btn_gold = widget_create_button(95, 106, 80, 24, "Gold", theme_gold_click, NULL);
    window_add_widget(personalize_win, btn_gold);

    widget_t* btn_midnight = widget_create_button(180, 106, 80, 24, "Midnight", theme_midnight_click, NULL);
    window_add_widget(personalize_win, btn_midnight);

    widget_t* btn_lavender = widget_create_button(265, 106, 80, 24, "Lavender", theme_lavender_click, NULL);
    window_add_widget(personalize_win, btn_lavender);

    widget_t* accent_lbl = widget_create_label(10, 141, 370, 16, "Accent Color:", msf_settings.font_color);
    window_add_widget(personalize_win, accent_lbl);

    /* Accent row 1 */
    widget_t* btn_ac_blue = widget_create_button(10, 161, 80, 24, "Blue", accent_blue_click, NULL);
    window_add_widget(personalize_win, btn_ac_blue);

    widget_t* btn_ac_red = widget_create_button(95, 161, 80, 24, "Red", accent_red_click, NULL);
    window_add_widget(personalize_win, btn_ac_red);

    widget_t* btn_ac_green = widget_create_button(180, 161, 80, 24, "Green", accent_green_click, NULL);
    window_add_widget(personalize_win, btn_ac_green);

    widget_t* btn_ac_purple = widget_create_button(265, 161, 80, 24, "Purple", accent_purple_click, NULL);
    window_add_widget(personalize_win, btn_ac_purple);

    /* Accent row 2 */
    widget_t* btn_ac_orange = widget_create_button(10, 189, 80, 24, "Orange", accent_orange_click, NULL);
    window_add_widget(personalize_win, btn_ac_orange);

    widget_t* btn_ac_pink = widget_create_button(95, 189, 80, 24, "Pink", accent_pink_click, NULL);
    window_add_widget(personalize_win, btn_ac_pink);

    widget_t* btn_ac_teal = widget_create_button(180, 189, 80, 24, "Teal", accent_teal_click, NULL);
    window_add_widget(personalize_win, btn_ac_teal);

    widget_t* btn_ac_indigo = widget_create_button(265, 189, 80, 24, "Indigo", accent_indigo_click, NULL);
    window_add_widget(personalize_win, btn_ac_indigo);

    /* Accent row 3 */
    widget_t* btn_ac_rose = widget_create_button(10, 217, 80, 24, "Rose", accent_rose_click, NULL);
    window_add_widget(personalize_win, btn_ac_rose);

    widget_t* btn_ac_amber = widget_create_button(95, 217, 80, 24, "Amber", accent_amber_click, NULL);
    window_add_widget(personalize_win, btn_ac_amber);

    widget_t* btn_ac_emerald = widget_create_button(180, 217, 80, 24, "Emerald", accent_emerald_click, NULL);
    window_add_widget(personalize_win, btn_ac_emerald);

    widget_t* btn_ac_violet = widget_create_button(265, 217, 80, 24, "Violet", accent_violet_click, NULL);
    window_add_widget(personalize_win, btn_ac_violet);

    /* Accent row 4 */
    widget_t* btn_ac_fuchsia = widget_create_button(10, 245, 80, 24, "Fuchsia", accent_fuchsia_click, NULL);
    window_add_widget(personalize_win, btn_ac_fuchsia);

    widget_t* btn_ac_sky = widget_create_button(95, 245, 80, 24, "Sky", accent_sky_click, NULL);
    window_add_widget(personalize_win, btn_ac_sky);

    widget_t* btn_ac_lime = widget_create_button(180, 245, 80, 24, "Lime", accent_lime_click, NULL);
    window_add_widget(personalize_win, btn_ac_lime);

    widget_t* wall_lbl = widget_create_label(10, 281, 370, 16, "Wallpaper:", msf_settings.font_color);
    window_add_widget(personalize_win, wall_lbl);

    /* Wallpaper row 1 */
    widget_t* btn_wall_def = widget_create_button(10, 301, 80, 24, "Default", wall_default_click, NULL);
    window_add_widget(personalize_win, btn_wall_def);

    widget_t* btn_wall_sun = widget_create_button(95, 301, 80, 24, "Sunset", wall_sunset_click, NULL);
    window_add_widget(personalize_win, btn_wall_sun);

    widget_t* btn_wall_for = widget_create_button(180, 301, 80, 24, "Forest", wall_forest_click, NULL);
    window_add_widget(personalize_win, btn_wall_for);

    widget_t* btn_wall_night = widget_create_button(265, 301, 80, 24, "Night", wall_night_click, NULL);
    window_add_widget(personalize_win, btn_wall_night);

    /* Wallpaper row 2 */
    widget_t* btn_wall_ocean = widget_create_button(10, 329, 80, 24, "Ocean", wall_ocean_click, NULL);
    window_add_widget(personalize_win, btn_wall_ocean);

    widget_t* btn_wall_aurora = widget_create_button(95, 329, 80, 24, "Aurora", wall_aurora_click, NULL);
    window_add_widget(personalize_win, btn_wall_aurora);

    widget_t* btn_wall_rose = widget_create_button(180, 329, 80, 24, "Rose", wall_rose_click, NULL);
    window_add_widget(personalize_win, btn_wall_rose);

    widget_t* btn_wall_gold = widget_create_button(265, 329, 80, 24, "Gold", wall_gold_click, NULL);
    window_add_widget(personalize_win, btn_wall_gold);

    widget_t* msf_info = widget_create_label(10, 368, 370, 16,
        "Settings: settings.msf", msf_settings.font_color);
    window_add_widget(personalize_win, msf_info);

    widget_t* color_info = widget_create_label(10, 386, 370, 16,
        "Color: 32-bit true color (16M colors)", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(personalize_win, color_info);

    widget_t* color_info2 = widget_create_label(10, 402, 370, 16,
        "400+ named colors, HSL + gradients", RGB(0xAE, 0xAE, 0xB0));
    window_add_widget(personalize_win, color_info2);

    widget_t* btn_close = widget_create_button(150, 426, 80, 24, "Close", NULL, NULL);
    window_add_widget(personalize_win, btn_close);

    wm_add_window(personalize_win);

    wm_paint();
}

/* 简单延时 — 使用内核 timer.h 的 msleep */
static void gui_sleep_ms(uint32_t ms) {
    msleep((uint64_t)ms);
}

/* GUI 主入口：由 kernel.c 调用 */
void gui_run(void) {
    gui_serial_putc('1');
    /* 从 KenuxK framebuffer 初始化 GUI framebuffer */
    gui_fb_init();
    gui_serial_putc('2');
    if (fb.base == NULL) { gui_serial_putc('N'); return; }
    gui_serial_putc('3');

    /* 加载默认主题 */
    msf_load_defaults();
    gui_serial_putc('4');
    msf_apply();
    gui_serial_putc('5');

    /* 清屏 */
    fb_clear(msf_settings.wallpaper_color1);
    gui_serial_putc('6');
    msleep(50);  /* let framebuffer settle */
    gui_serial_putc('7');

    /* 逐步搭建桌面 — 避免一下子初始化全部组件 */
    demo_main();
    gui_serial_putc('8');

    /* 首次渲染 — 确保窗口和桌面立即可见，不需要等输入 */
    wm_paint();
    gui_serial_putc('9');

    /* No auto test pattern — show clean desktop */
    gui_serial_putc('A');
    msleep(500);
    gui_serial_putc('B');

#ifdef GUI_SHOWCASE_MODE
    static bool showcase_opened = false;
    static uint64_t showcase_next_ms = 0;
    static void (*showcase_openers[])(void) = {
        menu_open_thispc, menu_open_filemgr, menu_open_terminal,
        menu_open_taskmgr, menu_open_sysinfo, menu_open_compat,
        menu_open_info, menu_personalize, menu_color_test
    };
    const uint32_t showcase_count = sizeof(showcase_openers) / sizeof(showcase_openers[0]);
    showcase_next_ms = timer_jiffies_to_ms(timer_get_jiffies()) + 400;
#endif

    /* 事件循环（使用内核键盘/鼠标） */
    static uint8_t prev_left = 0;
    static uint8_t prev_right = 0;
    uint64_t last_app_update_ms = timer_jiffies_to_ms(timer_get_jiffies());
    uint64_t last_terminal_tick_ms = last_app_update_ms;
    uint64_t last_clock_update_ms = last_app_update_ms;
    uint64_t desktop_clock_base_min = 12 * 60;

    for (;;) {
        bool app_content_refresh = false;
        /* 键盘输入 */
        key_event_t kbd_ev;
        if (keyboard_poll(&kbd_ev)) {
            if (kbd_ev.pressed) {
                if (kbd_ev.keycode == KEY_ESC) {
                    /* ESC 退出 GUI，回到 shell */
                    break;
                }
                if ((kbd_ev.modifiers & (KEYMOD_LCTRL | KEYMOD_RCTRL)) &&
                    (kbd_ev.modifiers & (KEYMOD_LALT | KEYMOD_RALT)) &&
                    (kbd_ev.keycode == 'c' || kbd_ev.keycode == 'C' ||
                     kbd_ev.ascii == 'c' || kbd_ev.ascii == 'C')) {
                    menu_open_compat();
                    continue;
                }
                /* Win 键切换开始菜单 */
                if (kbd_ev.keycode == KEY_LWIN || kbd_ev.keycode == KEY_RWIN) {
                    desktop.start_menu_open = !desktop.start_menu_open;
                    if (desktop.sm_w > 0 && desktop.sm_h > 0) {
                        wm_invalidate_rect(desktop.sm_x, desktop.sm_y,
                                           desktop.sm_w, desktop.sm_h);
                    }
                    if (desktop.tb_w > 0 && desktop.tb_h > 0) {
                        wm_invalidate_rect(desktop.tb_x, desktop.tb_y,
                                           desktop.tb_w, desktop.tb_h);
                    }
                    wm_flush_dirty();
                    continue;
                }
                /* Route keyboard to terminal when it is the active window */
                if (terminal_win_ptr && wm.active_window == terminal_win_ptr) {
                    terminal_handle_key((uint16_t)kbd_ev.keycode,
                                        (uint16_t)kbd_ev.ascii);
                    app_content_refresh = true;
                } else {
                    wm_handle_key((uint16_t)kbd_ev.keycode,
                                  (uint16_t)kbd_ev.ascii);
                    if (wm.active_window) {
                        wm_invalidate_window(wm.active_window);
                    }
                    wm_flush_dirty();
                }
            }
        }

        /* 鼠标输入 */
        mouse_packet_t mse_pkt;
        if (mouse_poll(&mse_pkt)) {
            int32_t move_x = mse_pkt.dx;
            int32_t move_y = mse_pkt.dy;
            uint8_t buttons = mse_pkt.buttons;

            for (uint32_t drain = 0; drain < 2 && mouse_poll(&mse_pkt); drain++) {
                move_x += mse_pkt.dx;
                move_y += mse_pkt.dy;
                buttons = mse_pkt.buttons;
            }

            move_x = mouse_filter_delta(clamp32(move_x, -64, 64));
            move_y = mouse_filter_delta(clamp32(move_y, -64, 64));

            int32_t new_x = (int32_t)wm.mouse_x + move_x;
            int32_t new_y = (int32_t)wm.mouse_y + move_y;

            if (new_x < 0) new_x = 0;
            if (new_x >= (int32_t)fb.width) new_x = (int32_t)fb.width - 1;
            if (new_y < 0) new_y = 0;
            if (new_y >= (int32_t)fb.height) new_y = (int32_t)fb.height - 1;

            if (move_x != 0 || move_y != 0) {
                win_ctrl_hit_t prev_hover = WIN_CTRL_NONE;
                if (wm.active_window) {
                    prev_hover = wm.active_window->state.hover_ctrl;
                }
                wm_handle_mouse_move((uint32_t)new_x, (uint32_t)new_y);

                if (wm.dragging) {
                    wm_flush_dirty();
                } else if (desktop.icon_dragging) {
                    wm_flush_dirty();
                    wm_paint_cursor_only();
                } else if (wm.active_window && prev_hover != wm.active_window->state.hover_ctrl) {
                    wm_flush_dirty();
                } else {
                    wm_paint_cursor_only();
                }
            }

            uint8_t cur_left = buttons & MOUSE_BTN_LEFT ? 1 : 0;
            uint8_t cur_right = buttons & MOUSE_BTN_RIGHT ? 1 : 0;

            if (cur_left != prev_left) {
                if (cur_left) {
                    wm_handle_mouse_down(0, wm.mouse_x, wm.mouse_y);
                } else {
                    wm_handle_mouse_up(0, wm.mouse_x, wm.mouse_y);
                }
                prev_left = cur_left;
                if (wm.need_full_repaint) {
                    wm.need_full_repaint = false;
                    wm_paint();
                } else {
                    wm_flush_dirty();
                }
            }

            if (cur_right != prev_right) {
                if (cur_right) {
                    wm_handle_mouse_down(1, wm.mouse_x, wm.mouse_y);
                } else {
                    wm_handle_mouse_up(1, wm.mouse_x, wm.mouse_y);
                }
                prev_right = cur_right;
                wm_flush_dirty();
            }
        }

        uint64_t now_ms = timer_jiffies_to_ms(timer_get_jiffies());

#ifdef GUI_SHOWCASE_MODE
        if (!showcase_opened && now_ms >= showcase_next_ms) {
            for (window_t* old = wm.windows; old; old = old->next) {
                old->visible = false;
                old->state.minimized = true;
            }
            uint32_t app = (uint32_t)GUI_SHOWCASE_APP;
            if (app >= showcase_count) app = 0;
            showcase_openers[app]();
            showcase_opened = true;
            wm_paint();
        }
#endif

        /* 低频应用维护：任务管理器等重绘不能跟着 2ms 主循环跑。 */
        if (now_ms - last_app_update_ms >= 250) {
            last_app_update_ms = now_ms;
            taskmgr_update();
        }

        /* 终端光标闪烁只需要约 20Hz 检查。 */
        if (now_ms - last_terminal_tick_ms >= 50) {
            last_terminal_tick_ms = now_ms;
            terminal_tick();
        }
        if (app_content_refresh) {
            /* terminal_handle_key() already redraws the terminal content.
             * Do not repaint the whole desktop here; full redraw on every
             * keystroke was one of the visible lag sources. */
        }

        if (now_ms - last_clock_update_ms >= 1000) {
            last_clock_update_ms = now_ms;
            static uint32_t last_clock_hour = 0xFFFFFFFFu;
            static uint32_t last_clock_minute = 0xFFFFFFFFu;
            uint32_t elapsed_sec = (uint32_t)(now_ms / 1000);
            uint32_t total_min = desktop_clock_base_min + elapsed_sec / 60;
            uint32_t hour = (total_min / 60) % 24;
            uint32_t minute = total_min % 60;
            if (hour != last_clock_hour || minute != last_clock_minute) {
                last_clock_hour = hour;
                last_clock_minute = minute;
                taskbar_update_clock(hour, minute);
                if (desktop.tb_w > 0 && desktop.tb_h > 0) {
                    wm_invalidate_rect(desktop.tb_x, desktop.tb_y,
                                       desktop.tb_w, desktop.tb_h);
                }
            }
        }

        /* Safety net: if damage was added but wm_flush_dirty() wasn't
         * called (e.g., menu toggle inside a callback), flush now so
         * UI changes appear without requiring mouse movement. */
        if (wm.needs_flush) {
            wm_flush_dirty();
        }

        gui_sleep_ms(2);
    }
}
