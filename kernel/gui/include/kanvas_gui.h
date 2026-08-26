#ifndef KANVAS_GUI_H
#define KANVAS_GUI_H

#include "kanvas_desktop.h"
#include "kanvas_window.h"
#include "kanvas_taskbar.h"
#include "kanvas_start_menu.h"

int kanvas_gui_init(int screen_w, int screen_h, uint32_t* fb, int stride);
void kanvas_gui_shutdown(void);
void kanvas_gui_process_mouse(int x, int y, bool left, bool right, bool mid);
void kanvas_gui_process_key(int key, bool down, uint32_t mods);
void kanvas_gui_process_char(uint32_t ch);
void kanvas_gui_process_scroll(int delta);
void kanvas_gui_update(uint64_t now_ms);
void kanvas_gui_paint(void);
void kanvas_gui_set_theme(int theme_id);
kanvas_desktop_t* kanvas_gui_get_desktop(void);

#endif