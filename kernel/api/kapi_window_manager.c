#include "kapi_window.h"
#include "kapi_graphics2d.h"
#include "kapi_input.h"
#include "kapi_logging.h"
#include "kapi_memory.h"

/* Window manager internal state */
static kapi_window_t* window_list = NULL;
static kapi_display_t* display_list = NULL;
static kapi_display_t* primary_display = NULL;
static uint32_t window_count = 0;
static uint32_t next_window_id = 1;
static kapi_window_manager_t* window_manager = NULL;

/* Window operations implementation */
static int window_create_impl(kapi_window_t* window, const kapi_window_properties_t* props);
static int window_destroy_impl(kapi_window_t* window);
static int window_show_impl(kapi_window_t* window);
static int window_hide_impl(kapi_window_t* window);
static int window_move_impl(kapi_window_t* window, int32_t x, int32_t y);
static int window_resize_impl(kapi_window_t* window, int32_t width, int32_t height);
static int window_minimize_impl(kapi_window_t* window);
static int window_maximize_impl(kapi_window_t* window);
static int window_restore_impl(kapi_window_t* window);
static int window_set_focus_impl(kapi_window_t* window);
static int window_remove_focus_impl(kapi_window_t* window);
static int window_bring_to_front_impl(kapi_window_t* window);
static int window_send_to_back_impl(kapi_window_t* window);
static int window_set_always_on_top_impl(kapi_window_t* window, int topmost);
static int window_set_transparency_impl(kapi_window_t* window, uint8_t alpha);
static kapi_surface_t* window_get_surface_impl(kapi_window_t* window);
static int window_set_title_impl(kapi_window_t* window, const char* title);
static int window_set_icon_impl(kapi_window_t* window, const char* icon_path);
static int window_set_background_color_impl(kapi_window_t* window, const kapi_color_t* color);

/* Default window operations structure */
static kapi_window_ops_t default_window_ops = {
    .create = window_create_impl,
    .destroy = window_destroy_impl,
    .show = window_show_impl,
    .hide = window_hide_impl,
    .move = window_move_impl,
    .resize = window_resize_impl,
    .minimize = window_minimize_impl,
    .maximize = window_maximize_impl,
    .restore = window_restore_impl,
    .set_focus = window_set_focus_impl,
    .remove_focus = window_remove_focus_impl,
    .bring_to_front = window_bring_to_front_impl,
    .send_to_back = window_send_to_back_impl,
    .set_always_on_top = window_set_always_on_top_impl,
    .set_transparency = window_set_transparency_impl,
    .get_surface = window_get_surface_impl,
    .set_title = window_set_title_impl,
    .set_icon = window_set_icon_impl,
    .set_background_color = window_set_background_color_impl
};

/* Display operations */
static int display_create(kapi_display_t* display);
static int display_destroy(kapi_display_t* display);
static int display_update(kapi_display_t* display);

/* Display operations structure */
static kapi_display_ops_t default_display_ops = {
    .create = display_create,
    .destroy = display_destroy,
    .update = display_update
};

/* Window manager initialization */
int kapi_window_manager_init(void) {
    kapi_log_info("Initializing window manager...");
    
    /* Allocate window manager structure */
    window_manager = (kapi_window_manager_t*)kapi_malloc(sizeof(kapi_window_manager_t));
    if (!window_manager) {
        kapi_log_err("Failed to allocate window manager");
        return -1;
    }
    
    /* Initialize window manager state */
    memset(window_manager, 0, sizeof(kapi_window_manager_t));
    
    /* Initialize displays */
    if (kapi_display_init() != 0) {
        kapi_log_err("Failed to initialize displays");
        kapi_free(window_manager);
        return -1;
    }
    
    /* Create primary display */
    primary_display = kapi_display_get_primary();
    if (!primary_display) {
        kapi_log_err("Failed to create primary display");
        kapi_display_cleanup();
        kapi_free(window_manager);
        return -1;
    }
    
    /* Create desktop window */
    kapi_window_properties_t desktop_props = {
        .title = "Desktop",
        .type = KAPI_WINDOW_TYPE_DESKTOP,
        .flags = KAPI_WINDOW_FLAG_VISIBLE | KAPI_WINDOW_FLAG_FULLSCREEN,
        .bounds = {0, 0, primary_display->width, primary_display->height},
        .client_rect = {0, 0, primary_display->width, primary_display->height},
        .background_color = {0, 0, 0, 255}  /* Black background */
    };
    
    kapi_window_t* desktop_window = kapi_window_create("Desktop", 0, 0, 
                                                    primary_display->width, primary_display->height,
                                                    KAPI_WINDOW_TYPE_DESKTOP, 
                                                    KAPI_WINDOW_FLAG_VISIBLE | KAPI_WINDOW_FLAG_FULLSCREEN);
    if (!desktop_window) {
        kapi_log_err("Failed to create desktop window");
        kapi_display_cleanup();
        kapi_free(window_manager);
        return -1;
    }
    
    primary_display->desktop_window = desktop_window;
    
    kapi_log_info("Window manager initialized successfully");
    return 0;
}

