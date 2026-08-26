#ifndef KENUX_SYSTEM_H
#define KENUX_SYSTEM_H

#include <kapi.h>

typedef struct {
    uint8_t r, g, b, a;
} color_t;

typedef struct {
    int x, y, width, height;
} rect_t;

typedef enum {
    WINDOW_FLAG_NONE           = 0x0000,
    WINDOW_FLAG_RESIZABLE      = 0x0001,
    WINDOW_FLAG_BORDERLESS     = 0x0002,
    WINDOW_FLAG_ALWAYS_ON_TOP  = 0x0004,
    WINDOW_FLAG_MAXIMIZED      = 0x0008,
    WINDOW_FLAG_MINIMIZED      = 0x0010,
    WINDOW_FLAG_HIDDEN         = 0x0020,
    WINDOW_FLAG_MODAL          = 0x0040,
    WINDOW_FLAG_TOOL_WINDOW    = 0x0080
} window_flags_t;

typedef enum {
    PROCESS_FLAG_NONE            = 0x00,
    PROCESS_FLAG_HIGH_PRIORITY   = 0x01,
    PROCESS_FLAG_LOW_PRIORITY    = 0x02,
    PROCESS_FLAG_REALTIME        = 0x04,
    PROCESS_FLAG_CREATE_SUSPENDED = 0x08
} process_flags_t;

typedef enum {
    PRIORITY_IDLE       = 0,
    PRIORITY_LOW        = 1,
    PRIORITY_NORMAL     = 2,
    PRIORITY_HIGH       = 3,
    PRIORITY_REALTIME   = 4,
    PRIORITY_CRITICAL   = 5
} process_priority_t;

typedef enum {
    ALLOC_FLAG_NONE       = 0x00,
    ALLOC_FLAG_ZEROED     = 0x01,
    ALLOC_FLAG_DMA        = 0x02,
    ALLOC_FLAG_CONTIGUOUS = 0x04,
    ALLOC_FLAG_CACHE_WB   = 0x08,
    ALLOC_FLAG_CACHE_WT   = 0x10,
    ALLOC_FLAG_CACHE_UC   = 0x20
} alloc_flags_t;

typedef enum {
    FILE_FLAG_NONE         = 0x00,
    FILE_FLAG_NONBLOCKING  = 0x01,
    FILE_FLAG_SYNC         = 0x02,
    FILE_FLAG_APPEND       = 0x04,
    FILE_FLAG_CREATE       = 0x08,
    FILE_FLAG_TRUNCATE     = 0x10,
    FILE_FLAG_EXCLUSIVE    = 0x20
} file_flags_t;

typedef enum {
    TIMER_FLAG_NONE      = 0x00,
    TIMER_FLAG_PERIODIC  = 0x01,
    TIMER_FLAG_ONESHOT   = 0x02
} timer_flags_t;

typedef enum {
    WORKQUEUE_FLAG_NONE           = 0x00,
    WORKQUEUE_FLAG_HIGH_PRIORITY  = 0x01,
    WORKQUEUE_FLAG_LOW_PRIORITY   = 0x02
} workqueue_flags_t;

typedef enum {
    SHUTDOWN_POWER_OFF = 0,
    SHUTDOWN_REBOOT    = 1,
    SHUTDOWN_HIBERNATE = 2,
    SHUTDOWN_SUSPEND   = 3
} shutdown_mode_t;

typedef struct {
    char title[64];
    int window_id;
    bool is_active;
} taskbar_entry_t;

typedef struct {
    bool present;
    bool connected;
    bool charging;
    uint32_t design_capacity_mwh;
    uint32_t last_full_capacity_mwh;
    uint32_t current_capacity_mwh;
    uint32_t discharge_rate_mw;
    uint16_t voltage_mv;
    uint16_t temperature_k;
    uint32_t cycle_count;
    float health_percent;
} battery_info_t;

typedef void (*process_func_t)(void*);
typedef void (*button_callback_t)(struct button_component*);
typedef void (*timer_callback_t)(struct timer*, void*);
typedef void (*workqueue_func_t)(void*);

typedef enum {
    MOUSE_EVENT_MOVE,
    MOUSE_EVENT_BUTTON_DOWN,
    MOUSE_EVENT_BUTTON_UP,
    MOUSE_EVENT_DOUBLE_CLICK,
    MOUSE_EVENT_SCROLL
} mouse_event_type_t;

typedef enum {
    KEYBOARD_EVENT_KEY_DOWN,
    KEYBOARD_EVENT_KEY_UP,
    KEYBOARD_EVENT_KEY_REPEAT
} keyboard_event_type_t;

typedef struct {
    mouse_event_type_t type;
    int x, y;
    int button;
    int delta;
} mouse_event_t;

typedef struct {
    keyboard_event_type_t type;
    int keycode;
    int modifiers;
    char ascii;
} keyboard_event_t;

#define IMAGE_MAGIC_BMP  0x4D42
#define IMAGE_MAGIC_PNG  0x5089

typedef struct {
    uint16_t magic;
    uint32_t width, height;
    uint16_t bpp;
    uint32_t data_offset;
} image_header_t;

#define ICON_COMPUTER   1001
#define ICON_FOLDER     1002
#define ICON_FILE       1003
#define ICON_TERMINAL   1004
#define ICON_SETTINGS   1005
#define ICON_TRASH      1006
#define ICON_NETWORK    1007

void system_integration_init(void);
bool is_system_ready(void);
int system_shutdown(int mode);
int system_suspend_resume(void);
uint64_t get_system_uptime(void);
float get_system_load_average(int minutes);

