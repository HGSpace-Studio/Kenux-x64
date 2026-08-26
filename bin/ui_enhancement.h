#ifndef UI_ENHANCEMENT_H
#define UI_ENHANCEMENT_H

#include <stdint.h>
#include <stddef.h>

// UI增强功能定义
typedef struct {
    uint32_t ui_flags;
    uint32_t theme_color;
    uint32_t font_size;
    uint32_t window_style;
    uint32_t animation_speed;
    uint32_t transparency_level;
    uint32_t sound_enabled;
    uint32_t touch_support;
    uint32_t gesture_support;
    uint32_t multi_window_support;
    uint32_t desktop_effects;
    uint32_t menu_style;
} ui_enhancement_t;

// UI标志
#define UI_FLAG_DARK_MODE           (1 << 0)
#define UI_FLAG_ANIMATIONS         (1 << 1)
#define UI_FLAG_SOUND               (1 << 2)
#define UI_FLAG_TOUCH               (1 << 3)
#define UI_FLAG_GESTURES           (1 << 4)
#define UI_FLAG_MULTI_WINDOW        (1 << 5)
#define UI_FLAG_DESKTOP_EFFECTS    (1 << 6)
#define UI_FLAG_ADVANCED_THEMES    (1 << 7)
#define UI_FLAG_ICON_PACK           (1 << 8)
#define UI_FLAG_TASKBAR            (1 << 9)
#define UI_FLAG_NOTIFICATIONS      (1 << 10)
#define UI_FLAG_START_MENU         (1 << 11)
#define UI_FLAG_FILE_MANAGER       (1 << 12)
#define UI_FLAG_CONTROL_PANEL      (1 << 13)
#define UI_FLAG_SYSTEM_SETTINGS    (1 << 14)
#define UI_FLAG_THEME_MANAGER      (1 << 15)

// 主题颜色
#define THEME_COLOR_BLUE           0x0000FF
#define THEME_COLOR_GREEN          0x00FF00
#define THEME_COLOR_RED            0xFF0000
#define THEME_COLOR_YELLOW         0xFFFF00
#define THEME_COLOR_PURPLE         0xFF00FF
#define THEME_COLOR_CYAN           0x00FFFF
#define THEME_COLOR_ORANGE         0xFFA500
#define THEME_COLOR_PINK           0xFFC0CB
#define THEME_COLOR_GRAY           0x808080
#define THEME_COLOR_BLACK          0x000000
#define THEME_COLOR_WHITE          0xFFFFFF

// 字体大小
#define FONT_SIZE_SMALL             8
#define FONT_SIZE_MEDIUM           12
#define FONT_SIZE_LARGE            16
#define FONT_SIZE_EXTRA_LARGE      20

// 窗口样式
#define WINDOW_STYLE_MODERN        0
#define WINDOW_STYLE_CLASSIC       1
#define WINDOW_STYLE_FLAT          2
#define WINDOW_STYLE_GLASS         3
#define WINDOW_STYLE_METALLIC      4

// 动画速度
#define ANIMATION_SPEED_SLOW       1000
#define ANIMATION_SPEED_MEDIUM     500
#define ANIMATION_SPEED_FAST       250
#define ANIMATION_SPEED_INSTANT    0

// 透明度级别
#define TRANSPARENCY_LEVEL_NONE    0
#define TRANSPARENCY_LEVEL_LOW     25
#define TRANSPARENCY_LEVEL_MEDIUM  50
#define TRANSPARENCY_LEVEL_HIGH    75
#define TRANSPARENCY_LEVEL_FULL    100

// 菜单样式
#define MENU_STYLE_CLASSIC         0
#define MENU_STYLE_MODERN         1
#define MENU_STYLE_FLAT           2
#define MENU_STYLE_MATERIAL        3

// 桌面效果
#define DESKTOP_EFFECT_NONE        0
#define DESKTOP_EFFECT_BLUR        1
#define DESKTOP_EFFECT_SHADOW      2
#define DESKTOP_EFFECT_GLOW        3
#define DESKTOP_EFFECT_FROST       4
#define DESKTOP_EFFECT_GLASS       5

// 函数声明
void ui_enhancement_init(void);
void ui_apply_theme(void);
int ui_set_theme_color(uint32_t color);
int ui_set_font_size(uint32_t size);
int ui_set_window_style(uint32_t style);
int ui_set_animation_speed(uint32_t speed);
int ui_set_transparency(uint32_t level);
int ui_set_menu_style(uint32_t style);
int ui_toggle_dark_mode(void);
int ui_toggle_animations(void);
int ui_toggle_touch_support(void);
int ui_toggle_gesture_support(void);
int ui_toggle_multi_window_support(void);
int ui_toggle_desktop_effects(void);
int ui_toggle_taskbar(void);
int ui_toggle_notifications(void);
int ui_toggle_start_menu(void);
int ui_toggle_file_manager(void);
int ui_toggle_control_panel(void);
int ui_toggle_system_settings(void);
int ui_toggle_theme_manager(void);
int ui_get_config(char* buffer, size_t buffer_size);
void ui_create_enhanced_desktop(void);
int ui_apply_theme_preset(const char* preset_name);

// UI增强命令
int ui_enhancement_shell_commands(char** args, int arg_count);

#endif /* UI_ENHANCEMENT_H */