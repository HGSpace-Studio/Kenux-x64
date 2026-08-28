#include "kanvas_desktop.h"
#include "kanvas_window.h"
#include "kanvas_taskbar.h"
#include "kanvas_start_menu.h"
#include "kanvas_animator.h"
#include "kanvas_boot_splash.h"
#include "kanvas_system_tray.h"
#include "kanvas_kex_runner.h"
#include "kanvas_file_manager.h"
#include "kanvas_task_manager.h"
#include "kanvas_terminal.h"
#include "kapi.h"
#include "kapi_curvo_icons.h"
#include <string.h>

static kanvas_desktop_t* g_desktop = NULL;
static kanvas_boot_splash_t* g_splash = NULL;
static kanvas_system_tray_t* g_sys_tray = NULL;
static kanvas_kex_runner_t* g_kex_runner = NULL;
static kanvas_file_manager_t* g_file_mgr = NULL;
static kanvas_task_manager_t* g_task_mgr = NULL;
static kanvas_terminal_t* g_terminal = NULL;

int kanvas_gui_init(int screen_w, int screen_h, uint32_t* fb, int stride)
{
    if (g_desktop) return -1;

    kanvas_animator_init();
    kapi_curvo_init("/kenux/curvo-CN");

    g_splash = kanvas_boot_splash_create(screen_w, screen_h);
    g_sys_tray = kanvas_system_tray_create();
    g_kex_runner = kanvas_kex_runner_create();
    g_file_mgr = kanvas_file_manager_create();
    g_task_mgr = kanvas_task_manager_create();
    g_terminal = kanvas_terminal_create();

    g_desktop = kanvas_desktop_create(screen_w, screen_h, fb, stride);
    if (!g_desktop) return -1;

    int icon_y_start = KANVAS_TASKBAR_HEIGHT + 16;
    kanvas_desktop_add_icon(g_desktop, "Files", 1, 32, icon_y_start);
    kanvas_desktop_add_icon(g_desktop, "Terminal", 2, 32, icon_y_start + 80);
    kanvas_desktop_add_icon(g_desktop, "Settings", 3, 32, icon_y_start + 160);
    kanvas_desktop_add_icon(g_desktop, "Browser", 4, 32, icon_y_start + 240);
    kanvas_desktop_add_icon(g_desktop, "Editor", 5, 32, icon_y_start + 320);
    kanvas_desktop_add_icon(g_desktop, "Calculator", 6, 96, icon_y_start);
    kanvas_desktop_add_icon(g_desktop, "Image Viewer", 7, 96, icon_y_start + 80);
    kanvas_desktop_add_icon(g_desktop, "Music", 8, 96, icon_y_start + 160);
    kanvas_desktop_add_icon(g_desktop, "Kex Runner", 9, 96, icon_y_start + 240);
    kanvas_desktop_add_icon(g_desktop, "Task Manager", 10, 96, icon_y_start + 320);

    kanvas_start_menu_add_item(g_desktop->start_menu, "Files", "/kex/files", 1, KANVAS_START_SECTION_PINNED, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Terminal", "/kex/terminal", 2, KANVAS_START_SECTION_PINNED, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Browser", "/kex/browser", 4, KANVAS_START_SECTION_PINNED, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Editor", "/kex/editor", 5, KANVAS_START_SECTION_PINNED, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Kex Runner", "/kex/kex-runner", 9, KANVAS_START_SECTION_PINNED, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Settings", "/kex/settings", 3, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Calculator", "/kex/calc", 6, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Image Viewer", "/kex/imgview", 7, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Music Player", "/kex/music", 8, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "System Monitor", "/kex/sysmon", 10, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Task Manager", "/kex/taskmgr", 10, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Package Manager", "/kex/pkgman", 11, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Kex Installer", "/kex/kex-install", 9, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "File Manager", "/kex/filemgr", 1, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Text Editor", "/kex/textedit", 5, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Calendar", "/kex/calendar", 12, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Network Tools", "/kex/nettools", 13, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Compatibility", "/kex/compat", 14, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "Display Settings", "/kex/display", 15, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_add_item(g_desktop->start_menu, "About Kenux", "/kex/about", 16, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_filter(g_desktop->start_menu);

    kanvas_taskbar_add_item(g_desktop->taskbar, "Files", 1, true, NULL);
    kanvas_taskbar_add_item(g_desktop->taskbar, "Terminal", 2, true, NULL);
    kanvas_taskbar_add_item(g_desktop->taskbar, "Browser", 4, true, NULL);
    kanvas_taskbar_add_item(g_desktop->taskbar, "Editor", 5, true, NULL);
    kanvas_taskbar_add_item(g_desktop->taskbar, "Kex", 9, true, NULL);

    kanvas_desktop_tray_add(g_desktop, "Network", NULL);
    kanvas_desktop_tray_add(g_desktop, "Bluetooth", NULL);
    kanvas_desktop_tray_add(g_desktop, "Volume", NULL);
    kanvas_desktop_tray_add(g_desktop, "Brightness", NULL);
    kanvas_desktop_tray_add(g_desktop, "Battery", NULL);

    kanvas_desktop_apply_theme_md3_dark(g_desktop);
    return 0;
}

void kanvas_gui_shutdown(void)
{
    if (g_kex_runner) { kanvas_kex_runner_destroy(g_kex_runner); g_kex_runner = NULL; }
    if (g_file_mgr) { kanvas_file_manager_destroy(g_file_mgr); g_file_mgr = NULL; }
    if (g_task_mgr) { kanvas_task_manager_destroy(g_task_mgr); g_task_mgr = NULL; }
    if (g_terminal) { kanvas_terminal_destroy(g_terminal); g_terminal = NULL; }
    if (g_sys_tray) { kanvas_system_tray_destroy(g_sys_tray); g_sys_tray = NULL; }
    if (g_splash) { kanvas_boot_splash_destroy(g_splash); g_splash = NULL; }
    if (g_desktop) {
        kanvas_desktop_destroy(g_desktop);
        g_desktop = NULL;
    }
    kanvas_animator_shutdown();
    kapi_curvo_shutdown();
}

void kanvas_gui_process_mouse(int x, int y, bool left, bool right, bool mid)
{
    if (!g_desktop) return;
    kanvas_desktop_handle_mouse(g_desktop, x, y, left, right, mid);
}

void kanvas_gui_process_key(int key, bool down, uint32_t mods)
{
    if (!g_desktop) return;
    kanvas_desktop_handle_key(g_desktop, key, down, mods);
}

void kanvas_gui_process_char(uint32_t ch)
{
    if (!g_desktop) return;
    if (g_desktop->start_menu->visible && g_desktop->start_menu->search_focused) {
        kanvas_start_menu_handle_char(g_desktop->start_menu, ch);
    }
}

void kanvas_gui_process_scroll(int delta)
{
    if (!g_desktop) return;
    kanvas_desktop_handle_scroll(g_desktop, delta);
}

void kanvas_gui_update(uint64_t now_ms)
{
    if (!g_desktop) return;
    kanvas_animator_update(now_ms);
    if (g_splash && g_splash->active) kanvas_boot_splash_update(g_splash, now_ms);
    if (g_sys_tray) kanvas_system_tray_update(g_sys_tray, now_ms);
    if (g_kex_runner) kanvas_kex_runner_update(g_kex_runner, now_ms);
    if (g_task_mgr) kanvas_task_manager_update(g_task_mgr, now_ms);
    if (g_terminal) kanvas_terminal_update(g_terminal, now_ms);
    kanvas_desktop_update(g_desktop, now_ms);
}

void kanvas_gui_paint(void)
{
    if (!g_desktop) return;
    uint32_t* fb = NULL;
    int stride = g_desktop->screen_width * 4;
    if (g_splash && g_splash->active) {
        kanvas_boot_splash_paint(g_splash, fb, stride, g_desktop->screen_width, g_desktop->screen_height);
        return;
    }
    kanvas_desktop_paint(g_desktop);
    if (g_file_mgr && g_file_mgr->visible) kanvas_file_manager_paint(g_file_mgr, fb, stride, g_desktop->screen_width, g_desktop->screen_height);
    if (g_task_mgr && g_task_mgr->visible) kanvas_task_manager_paint(g_task_mgr, fb, stride, g_desktop->screen_width, g_desktop->screen_height);
    if (g_terminal && g_terminal->visible) kanvas_terminal_paint(g_terminal, fb, stride, g_desktop->screen_width, g_desktop->screen_height);
    if (g_sys_tray && g_sys_tray->popup_visible) kanvas_system_tray_paint_popup(g_sys_tray, fb, stride, g_desktop->screen_width, g_desktop->screen_height);
}

void kanvas_gui_set_theme(int theme_id)
{
    if (!g_desktop) return;
    switch (theme_id) {
    case 0: kanvas_desktop_apply_theme_md3_dark(g_desktop); break;
    case 1: kanvas_desktop_apply_theme_md3_light(g_desktop); break;
    case 2: kanvas_desktop_apply_theme_classic(g_desktop); break;
    default: kanvas_desktop_apply_theme_md3_dark(g_desktop); break;
    }
}

kanvas_desktop_t* kanvas_gui_get_desktop(void)
{
    return g_desktop;
}

kanvas_boot_splash_t* kanvas_gui_get_splash(void)
{
    return g_splash;
}

kanvas_system_tray_t* kanvas_gui_get_system_tray(void)
{
    return g_sys_tray;
}

kanvas_kex_runner_t* kanvas_gui_get_kex_runner(void)
{
    return g_kex_runner;
}

kanvas_file_manager_t* kanvas_gui_get_file_manager(void)
{
    return g_file_mgr;
}

kanvas_task_manager_t* kanvas_gui_get_task_manager(void)
{
    return g_task_mgr;
}

kanvas_terminal_t* kanvas_gui_get_terminal(void)
{
    return g_terminal;
}

void kanvas_gui_skip_splash(void)
{
    if (g_splash) kanvas_boot_splash_skip(g_splash);
}

bool kanvas_gui_is_splash_active(void)
{
    return g_splash && g_splash->active;
}