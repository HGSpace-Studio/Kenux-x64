#include <kapi.h>
#include "stardustui_integration.h"
#include "display_enhanced.h"
#include "system_integration.h"
#include "power_management.h"

#define DESKTOP_MAX_WINDOWS  64
#define TASKBAR_HEIGHT       48
#define START_MENU_WIDTH     320
#define START_MENU_HEIGHT    480
#define ICON_SIZE            48
#define ICON_GRID_SIZE       80
#define MAX_DESKTOP_ICONS    24

typedef struct {
    int x, y;
    int width, height;
    char label[128];
    char command[256];
    uint32_t icon_id;
    bool visible;
} desktop_icon_t;

typedef struct {
    window_context_t* window;
    taskbar_entry_t entry;
    int z_order;
    bool minimized;
    bool maximized;
    bool focused;
} managed_window_t;

typedef struct {
    window_context_t* start_menu;
    bool start_menu_visible;
    window_context_t* taskbar;
    desktop_icon_t icons[MAX_DESKTOP_ICONS];
    int icon_count;
    managed_window_t windows[DESKTOP_MAX_WINDOWS];
    int window_count;
    int active_window_idx;
    color_t wallpaper_color;
    uint32_t* wallpaper_image;
    int wallpaper_width;
    int wallpaper_height;
    theme_t current_theme;
    spinlock_t lock;
    bool initialized;
    bool show_desktop_icons;
    int grid_cols;
    int grid_rows;
} desktop_manager_t;

static desktop_manager_t desktop;

void desktop_init(int screen_width, int screen_height)
{
    if (desktop.initialized) return;

    spin_init(&desktop.lock);
    memset(&desktop, sizeof(desktop_manager_t), 0);

    desktop.window_count = 0;
    desktop.icon_count = 0;
    desktop.active_window_idx = -1;
    desktop.start_menu_visible = false;
    desktop.show_desktop_icons = true;
    desktop.wallpaper_color = (color_t){30, 30, 50, 255};
    desktop.wallpaper_image = NULL;
    desktop.wallpaper_width = 0;
    desktop.wallpaper_height = 0;

    desktop.grid_cols = (screen_width - 20) / ICON_GRID_SIZE;
    desktop.grid_rows = (screen_height - TASKBAR_HEIGHT - 20) / ICON_GRID_SIZE;

    desktop.current_theme = stardustui_get_default_theme();

    desktop.taskbar = system_create_window("Taskbar", screen_width, TASKBAR_HEIGHT,
                                          WINDOW_FLAG_BORDERLESS | WINDOW_FLAG_ALWAYS_ON_TOP);
    if (desktop.taskbar) {
        rect_t taskbar_rect = {0, screen_height - TASKBAR_HEIGHT, screen_width, TASKBAR_HEIGHT};
        stardustui_move_window(desktop.taskbar, 0, screen_height - TASKBAR_HEIGHT);

        create_taskbar_buttons();
    }

    desktop.start_menu = system_create_window("Start Menu", START_MENU_WIDTH, START_MENU_HEIGHT,
                                              WINDOW_FLAG_BORDERLESS | WINDOW_FLAG_ALWAYS_ON_TOP);
    if (desktop.start_menu) {
        stardustui_hide_window(desktop.start_menu);
        populate_start_menu();
    }

    add_default_desktop_icons();

    desktop.initialized = true;
}

void create_taskbar_buttons(void)
{
    if (!desktop.taskbar) return;

    int btn_width = 120;
    int btn_height = 32;
    int y_offset = (TASKBAR_HEIGHT - btn_height) / 2;

    button_component_t* start_btn = system_create_button(
        desktop.taskbar, "Start",
        (rect_t){10, y_offset, btn_width, btn_height},
        on_start_button_click
    );

    if (start_btn) {
        stardustui_set_component_style(start_btn, desktop.current_theme.accent,
                                       (color_t){255, 255, 255, 255});
    }

    show_running_apps_in_taskbar();
}

void show_running_apps_in_taskbar(void)
{
    if (!desktop.taskbar) return;

    int x_offset = 140;

    for (int i = 0; i < desktop.window_count; i++) {
        if (!desktop.windows[i].window) continue;

        char title[64];
        strncpy(title, desktop.windows[i].window->title, 63);

        int btn_width = min(150, strlen(title) * 8 + 20);
        button_component_t* app_btn = system_create_button(
            desktop.taskbar, title,
            (rect_t){x_offset, 8, btn_width, TASKBAR_HEIGHT - 16},
            on_taskbar_app_click
        );

        if (app_btn) {
            app_btn->user_data = (void*)(intptr_t)i;
            x_offset += btn_width + 5;
        }
    }
}