void kapi_window_manager_cleanup(void) {
    kapi_log_info("Cleaning up window manager...");
    
    /* Destroy all windows */
    kapi_window_t* window = window_list;
    while (window) {
        kapi_window_t* next = window->next;
        kapi_window_destroy(window);
        window = next;
    }
    
    /* Destroy displays */
    kapi_display_cleanup();
    
    /* Free window manager */
    if (window_manager) {
        kapi_free(window_manager);
        window_manager = NULL;
    }
    
    kapi_log_info("Window manager cleanup completed");
}

int kapi_window_manager_update(void) {
    /* Update window manager state */
    if (!window_manager) {
        return -1;
    }
    
    /* Update displays */
    kapi_display_update_all();
    
    /* Process input events */
    kapi_input_manager_update();
    
    /* Update all windows */
    kapi_window_t* window = window_list;
    while (window) {
        if (window->ops && window->ops->update) {
            window->ops->update(window);
        }
        window = window->next;
    }
    
    return 0;
}

/* Window creation */
kapi_window_t* kapi_window_create(const char* title, int x, int y, int width, int height,
                                  uint32_t type, uint32_t flags) {
    if (!title || width <= 0 || height <= 0) {
        kapi_log_err("Invalid parameters for window creation");
        return NULL;
    }
    
    /* Allocate window structure */
    kapi_window_t* window = (kapi_window_t*)kapi_malloc(sizeof(kapi_window_t));
    if (!window) {
        kapi_log_err("Failed to allocate window structure");
        return NULL;
    }
    
    /* Initialize window structure */
    memset(window, 0, sizeof(kapi_window_t));
    
    /* Set window properties */
    strncpy(window->properties.title, title, sizeof(window->properties.title) - 1);
    window->properties.title[sizeof(window->properties.title) - 1] = '\0';
    window->properties.type = type;
    window->properties.flags = flags;
    window->properties.bounds = (kapi_rect_t){x, y, width, height};
    window->properties.client_rect = (kapi_rect_t){x, y, width, height};
    window->properties.background_color = (kapi_color_t){240, 240, 240, 255};  /* Light gray */
    window->properties.border_color = (kapi_color_t){100, 100, 100, 255};  /* Dark gray */
    window->properties.border_width = 1;
    window->properties.title_bar_height = 24;
    window->properties.menu_bar_height = 0;
    window->properties.state = KAPI_WINDOW_STATE_NORMAL;
    window->properties.flags |= KAPI_WINDOW_FLAG_VISIBLE;
    
    /* Set window operations */
    window->ops = &default_window_ops;
    
    /* Generate window ID */
    window->id = next_window_id++;
    
    /* Set creation time */
    window->creation_time = kapi_time_get_current();
    
    /* Add to window list */
    if (!window_list) {
        window_list = window;
        window_list->next = NULL;
    } else {
        /* Add to end of list */
        kapi_window_t* current = window_list;
        while (current->next) {
            current = current->next;
        }
        current->next = window;
        window->next = NULL;
    }
    
    /* Increment window count */
    window_count++;
    
    /* Create window surface */
    if (window->ops->create) {
        if (window->ops->create(window, &window->properties) != 0) {
            kapi_log_err("Failed to create window surface");
            /* Remove from list */
            if (window_list == window) {
                window_list = window->next;
            } else {
                kapi_window_t* current = window_list;
                while (current->next != window) {
                    current = current->next;
                }
                current->next = window->next;
            }
            window_count--;
            kapi_free(window);
            return NULL;
        }
    }
    
    kapi_log_info("Created window: %s (ID: %u, %dx%d at %d,%d)", 
                 title, window->id, width, height, x, y);
    
    return window;
}