memory_stats_t* get_memory_statistics(void);
scheduler_stats_t* get_scheduler_statistics(void);
interrupt_stats_t* get_interrupt_statistics(int cpu);
display_stats_t* get_display_statistics(void);
filesystem_stats_t* get_filesystem_statistics(void);
network_stats_t* get_network_statistics(void);

int set_hostname(const char* name);
const char* get_hostname(void);
int set_domain(const char* domain);
const char* get_domain(void);

int system_create_process(const char* path, const char** argv, const char** envp,
                          process_flags_t flags, process_priority_t priority);
int system_create_thread(process_func_t func, void* arg, thread_flags_t flags,
                         thread_priority_t priority);

void* system_alloc(size_t size, alloc_flags_t flags);
void system_free(void* ptr);

int system_open_file(const char* path, file_mode_t mode, file_flags_t flags);
ssize_t system_read_file(int fd, void* buf, size_t count);
ssize_t system_write_file(int fd, const void* buf, size_t count);
int system_close_file(int fd);

int system_create_socket(int domain, int type, int protocol);
int system_connect_socket(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int system_bind_socket(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int system_listen_socket(int sockfd, int backlog);
int system_accept_socket(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
ssize_t system_send_socket(int sockfd, const void* buf, size_t len, int flags);
ssize_t system_recv_socket(int sockfd, void* buf, size_t len, int flags);
int system_close_socket(int sockfd);

window_context_t* system_create_window(const char* title, int width, int height,
                                       window_flags_t flags);
button_component_t* system_create_button(window_context_t* win, const char* text,
                                         rect_t bounds, button_callback_t callback);
label_component_t* system_create_label(window_context_t* win, const char* text,
                                       rect_t bounds);
textbox_component_t* system_create_textbox(window_context_t* win, rect_t bounds,
                                           textbox_flags_t flags);
int system_ui_event_loop(void);
void system_invalidate_window(window_context_t* win);

security_context_t* system_create_security_context(security_level_t level);
int system_check_permission(security_context_t* ctx, const char* resource,
                            permission_t perm);

virt_machine_t* system_create_virtual_machine(virt_config_t* config);
int system_start_virtual_machine(virt_machine_t* vm);
int system_stop_virtual_machine(virt_machine_t* vm);

timer_t* system_create_timer(uint64_t interval_ns, timer_callback_t callback,
                             void* data, timer_flags_t flags);
int system_start_timer(timer_t* timer);
int system_stop_timer(timer_t* timer);
void system_destroy_timer(timer_t* timer);

int system_add_workqueue(workqueue_func_t func, void* data, workqueue_flags_t flags);
void system_flush_workqueue(workqueue_priority_t priority);

int system_register_driver(device_driver_t* driver);
int system_unregister_driver(device_driver_t* driver);

void system_debug_print(const char* fmt, ...);
void system_panic(const char* fmt, ...);

void perf_init(void);
int perf_alloc_counter(perf_event_type_t type, const char* name, int cpu_id, pid_t pid);
int perf_free_counter(int counter_id);
int perf_reset_counter(int counter_id);
int perf_read_counter(int counter_id, uint64_t* value);
int perf_start_counter(int counter_id);
int perf_stop_counter(int counter_id);
performance_sample_t* perf_get_latest_sample(void);
float perf_get_cpu_usage_avg(int seconds);
float perf_get_memory_usage_avg(int seconds);
int perf_generate_report(char* buffer, size_t size);
void perf_cleanup(void);

int power_init(void);
void enable_acpi_mode(void);
int set_power_state(power_state_t state);
int enter_cpu_idle_state(cpu_state_t state);
int acpi_power_off(void);
int acpi_reset(void);
int acpi_suspend(void);
int acpi_hibernate(void);
power_state_t get_current_power_state(void);
cpu_state_t get_current_cpu_state(void);
uint64_t get_total_idle_time(void);
uint64_t get_total_wakeups(void);
float get_cpu_idle_percent(void);
int set_wake_timer(uint64_t time_ns);
int clear_wake_status(void);
bool is_battery_powered(void);
int get_battery_info(battery_info_t* info);
int set_screen_brightness(int percent);
int get_screen_brightness(void);
void power_cleanup(void);

void desktop_init(int screen_width, int screen_height);
int desktop_create_managed_window(const char* title, int width, int height,
                                   window_flags_t flags);
int desktop_close_window(int window_idx);
int desktop_focus_window(int window_idx);
int desktop_minimize_window(int window_idx);
int desktop_maximize_window(int window_idx);
void desktop_handle_mouse_event(mouse_event_t* event);
void desktop_handle_keyboard_event(keyboard_event_t* event);
void desktop_render(void);
int desktop_set_wallpaper(const char* image_path);
void desktop_set_wallpaper_color(color_t color);
int desktop_add_icon(const char* label, const char* command, uint32_t icon_id);
void desktop_remove_icon(int icon_idx);
void desktop_set_show_icons(bool show);
void desktop_set_theme(theme_t* theme);
theme_t* desktop_get_theme(void);
int desktop_get_window_count(void);
managed_window_t* desktop_get_window(int index);
int desktop_get_active_window(void);
void desktop_cleanup(void);

void boot_init(void);
void log_boot_phase_start(boot_phase_t phase, const char* name);
void log_boot_phase_end(bool success, const char* error_msg);
int perform_boot_sequence(void);
char* generate_boot_report(char* buffer, size_t size);
bool is_boot_complete(void);
uint64_t get_total_boot_time(void);
int get_boot_phase_count(void);
boot_log_entry_t* get_boot_phase(int index);
void reboot_system(void);
void shutdown_system(void);
void emergency_shutdown(const char* reason);
void sync_filesystems(void);
void stop_all_services(void);

#endif