void populate_start_menu(void)
{
    if (!desktop.start_menu) return;

    int y_offset = 10;
    const char* menu_items[] = {
        "File Manager", "filemanager",
        "Terminal", "terminal",
        "Settings", "settings",
        "Browser", "browser",
        "Text Editor", "texteditor",
        "System Info", "sysinfo",
        "Power Off", "poweroff",
        "Reboot", "reboot",
        NULL
    };

    for (int i = 0; menu_items[i]; i += 2) {
        const char* name = menu_items[i];
        const char* cmd = menu_items[i + 1];

        button_component_t* item = system_create_button(
            desktop.start_menu, name,
            (rect_t){10, y_offset, START_MENU_WIDTH - 20, 36},
            on_start_menu_item_click
        );

        if (item) {
            strncpy(item->base.name, cmd, 63);
            y_offset += 40;
        }
    }
}

void add_default_desktop_icons(void)
{
    if (desktop.icon_count >= MAX_DESKTOP_ICONS) return;

    struct {
        const char* label;
        const char* command;
        uint32_t icon_id;
    } default_icons[] = {
        {"My Computer", "filemanager://", ICON_COMPUTER},
        {"Recycle Bin", "recyclebin://", ICON_TRASH},
        {"Terminal", "terminal", ICON_TERMINAL},
        {"Settings", "controlpanel", ICON_SETTINGS},
        {"Documents", "/home/user/Documents", ICON_FOLDER},
        {NULL, NULL, 0}
    };

    for (int i = 0; default_icons[i].label; i++) {
        if (desktop.icon_count >= MAX_DESKTOP_ICONS) break;

        int col = desktop.icon_count % desktop.grid_cols;
        int row = desktop.icon_count / desktop.grid_cols;

        desktop_icon_t* icon = &desktop.icons[desktop.icon_count];
        icon->x = 20 + col * ICON_GRID_SIZE;
        icon->y = 20 + row * ICON_GRID_SIZE;
        icon->width = ICON_SIZE;
        icon->height = ICON_SIZE;
        strncpy(icon->label, default_icons[i].label, 127);
        strncpy(icon->command, default_icons[i].command, 255);
        icon->icon_id = default_icons[i].icon_id;
        icon->visible = true;

        desktop.icon_count++;
    }
}

int desktop_create_managed_window(const char* title, int width, int height,
                                   window_flags_t flags)
{
    if (!title || desktop.window_count >= DESKTOP_MAX_WINDOWS) return -ENOSPC;

    spin_lock(&desktop.lock);

    int idx = desktop.window_count++;
    managed_window_t* mw = &desktop.windows[idx];

    mw->window = system_create_window(title, width, height, flags);
    if (!mw->window) {
        desktop.window_count--;
        spin_unlock(&desktop.lock);
        return -ENOMEM;
    }

    mw->z_order = idx;
    mw->minimized = false;
    mw->maximized = false;
    mw->focused = true;

    strncpy(mw->entry.title, title, 63);
    mw->entry.window_id = mw->window->id;
    mw->entry.is_active = true;

    for (int i = 0; i < idx; i++) {
        desktop.windows[i].focused = false;
        desktop.windows[i].entry.is_active = false;
    }

    desktop.active_window_idx = idx;

    spin_unlock(&desktop.lock);

    show_running_apps_in_taskbar();

    return idx;
}

int desktop_close_window(int window_idx)
{
    if (window_idx < 0 || window_idx >= desktop.window_count) return -EINVAL;

    spin_lock(&desktop.lock);

    managed_window_t* mw = &desktop.windows[window_idx];
    if (mw->window) {
        stardustui_destroy_window(mw->window);
        mw->window = NULL;
    }

    for (int i = window_idx; i < desktop.window_count - 1; i++) {
        desktop.windows[i] = desktop.windows[i + 1];
    }

    desktop.window_count--;

    if (desktop.active_window_idx == window_idx) {
        desktop.active_window_idx = desktop.window_count > 0 ? desktop.window_count - 1 : -1;
    } else if (desktop.active_window_idx > window_idx) {
        desktop.active_window_idx--;
    }

    spin_unlock(&desktop.lock);

    show_running_apps_in_taskbar();

    return 0;
}

