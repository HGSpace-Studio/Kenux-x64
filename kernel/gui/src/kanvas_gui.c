#include "kanvas_desktop.h"
#include "kanvas_window.h"
#include "kanvas_taskbar.h"
#include "kanvas_start_menu.h"
#include "kapi.h"
#include <string.h>

static kanvas_desktop_t* g_desktop = NULL;

int kanvas_gui_init(int screen_w, int screen_h, uint32_t* fb, int stride)
{
    if (g_desktop) return -1;
    g_desktop = kanvas_desktop_create(screen_w, screen_h, fb, stride);
    if (!g_desktop) return -1;
    kanvas_desktop_add_icon(g_desktop, "Files", 1, 32, 32);
    kanvas_desktop_add_icon(g_desktop, "Terminal", 2, 32, 96);
    kanvas_desktop_add_icon(g_desktop, "Settings", 3, 32, 160);
    kanvas_desktop_add_icon(g_desktop, "Browser", 4, 32, 224);
    kanvas_desktop_add_icon(g_desktop, "Editor", 5, 32, 288);
    kanvas_desktop_add_icon(g_desktop, "Calculator", 6, 96, 32);
    kanvas_desktop_add_icon(g_desktop, "Image Viewer", 7, 96, 96);
    kanvas_desktop_add_icon(g_desktop, "Music", 8, 96, 160);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "Files", "/kex/files", 1, KANVAS_START_SECTION_PINNED, NULL);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "Terminal", "/kex/terminal", 2, KANVAS_START_SECTION_PINNED, NULL);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "Browser", "/kex/browser", 4, KANVAS_START_SECTION_PINNED, NULL);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "Editor", "/kex/editor", 5, KANVAS_START_SECTION_PINNED, NULL);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "Settings", "/kex/settings", 3, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "Calculator", "/kex/calc", 6, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "Image Viewer", "/kex/imgview", 7, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "Music Player", "/kex/music", 8, KANVAS_START_SECTION_ALL, NULL);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "System Monitor", "/kex/sysmon", 9, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_add_item(&g_desktop->start_menu, "Package Manager", "/kex/pkgman", 10, KANVAS_START_SECTION_SYSTEM, NULL);
    kanvas_start_menu_filter(&g_desktop->start_menu);
    kanvas_taskbar_add_item(&g_desktop->taskbar, "Files", 1, true, NULL);
    kanvas_taskbar_add_item(&g_desktop->taskbar, "Terminal", 2, true, NULL);
    kanvas_taskbar_add_item(&g_desktop->taskbar, "Browser", 4, true, NULL);
    kanvas_taskbar_add_item(&g_desktop->taskbar, "Editor", 5, true, NULL);
    kanvas_desktop_tray_add(g_desktop, "Network", NULL);
    kanvas_desktop_tray_add(g_desktop, "Volume", NULL);
    kanvas_desktop_tray_add(g_desktop, "Battery", NULL);
    kanvas_desktop_apply_theme_md3_dark(g_desktop);
    return 0;
}

void kanvas_gui_shutdown(void)
{
    if (g_desktop) {
        kanvas_desktop_destroy(g_desktop);
        g_desktop = NULL;
    }
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
    if (g_desktop->start_menu.visible && g_desktop->start_menu.search_focused) {
        kanvas_start_menu_handle_char(&g_desktop->start_menu, ch);
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
    kanvas_desktop_update(g_desktop, now_ms);
}

void kanvas_gui_paint(void)
{
    if (!g_desktop) return;
    kanvas_desktop_paint(g_desktop);
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