/* Window destruction */
int kapi_window_destroy(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for destruction");
        return -1;
    }
    
    kapi_log_info("Destroying window: %s (ID: %u)", window->properties.title, window->id);
    
    /* Destroy window surface */
    if (window->ops && window->ops->destroy) {
        window->ops->destroy(window);
    }
    
    /* Remove from window list */
    if (window_list == window) {
        window_list = window->next;
    } else {
        kapi_window_t* current = window_list;
        while (current->next != window) {
            current = current->next;
        }
        current->next = window->next;
    }
    
    /* Decrement window count */
    window_count--;
    
    /* Free window structure */
    kapi_free(window);
    
    return 0;
}

/* Window lookup */
kapi_window_t* kapi_window_find(uint32_t window_id) {
    kapi_window_t* window = window_list;
    while (window) {
        if (window->id == window_id) {
            return window;
        }
        window = window->next;
    }
    return NULL;
}

kapi_window_t* kapi_window_find_by_title(const char* title) {
    if (!title) {
        return NULL;
    }
    
    kapi_window_t* window = window_list;
    while (window) {
        if (strcmp(window->properties.title, title) == 0) {
            return window;
        }
        window = window->next;
    }
    return NULL;
}

/* Window visibility operations */
int kapi_window_show(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for show operation");
        return -1;
    }
    
    if (window->ops && window->ops->show) {
        return window->ops->show(window);
    }
    
    return -1;
}

int kapi_window_hide(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for hide operation");
        return -1;
    }
    
    if (window->ops && window->ops->hide) {
        return window->ops->hide(window);
    }
    
    return -1;
}

/* Window geometry operations */
int kapi_window_move(kapi_window_t* window, int32_t x, int32_t y) {
    if (!window) {
        kapi_log_err("Invalid window for move operation");
        return -1;
    }
    
    if (window->ops && window->ops->move) {
        return window->ops->move(window, x, y);
    }
    
    return -1;
}

int kapi_window_resize(kapi_window_t* window, int32_t width, int32_t height) {
    if (!window || width <= 0 || height <= 0) {
        kapi_log_err("Invalid window for resize operation");
        return -1;
    }
    
    if (window->ops && window->ops->resize) {
        return window->ops->resize(window, width, height);
    }
    
    return -1;
}

/* Window state operations */
int kapi_window_minimize(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for minimize operation");
        return -1;
    }
    
    if (window->ops && window->ops->minimize) {
        return window->ops->minimize(window);
    }
    
    return -1;
}

int kapi_window_maximize(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for maximize operation");
        return -1;
    }
    
    if (window->ops && window->ops->maximize) {
        return window->ops->maximize(window);
    }
    
    return -1;
}

int kapi_window_restore(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for restore operation");
        return -1;
    }
    
    if (window->ops && window->ops->restore) {
        return window->ops->restore(window);
    }
    
    return -1;
}

/* Window focus operations */
int kapi_window_set_focus(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for focus operation");
        return -1;
    }
    
    if (window->ops && window->ops->set_focus) {
        return window->ops->set_focus(window);
    }
    
    return -1;
}

int kapi_window_remove_focus(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for focus removal operation");
        return -1;
    }
    
    if (window->ops && window->ops->remove_focus) {
        return window->ops->remove_focus(window);
    }
    
    return -1;
}

/* Window Z-order operations */
int kapi_window_bring_to_front(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for bring-to-front operation");
        return -1;
    }
    
    if (window->ops && window->ops->bring_to_front) {
        return window->ops->bring_to_front(window);
    }
    
    return -1;
}

