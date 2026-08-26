#include <arch/desktop.h>
#include <arch/vga.h>
#include <string.h>
#include <memory.h>
#include <fs.h>
#include <syscall.h>
#include <process.h>

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

static ui_enhancement_t ui_config;

// 初始化UI增强功能
void ui_enhancement_init(void) {
    memset(&ui_config, 0, sizeof(ui_config));
    
    // 默认设置
    ui_config.ui_flags = UI_FLAG_DARK_MODE | UI_FLAG_ANIMATIONS | UI_FLAG_MULTI_WINDOW | 
                        UI_FLAG_DESKTOP_EFFECTS | UI_FLAG_TASKBAR | UI_FLAG_NOTIFICATIONS |
                        UI_FLAG_START_MENU | UI_FLAG_FILE_MANAGER | UI_FLAG_CONTROL_PANEL |
                        UI_FLAG_SYSTEM_SETTINGS | UI_FLAG_THEME_MANAGER;
    
    ui_config.theme_color = THEME_COLOR_BLUE;
    ui_config.font_size = FONT_SIZE_MEDIUM;
    ui_config.window_style = WINDOW_STYLE_MODERN;
    ui_config.animation_speed = ANIMATION_SPEED_MEDIUM;
    ui_config.transparency_level = TRANSPARENCY_LEVEL_NONE;
    ui_config.sound_enabled = 1;
    ui_config.touch_support = 1;
    ui_config.gesture_support = 1;
    ui_config.multi_window_support = 1;
    ui_config.desktop_effects = DESKTOP_EFFECT_BLUR;
    ui_config.menu_style = MENU_STYLE_MODERN;
}

// 应用UI主题
void ui_apply_theme(void) {
    uint32_t bg_color, fg_color, accent_color;
    
    if (ui_config.ui_flags & UI_FLAG_DARK_MODE) {
        bg_color = 0x1E1E1E;  // 深灰色背景
        fg_color = 0xFFFFFF;  // 白色文字
        accent_color = ui_config.theme_color;
    } else {
        bg_color = 0xF0F0F0;  // 浅灰色背景
        fg_color = 0x000000;  // 黑色文字
        accent_color = ui_config.theme_color;
    }
    
    // 应用颜色主题
    vga_set_background_color(bg_color);
    vga_set_foreground_color(fg_color);
    
    // 应用主题到桌面
    desktop_set_background_color(bg_color);
    desktop_set_foreground_color(fg_color);
    desktop_set_accent_color(accent_color);
    
    // 应用窗口样式
    desktop_set_window_style(ui_config.window_style);
    
    // 应用字体大小
    desktop_set_font_size(ui_config.font_size);
    
    // 应用透明度
    if (ui_config.transparency_level > TRANSPARENCY_LEVEL_NONE) {
        desktop_set_transparency(ui_config.transparency_level);
    }
    
    // 应用桌面效果
    if (ui_config.desktop_effects > DESKTOP_EFFECT_NONE) {
        desktop_set_effect(ui_config.desktop_effects);
    }
    
    // 应用菜单样式
    desktop_set_menu_style(ui_config.menu_style);
}

// 设置主题颜色
int ui_set_theme_color(uint32_t color) {
    ui_config.theme_color = color;
    ui_apply_theme();
    return 0;
}

// 设置字体大小
int ui_set_font_size(uint32_t size) {
    if (size < FONT_SIZE_SMALL || size > FONT_SIZE_EXTRA_LARGE) {
        return -1;
    }
    
    ui_config.font_size = size;
    ui_apply_theme();
    return 0;
}

// 设置窗口样式
int ui_set_window_style(uint32_t style) {
    if (style > WINDOW_STYLE_METALLIC) {
        return -1;
    }
    
    ui_config.window_style = style;
    ui_apply_theme();
    return 0;
}

// 设置动画速度
int ui_set_animation_speed(uint32_t speed) {
    if (speed > ANIMATION_SPEED_SLOW) {
        return -1;
    }
    
    ui_config.animation_speed = speed;
    desktop_set_animation_speed(speed);
    return 0;
}

// 设置透明度
int ui_set_transparency(uint32_t level) {
    if (level > TRANSPARENCY_LEVEL_FULL) {
        return -1;
    }
    
    ui_config.transparency_level = level;
    ui_apply_theme();
    return 0;
}

// 设置菜单样式
int ui_set_menu_style(uint32_t style) {
    if (style > MENU_STYLE_MATERIAL) {
        return -1;
    }
    
    ui_config.menu_style = style;
    ui_apply_theme();
    return 0;
}

// 切换暗色/亮色模式
int ui_toggle_dark_mode(void) {
    ui_config.ui_flags ^= UI_FLAG_DARK_MODE;
    ui_apply_theme();
    return 0;
}

