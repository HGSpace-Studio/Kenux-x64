#ifndef KANVAS_GUI_H
#define KANVAS_GUI_H

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

kanvas_boot_splash_t* kanvas_gui_get_splash(void);
kanvas_system_tray_t* kanvas_gui_get_system_tray(void);
kanvas_kex_runner_t* kanvas_gui_get_kex_runner(void);
kanvas_file_manager_t* kanvas_gui_get_file_manager(void);
kanvas_task_manager_t* kanvas_gui_get_task_manager(void);
kanvas_terminal_t* kanvas_gui_get_terminal(void);

void kanvas_gui_skip_splash(void);
bool kanvas_gui_is_splash_active(void);

#endif