int kapi_window_send_to_back(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for send-to-back operation");
        return -1;
    }
    
    if (window->ops && window->ops->send_to_back) {
        return window->ops->send_to_back(window);
    }
    
    return -1;
}

/* Window appearance operations */
int kapi_window_set_always_on_top(kapi_window_t* window, int topmost) {
    if (!window) {
        kapi_log_err("Invalid window for always-on-top operation");
        return -1;
    }
    
    if (window->ops && window->ops->set_always_on_top) {
        return window->ops->set_always_on_top(window, topmost);
    }
    
    return -1;
}

int kapi_window_set_transparency(kapi_window_t* window, uint8_t alpha) {
    if (!window) {
        kapi_log_err("Invalid window for transparency operation");
        return -1;
    }
    
    if (window->ops && window->ops->set_transparency) {
        return window->ops->set_transparency(window, alpha);
    }
    
    return -1;
}

/* Window property operations */
int kapi_window_set_title(kapi_window_t* window, const char* title) {
    if (!window || !title) {
        kapi_log_err("Invalid parameters for window title operation");
        return -1;
    }
    
    if (window->ops && window->ops->set_title) {
        return window->ops->set_title(window, title);
    }
    
    return -1;
}

int kapi_window_set_icon(kapi_window_t* window, const char* icon_path) {
    if (!window || !icon_path) {
        kapi_log_err("Invalid parameters for window icon operation");
        return -1;
    }
    
    if (window->ops && window->ops->set_icon) {
        return window->ops->set_icon(window, icon_path);
    }
    
    return -1;
}

int kapi_window_set_background_color(kapi_window_t* window, const kapi_color_t* color) {
    if (!window || !color) {
        kapi_log_err("Invalid parameters for window background color operation");
        return -1;
    }
    
    if (window->ops && window->ops->set_background_color) {
        return window->ops->set_background_color(window, color);
    }
    
    return -1;
}

/* Window surface operations */
kapi_surface_t* kapi_window_get_surface(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for surface operation");
        return NULL;
    }
    
    if (window->ops && window->ops->get_surface) {
        return window->ops->get_surface(window);
    }
    
    return NULL;
}

/* Window hierarchy operations */
int kapi_window_set_parent(kapi_window_t* window, kapi_window_t* parent) {
    if (!window) {
        kapi_log_err("Invalid window for parent operation");
        return -1;
    }
    
    /* Remove from current parent's children */
    if (window->parent) {
        kapi_window_t* child = window->parent->first_child;
        if (child == window) {
            window->parent->first_child = window->next;
        } else {
            while (child && child->next != window) {
                child = child->next;
            }
            if (child) {
                child->next = window->next;
            }
        }
    }
    
    /* Set new parent */
    window->parent = parent;
    window->next = NULL;
    
    /* Add to new parent's children */
    if (parent) {
        if (!parent->first_child) {
            parent->first_child = window;
            parent->last_child = window;
        } else {
            parent->last_child->next = window;
            parent->last_child = window;
        }
    }
    
    kapi_log_info("Set parent for window %u to %u", window->id, 
                 parent ? parent->id : 0);
    
    return 0;
}

int kapi_window_add_child(kapi_window_t* parent, kapi_window_t* child) {
    if (!parent || !child) {
        kapi_log_err("Invalid parameters for child window addition");
        return -1;
    }
    
    /* Check if child already has a parent */
    if (child->parent) {
        kapi_log_warn("Window %u already has parent %u", child->id, child->parent->id);
        return kapi_window_set_parent(child, parent);
    }
    
    /* Add child to parent */
    child->parent = parent;
    child->next = NULL;
    
    if (!parent->first_child) {
        parent->first_child = child;
        parent->last_child = child;
    } else {
        parent->last_child->next = child;
        parent->last_child = child;
    }
    
    kapi_log_info("Added child window %u to parent %u", child->id, parent->id);
    
    return 0;
}