// 切换动画效果
int ui_toggle_animations(void) {
    ui_config.ui_flags ^= UI_FLAG_ANIMATIONS;
    desktop_set_animations_enabled(ui_config.ui_flags & UI_FLAG_ANIMATIONS);
    return 0;
}

// 切换触摸支持
int ui_toggle_touch_support(void) {
    ui_config.ui_flags ^= UI_FLAG_TOUCH;
    desktop_set_touch_support(ui_config.ui_flags & UI_FLAG_TOUCH);
    return 0;
}

// 切换手势支持
int ui_toggle_gesture_support(void) {
    ui_config.ui_flags ^= UI_FLAG_GESTURES;
    desktop_set_gesture_support(ui_config.ui_flags & UI_FLAG_GESTURES);
    return 0;
}

// 切换多窗口支持
int ui_toggle_multi_window_support(void) {
    ui_config.ui_flags ^= UI_FLAG_MULTI_WINDOW;
    desktop_set_multi_window_support(ui_config.ui_flags & UI_FLAG_MULTI_WINDOW);
    return 0;
}

// 切换桌面效果
int ui_toggle_desktop_effects(void) {
    ui_config.ui_flags ^= UI_FLAG_DESKTOP_EFFECTS;
    desktop_set_desktop_effects_enabled(ui_config.ui_flags & UI_FLAG_DESKTOP_EFFECTS);
    return 0;
}

// 切换任务栏
int ui_toggle_taskbar(void) {
    ui_config.ui_flags ^= UI_FLAG_TASKBAR;
    desktop_set_taskbar_enabled(ui_config.ui_flags & UI_FLAG_TASKBAR);
    return 0;
}

// 切换通知
int ui_toggle_notifications(void) {
    ui_config.ui_flags ^= UI_FLAG_NOTIFICATIONS;
    desktop_set_notifications_enabled(ui_config.ui_flags & UI_FLAG_NOTIFICATIONS);
    return 0;
}

// 切换开始菜单
int ui_toggle_start_menu(void) {
    ui_config.ui_flags ^= UI_FLAG_START_MENU;
    desktop_set_start_menu_enabled(ui_config.ui_flags & UI_FLAG_START_MENU);
    return 0;
}

// 切换文件管理器
int ui_toggle_file_manager(void) {
    ui_config.ui_flags ^= UI_FLAG_FILE_MANAGER;
    desktop_set_file_manager_enabled(ui_config.ui_flags & UI_FLAG_FILE_MANAGER);
    return 0;
}

// 切换控制面板
int ui_toggle_control_panel(void) {
    ui_config.ui_flags ^= UI_FLAG_CONTROL_PANEL;
    desktop_set_control_panel_enabled(ui_config.ui_flags & UI_FLAG_CONTROL_PANEL);
    return 0;
}

// 切换系统设置
int ui_toggle_system_settings(void) {
    ui_config.ui_flags ^= UI_FLAG_SYSTEM_SETTINGS;
    desktop_set_system_settings_enabled(ui_config.ui_flags & UI_FLAG_SYSTEM_SETTINGS);
    return 0;
}

// 切换主题管理器
int ui_toggle_theme_manager(void) {
    ui_config.ui_flags ^= UI_FLAG_THEME_MANAGER;
    desktop_set_theme_manager_enabled(ui_config.ui_flags & UI_FLAG_THEME_MANAGER);
    return 0;
}