int desktop_focus_window(int window_idx)
{
    if (window_idx < 0 || window_idx >= desktop.window_count) return -EINVAL;

    spin_lock(&desktop.lock);

    for (int i = 0; i < desktop.window_count; i++) {
        desktop.windows[i].focused = (i == window_idx);
        desktop.windows[i].entry.is_active = (i == window_idx);
    }

    desktop.active_window_idx = window_idx;

    if (desktop.windows[window_idx].minimized) {
        stardustui_restore_window(desktop.windows[window_idx].window);
        desktop.windows[window_idx].minimized = false;
    }

    stardustui_raise_window(desktop.windows[window_idx].window);
    stardustui_focus_window(desktop.windows[window_idx].window);

    spin_unlock(&desktop.lock);

    return 0;
}

int desktop_minimize_window(int window_idx)
{
    if (window_idx < 0 || window_idx >= desktop.window_count) return -EINVAL;

    spin_lock(&desktop.lock);

    stardustui_minimize_window(desktop.windows[window_idx].window);
    desktop.windows[window_idx].minimized = true;
    desktop.windows[window_idx].focused = false;

    if (desktop.active_window_idx == window_idx) {
        desktop.active_window_idx = -1;
        for (int i = desktop.window_count - 1; i >= 0; i--) {
            if (!desktop.windows[i].minimized) {
                desktop.focus_window(i);
                break;
            }
        }
    }

    spin_unlock(&desktop.lock);

    return 0;
}

int desktop_maximize_window(int window_idx)
{
    if (window_idx < 0 || window_idx >= desktop.window_count) return -EINVAL;

    spin_lock(&desktop.lock);

    if (desktop.windows[window_idx].maximized) {
        stardustui_restore_window(desktop.windows[window_idx].window);
        desktop.windows[window_idx].maximized = false;
    } else {
        stardustui_maximize_window(desktop.windows[window_idx].window);
        desktop.windows[window_idx].maximized = true;
    }

    spin_unlock(&desktop.lock);

    return 0;
}

void on_start_button_click(button_component_t* btn)
{
    if (!desktop.start_menu) return;

    spin_lock(&desktop.lock);

    if (desktop.start_menu_visible) {
        stardustui_hide_window(desktop.start_menu);
        desktop.start_menu_visible = false;
    } else {
        fb_info_t* fb = display_get_info();
        if (fb) {
            int x = 10;
            int y = fb->height - TASKBAR_HEIGHT - START_MENU_HEIGHT - 10;
            stardustui_move_window(desktop.start_menu, x, y);
        }
        stardustui_show_window(desktop.start_menu);
        desktop.start_menu_visible = true;
    }

    spin_unlock(&desktop.lock);
}

void on_start_menu_item_click(button_component_t* btn)
{
    if (!btn || !btn->base.name) return;

    const char* cmd = btn->base.name;

    if (strcmp(cmd, "poweroff") == 0) {
        acpi_power_off();
    } else if (strcmp(cmd, "reboot") == 0) {
        acpi_reset();
    } else {
        system_create_process(cmd, NULL, NULL, PROCESS_FLAG_NONE, PRIORITY_NORMAL);
    }

    if (desktop.start_menu_visible) {
        stardustui_hide_window(desktop.start_menu);
        desktop.start_menu_visible = false;
    }
}

void on_taskbar_app_click(button_component_t* btn)
{
    if (!btn) return;

    int idx = (int)(intptr_t)btn->user_data;
    if (idx >= 0 && idx < desktop.window_count) {
        if (desktop.windows[idx].minimized ||
            desktop.active_window_idx != idx) {
            desktop_focus_window(idx);
        } else {
            desktop_minimize_window(idx);
        }
    }
}

void on_desktop_icon_double_click(int icon_idx)
{
    if (icon_idx < 0 || icon_idx >= desktop.icon_count) return;

    desktop_icon_t* icon = &desktop.icons[icon_idx];
    if (!icon->visible || !icon->command[0]) return;

    system_create_process(icon->command, NULL, NULL, PROCESS_FLAG_NONE, PRIORITY_NORMAL);
}