int kapi_window_remove_child(kapi_window_t* parent, kapi_window_t* child) {
    if (!parent || !child) {
        kapi_log_err("Invalid parameters for child window removal");
        return -1;
    }
    
    if (child->parent != parent) {
        kapi_log_err("Window %u is not a child of %u", child->id, parent->id);
        return -1;
    }
    
    /* Remove from parent's children */
    if (parent->first_child == child) {
        parent->first_child = child->next;
        if (parent->last_child == child) {
            parent->last_child = NULL;
        }
    } else {
        kapi_window_t* current = parent->first_child;
        while (current->next != child) {
            current = current->next;
        }
        current->next = child->next;
        if (parent->last_child == child) {
            parent->last_child = current;
        }
    }
    
    /* Clear child's parent */
    child->parent = NULL;
    child->next = NULL;
    
    kapi_log_info("Removed child window %u from parent %u", child->id, parent->id);
    
    return 0;
}

/* Display operations */
kapi_display_t* kapi_display_get_primary(void) {
    return primary_display;
}

kapi_display_t* kapi_display_get(uint32_t display_id) {
    kapi_display_t* display = display_list;
    while (display) {
        if (display->id == display_id) {
            return display;
        }
        display = display->next;
    }
    return NULL;
}

int kapi_display_get_count(uint32_t* count) {
    if (!count) {
        return -1;
    }
    
    *count = 0;
    kapi_display_t* display = display_list;
    while (display) {
        (*count)++;
        display = display->next;
    }
    
    return 0;
}

int kapi_display_update_all(void) {
    kapi_display_t* display = display_list;
    while (display) {
        if (display->ops && display->ops->update) {
            display->ops->update(display);
        }
        display = display->next;
    }
    return 0;
}

/* Window debugging */
int kapi_window_dump_hierarchy(kapi_window_t* window) {
    if (!window) {
        kapi_log_err("Invalid window for hierarchy dump");
        return -1;
    }
    
    kapi_log_info("Window hierarchy dump for %u (%s):", window->id, window->properties.title);
    kapi_log_info("  Bounds: %d,%d %dx%d", window->properties.bounds.x, window->properties.bounds.y,
                 window->properties.bounds.width, window->properties.bounds.height);
    kapi_log_info("  Client: %d,%d %dx%d", window->properties.client_rect.x, window->properties.client_rect.y,
                 window->properties.client_rect.width, window->properties.client_rect.height);
    kapi_log_info("  State: %u, Flags: %u", window->properties.state, window->properties.flags);
    
    /* Dump children */
    if (window->first_child) {
        kapi_log_info("  Children:");
        kapi_window_t* child = window->first_child;
        while (child) {
            kapi_log_info("    %u: %s", child->id, child->properties.title);
            child = child->next;
        }
    }
    
    return 0;
}

int kapi_window_dump_all(void) {
    kapi_log_info("Dumping all windows:");
    
    kapi_window_t* window = window_list;
    while (window) {
        kapi_window_dump_hierarchy(window);
        window = window->next;
    }
    
    return 0;
}

/* Default window operations implementation */
static int window_create_impl(kapi_window_t* window, const kapi_window_properties_t* props) {
    /* Create window surface */
    kapi_surface_t* surface = NULL;
    if (kapi_surface_create(&surface, props->bounds.width, props->bounds.height,
                           KAPI_SURFACE_FORMAT_ARGB8888, NULL) != 0) {
        kapi_log_err("Failed to create window surface");
        return -1;
    }
    
    window->properties.surface = surface;
    return 0;
}

static int window_destroy_impl(kapi_window_t* window) {
    if (window->properties.surface) {
        kapi_surface_destroy(window->properties.surface);
        window->properties.surface = NULL;
    }
    return 0;
}

static int window_show_impl(kapi_window_t* window) {
    window->properties.flags |= KAPI_WINDOW_FLAG_VISIBLE;
    kapi_log_info("Showing window %u", window->id);
    return 0;
}

static int window_hide_impl(kapi_window_t* window) {
    window->properties.flags &= ~KAPI_WINDOW_FLAG_VISIBLE;
    kapi_log_info("Hiding window %u", window->id);
    return 0;
}