// 获取UI配置信息
int ui_get_config(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    
    const char* dark_mode = (ui_config.ui_flags & UI_FLAG_DARK_MODE) ? "enabled" : "disabled";
    const char* animations = (ui_config.ui_flags & UI_FLAG_ANIMATIONS) ? "enabled" : "disabled";
    const char* touch = (ui_config.ui_flags & UI_FLAG_TOUCH) ? "enabled" : "disabled";
    const char* gestures = (ui_config.ui_flags & UI_FLAG_GESTURES) ? "enabled" : "disabled";
    const char* multi_window = (ui_config.ui_flags & UI_FLAG_MULTI_WINDOW) ? "enabled" : "disabled";
    const char* desktop_effects = (ui_config.ui_flags & UI_FLAG_DESKTOP_EFFECTS) ? "enabled" : "disabled";
    const char* taskbar = (ui_config.ui_flags & UI_FLAG_TASKBAR) ? "enabled" : "disabled";
    const char* notifications = (ui_config.ui_flags & UI_FLAG_NOTIFICATIONS) ? "enabled" : "disabled";
    const char* start_menu = (ui_config.ui_flags & UI_FLAG_START_MENU) ? "enabled" : "disabled";
    const char* file_manager = (ui_config.ui_flags & UI_FLAG_FILE_MANAGER) ? "enabled" : "disabled";
    const char* control_panel = (ui_config.ui_flags & UI_FLAG_CONTROL_PANEL) ? "enabled" : "disabled";
    const char* system_settings = (ui_config.ui_flags & UI_FLAG_SYSTEM_SETTINGS) ? "enabled" : "disabled";
    const char* theme_manager = (ui_config.ui_flags & UI_FLAG_THEME_MANAGER) ? "enabled" : "disabled";
    
    snprintf(buffer, buffer_size,
        "UI Configuration:\n"
        "  Dark Mode: %s\n"
        "  Animations: %s\n"
        "  Touch Support: %s\n"
        "  Gesture Support: %s\n"
        "  Multi Window: %s\n"
        "  Desktop Effects: %s\n"
        "  Taskbar: %s\n"
        "  Notifications: %s\n"
        "  Start Menu: %s\n"
        "  File Manager: %s\n"
        "  Control Panel: %s\n"
        "  System Settings: %s\n"
        "  Theme Manager: %s\n"
        "  Theme Color: #06%02x%02x%02x\n"
        "  Font Size: %dpx\n"
        "  Window Style: %d\n"
        "  Animation Speed: %dms\n"
        "  Transparency: %d%%\n"
        "  Menu Style: %d\n",
        dark_mode,
        animations,
        touch,
        gestures,
        multi_window,
        desktop_effects,
        taskbar,
        notifications,
        start_menu,
        file_manager,
        control_panel,
        system_settings,
        theme_manager,
        (ui_config.theme_color >> 16) & 0xFF,
        (ui_config.theme_color >> 8) & 0xFF,
        ui_config.theme_color & 0xFF,
        ui_config.font_size,
        ui_config.window_style,
        ui_config.animation_speed,
        ui_config.transparency_level,
        ui_config.menu_style);
    
    return 0;
}

// 创建增强桌面组件
void ui_create_enhanced_desktop(void) {
    // 创建任务栏
    if (ui_config.ui_flags & UI_FLAG_TASKBAR) {
        desktop_create_taskbar();
    }
    
    // 创建通知中心
    if (ui_config.ui_flags & UI_FLAG_NOTIFICATIONS) {
        desktop_create_notification_center();
    }
    
    // 创建开始菜单
    if (ui_config.ui_flags & UI_FLAG_START_MENU) {
        desktop_create_start_menu();
    }
    
    // 创建文件管理器
    if (ui_config.ui_flags & UI_FLAG_FILE_MANAGER) {
        desktop_create_file_manager();
    }
    
    // 创建控制面板
    if (ui_config.ui_flags & UI_FLAG_CONTROL_PANEL) {
        desktop_create_control_panel();
    }
    
    // 创建系统设置
    if (ui_config.ui_flags & UI_FLAG_SYSTEM_SETTINGS) {
        desktop_create_system_settings();
    }
    
    // 创建主题管理器
    if (ui_config.ui_flags & UI_FLAG_THEME_MANAGER) {
        desktop_create_theme_manager();
    }
}

// 应用主题预设
int ui_apply_theme_preset(const char* preset_name) {
    if (!preset_name) return -1;
    
    if (strcmp(preset_name, "dark") == 0) {
        ui_config.ui_flags |= UI_FLAG_DARK_MODE;
        ui_config.theme_color = THEME_COLOR_BLUE;
        ui_config.window_style = WINDOW_STYLE_MODERN;
        ui_config.transparency_level = TRANSPARENCY_LEVEL_NONE;
    } else if (strcmp(preset_name, "light") == 0) {
        ui_config.ui_flags &= ~UI_FLAG_DARK_MODE;
        ui_config.theme_color = THEME_COLOR_BLUE;
        ui_config.window_style = WINDOW_STYLE_CLASSIC;
        ui_config.transparency_level = TRANSPARENCY_LEVEL_NONE;
    } else if (strcmp(preset_name, "blue") == 0) {
        ui_config.theme_color = THEME_COLOR_BLUE;
    } else if (strcmp(preset_name, "green") == 0) {
        ui_config.theme_color = THEME_COLOR_GREEN;
    } else if (strcmp(preset_name, "red") == 0) {
        ui_config.theme_color = THEME_COLOR_RED;
    } else if (strcmp(preset_name, "purple") == 0) {
        ui_config.theme_color = THEME_COLOR_PURPLE;
    } else if (strcmp(preset_name, "material") == 0) {
        ui_config.window_style = WINDOW_STYLE_FLAT;
        ui_config.menu_style = MENU_STYLE_MATERIAL;
        ui_config.ui_flags |= UI_FLAG_DESKTOP_EFFECTS;
    } else if (strcmp(preset_name, "glass") == 0) {
        ui_config.window_style = WINDOW_STYLE_GLASS;
        ui_config.transparency_level = TRANSPARENCY_LEVEL_HIGH;
        ui_config.ui_flags |= UI_FLAG_DESKTOP_EFFECTS;
    } else {
        return -1;
    }
    
    ui_apply_theme();
    return 0;
}