void desktop_handle_mouse_event(mouse_event_t* event)
{
    if (!event || !desktop.initialized) return;

    switch (event->type) {
        case MOUSE_EVENT_DOUBLE_CLICK:
            for (int i = 0; i < desktop.icon_count; i++) {
                desktop_icon_t* icon = &desktop.icons[i];
                if (!icon->visible) continue;

                if (event->x >= icon->x && event->x < icon->x + icon->width &&
                    event->y >= icon->y && event->y < icon->y + icon->height) {
                    on_desktop_icon_double_click(i);
                    return;
                }
            }
            break;

        case MOUSE_EVENT_MOVE:
            if (desktop.start_menu_visible && !is_point_in_window(desktop.start_menu,
                                                                     event->x, event->y)) {
                if (!is_point_in_window(desktop.taskbar, event->x, event->y)) {
                    stardustui_hide_window(desktop.start_menu);
                    desktop.start_menu_visible = false;
                }
            }
            break;

        default:
            break;
    }
}

void desktop_handle_keyboard_event(keyboard_event_t* event)
{
    if (!event || !desktop.initialized) return;

    if (event->type == KEYBOARD_EVENT_KEY_DOWN) {
        switch (event->keycode) {
            case KEY_SUPER_L:
                on_start_button_click(NULL);
                break;

            case KEY_ESCAPE:
                if (desktop.start_menu_visible) {
                    stardustui_hide_window(desktop.start_menu);
                    desktop.start_menu_visible = false;
                }
                break;

            case KEY_F4:
                if (event->modifiers & KEY_MOD_ALT) {
                    if (desktop.active_window_idx >= 0) {
                        desktop_close_window(desktop.active_window_idx);
                    }
                }
                break;

            case KEY_TAB:
                if (event->modifiers & KEY_MOD_ALT) {
                    int next = desktop.active_window_idx + 1;
                    if (next >= desktop.window_count) next = 0;
                    desktop_focus_window(next);
                }
                break;

            default:
                if (desktop.active_window_idx >= 0 &&
                    desktop.windows[desktop.active_window_idx].window) {
                    stardustui_send_key_event(desktop.windows[desktop.active_window_idx].window,
                                              event);
                }
                break;
        }
    }
}

void desktop_render(void)
{
    if (!desktop.initialized) return;

    fb_info_t* fb = display_get_info();
    if (!fb) return;

    uint32_t* buffer = display_get_back_buffer();
    if (!buffer) return;

    for (int y = 0; y < fb->height - TASKBAR_HEIGHT; y++) {
        for (int x = 0; x < fb->width; x++) {
            if (desktop.wallpaper_image &&
                x < desktop.wallpaper_width && y < desktop.wallpaper_height) {
                buffer[y * fb->width + x] = desktop.wallpaper_image[y * desktop.wallpaper_width + x];
            } else {
                buffer[y * fb->width + x] = (desktop.wallpaper_color.a << 24) |
                                             (desktop.wallpaper_color.r << 16) |
                                             (desktop.wallpaper_color.g << 8) |
                                             desktop.wallpaper_color.b;
            }
        }
    }

    if (desktop.show_desktop_icons) {
        render_desktop_icons(buffer, fb->width, fb->height);
    }

    display_composite_layers();
    display_swap_buffers();
}

void render_desktop_icons(uint32_t* buffer, int screen_width, int screen_height)
{
    for (int i = 0; i < desktop.icon_count; i++) {
        desktop_icon_t* icon = &desktop.icons[i];
        if (!icon->visible) continue;

        draw_icon(buffer, screen_width, icon->x, icon->y, icon->icon_id, ICON_SIZE);

        int text_x = icon->x + (ICON_SIZE - strlen(icon->label) * 6) / 2;
        int text_y = icon->y + ICON_SIZE + 2;

        draw_text_with_background(buffer, screen_width, text_x, text_y,
                                   icon->label, (color_t){255, 255, 255, 255},
                                   (color_t){0, 0, 0, 128}, 8);
    }
}

bool is_point_in_window(window_context_t* win, int x, int y)
{
    if (!win) return false;

    rect_t* r = &win->client_rect;
    return (x >= r->x && x < r->x + r->width &&
            y >= r->y && y < r->y + r->height);
}