static int window_move_impl(kapi_window_t* window, int32_t x, int32_t y) {
    window->properties.bounds.x = x;
    window->properties.bounds.y = y;
    window->properties.client_rect.x = x + window->properties.border_width;
    window->properties.client_rect.y = y + window->properties.border_width + window->properties.title_bar_height;
    kapi_log_info("Moving window %u to %d,%d", window->id, x, y);
    return 0;
}

static int window_resize_impl(kapi_window_t* window, int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) {
        return -1;
    }
    
    window->properties.bounds.width = width;
    window->properties.bounds.height = height;
    window->properties.client_rect.width = width - 2 * window->properties.border_width;
    window->properties.client_rect.height = height - 2 * window->properties.border_width - window->properties.title_bar_height;
    
    /* Resize surface if it exists */
    if (window->properties.surface) {
        kapi_surface_resize(window->properties.surface, width, height);
    }
    
    kapi_log_info("Resizing window %u to %dx%d", window->id, width, height);
    return 0;
}

static int window_minimize_impl(kapi_window_t* window) {
    window->properties.state = KAPI_WINDOW_STATE_MINIMIZED;
    window->properties.flags &= ~KAPI_WINDOW_FLAG_VISIBLE;
    kapi_log_info("Minimizing window %u", window->id);
    return 0;
}

static int window_maximize_impl(kapi_window_t* window) {
    window->properties.state = KAPI_WINDOW_STATE_MAXIMIZED;
    kapi_log_info("Maximizing window %u", window->id);
    return 0;
}

static int window_restore_impl(kapi_window_t* window) {
    window->properties.state = KAPI_WINDOW_STATE_NORMAL;
    window->properties.flags |= KAPI_WINDOW_FLAG_VISIBLE;
    kapi_log_info("Restoring window %u", window->id);
    return 0;
}

static int window_set_focus_impl(kapi_window_t* window) {
    window->properties.flags |= KAPI_WINDOW_FLAG_FOCUSED;
    kapi_log_info("Setting focus to window %u", window->id);
    return 0;
}

static int window_remove_focus_impl(kapi_window_t* window) {
    window->properties.flags &= ~KAPI_WINDOW_FLAG_FOCUSED;
    kapi_log_info("Removing focus from window %u", window->id);
    return 0;
}

static int window_bring_to_front_impl(kapi_window_t* window) {
    kapi_log_info("Bringing window %u to front", window->id);
    return 0;
}

static int window_send_to_back_impl(kapi_window_t* window) {
    kapi_log_info("Sending window %u to back", window->id);
    return 0;
}

static int window_set_always_on_top_impl(kapi_window_t* window, int topmost) {
    if (topmost) {
        window->properties.flags |= KAPI_WINDOW_FLAG_TOPMOST;
    } else {
        window->properties.flags &= ~KAPI_WINDOW_FLAG_TOPMOST;
    }
    kapi_log_info("Setting window %u always-on-top: %d", window->id, topmost);
    return 0;
}

static int window_set_transparency_impl(kapi_window_t* window, uint8_t alpha) {
    window->properties.background_color.a = alpha;
    kapi_log_info("Setting window %u transparency: %d", window->id, alpha);
    return 0;
}

static kapi_surface_t* window_get_surface_impl(kapi_window_t* window) {
    return window->properties.surface;
}

static int window_set_title_impl(kapi_window_t* window, const char* title) {
    strncpy(window->properties.title, title, sizeof(window->properties.title) - 1);
    window->properties.title[sizeof(window->properties.title) - 1] = '\0';
    kapi_log_info("Setting title for window %u: %s", window->id, title);
    return 0;
}

static int window_set_icon_impl(kapi_window_t* window, const char* icon_path) {
    kapi_log_info("Setting icon for window %u: %s", window->id, icon_path);
    return 0;
}

static int window_set_background_color_impl(kapi_window_t* window, const kapi_color_t* color) {
    window->properties.background_color = *color;
    kapi_log_info("Setting background color for window %u: %d,%d,%d,%d", 
                 window->id, color->r, color->g, color->b, color->a);
    return 0;
}