int desktop_set_wallpaper(const char* image_path)
{
    if (!image_path) return -EINVAL;

    if (desktop.wallpaper_image) {
        kfree(desktop.wallpaper_image);
        desktop.wallpaper_image = NULL;
    }

    int fd = vfs_open(image_path, O_RDONLY);
    if (fd < 0) {
        desktop.wallpaper_color = (color_t){30, 30, 50, 255};
        return fd;
    }

    image_header_t header;
    ssize_t read_size = vfs_read(fd, &header, sizeof(header));
    if (read_size != sizeof(header)) {
        vfs_close(fd);
        return -EIO;
    }

    if (header.magic != IMAGE_MAGIC_BMP && header.magic != IMAGE_MAGIC_PNG) {
        vfs_close(fd);
        return -EINVAL;
    }

    desktop.wallpaper_width = header.width;
    desktop.wallpaper_height = header.height;

    size_t data_size = header.width * header.height * sizeof(uint32_t);
    desktop.wallpaper_image = kmalloc(data_size);
    if (!desktop.wallpaper_image) {
        vfs_close(fd);
        return -ENOMEM;
    }

    read_size = vfs_read(fd, desktop.wallpaper_image, data_size);
    vfs_close(fd);

    if (read_size != (ssize_t)data_size) {
        kfree(desktop.wallpaper_image);
        desktop.wallpaper_image = NULL;
        return -EIO;
    }

    return 0;
}

void desktop_set_wallpaper_color(color_t color)
{
    if (desktop.wallpaper_image) {
        kfree(desktop.wallpaper_image);
        desktop.wallpaper_image = NULL;
    }

    desktop.wallpaper_color = color;
}

int desktop_add_icon(const char* label, const char* command, uint32_t icon_id)
{
    if (!label || !command || desktop.icon_count >= MAX_DESKTOP_ICONS) return -EINVAL;

    spin_lock(&desktop.lock);

    int idx = desktop.icon_count++;
    desktop_icon_t* icon = &desktop.icons[idx];

    int col = idx % desktop.grid_cols;
    int row = idx / desktop.grid_cols;

    icon->x = 20 + col * ICON_GRID_SIZE;
    icon->y = 20 + row * ICON_GRID_SIZE;
    icon->width = ICON_SIZE;
    icon->height = ICON_SIZE;
    strncpy(icon->label, label, 127);
    strncpy(icon->command, command, 255);
    icon->icon_id = icon_id;
    icon->visible = true;

    spin_unlock(&desktop.lock);

    return idx;
}

void desktop_remove_icon(int icon_idx)
{
    if (icon_idx < 0 || icon_idx >= desktop.icon_count) return;

    spin_lock(&desktop.lock);

    for (int i = icon_idx; i < desktop.icon_count - 1; i++) {
        desktop.icons[i] = desktop.icons[i + 1];
    }

    desktop.icon_count--;

    spin_unlock(&desktop.lock);
}

void desktop_set_show_icons(bool show)
{
    desktop.show_desktop_icons = show;
}

void desktop_set_theme(theme_t* theme)
{
    if (!theme) return;

    memcpy(&desktop.current_theme, theme, sizeof(theme_t));
    stardustui_set_theme(theme);

    if (desktop.taskbar) {
        stardustui_invalidate_window(desktop.taskbar);
    }

    if (desktop.start_menu) {
        stardustui_invalidate_window(desktop.start_menu);
    }

    for (int i = 0; i < desktop.window_count; i++) {
        if (desktop.windows[i].window) {
            stardustui_invalidate_window(desktop.windows[i].window);
        }
    }
}

theme_t* desktop_get_theme(void)
{
    return &desktop.current_theme;
}

int desktop_get_window_count(void)
{
    return desktop.window_count;
}

managed_window_t* desktop_get_window(int index)
{
    if (index < 0 || index >= desktop.window_count) return NULL;
    return &desktop.windows[index];
}

int desktop_get_active_window(void)
{
    return desktop.active_window_idx;
}

void desktop_cleanup(void)
{
    if (!desktop.initialized) return;

    spin_lock(&desktop.lock);

    for (int i = 0; i < desktop.window_count; i++) {
        if (desktop.windows[i].window) {
            stardustui_destroy_window(desktop.windows[i].window);
        }
    }
    desktop.window_count = 0;
    desktop.active_window_idx = -1;

    if (desktop.taskbar) {
        stardustui_destroy_window(desktop.taskbar);
        desktop.taskbar = NULL;
    }

    if (desktop.start_menu) {
        stardustui_destroy_window(desktop.start_menu);
        desktop.start_menu = NULL;
    }

    if (desktop.wallpaper_image) {
        kfree(desktop.wallpaper_image);
        desktop.wallpaper_image = NULL;
    }

    spin_unlock(&desktop.lock);

    desktop.initialized = false;
}