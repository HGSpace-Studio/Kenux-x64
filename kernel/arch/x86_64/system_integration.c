#include <kapi.h>
#include <kapi_process.h>
#include <kapi_memory.h>
#include <kapi_fs_ext.h>
#include <kapi_net_ext.h>
#include <kapi_device_ext.h>
#include <kapi_sync_ext.h>
#include <kapi_sysinfo.h>
#include <kapi_security_ext.h>
#include <kapi_virt_ext.h>

#include "memory_optimized.h"
#include "scheduler_enhanced.h"
#include "interrupt_enhanced.h"
#include "display_enhanced.h"
#include "stardustui_integration.h"
#include "filesystem_enhanced.h"
#include "network_enhanced.h"

typedef struct {
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    char build_id[64];
    uint64_t init_time;
    uint64_t boot_time;
    char hostname[256];
    char domain[256];
} system_info_t;

typedef struct {
    bool memory_initialized;
    bool scheduler_initialized;
    bool interrupt_initialized;
    bool display_initialized;
    bool ui_initialized;
    bool filesystem_initialized;
    bool network_initialized;
    bool security_initialized;
    bool virtualization_initialized;
} subsystem_status_t;

static system_info_t sys_info;
static subsystem_status_t sub_status;
static spinlock_t integration_lock;
static bool system_ready = false;

void system_integration_init(void)
{
    if (system_ready) return;

    spin_init(&integration_lock);
    memset(&sys_info, 0, sizeof(system_info_t));
    memset(&sub_status, 0, sizeof(subsystem_status_t));

    sys_info.version_major = KAPI_VERSION_MAJOR;
    sys_info.version_minor = KAPI_VERSION_MINOR;
    sys_info.version_patch = KAPI_VERSION_PATCH;
    strncpy(sys_info.build_id, "Kenux-Enhanced-2.0", 63);
    sys_info.init_time = get_current_time_ns();

    spin_lock(&integration_lock);

    memory_init();
    sub_status.memory_initialized = true;

    scheduler_init(get_cpu_count());
    sub_status.scheduler_initialized = true;

    interrupt_init(get_cpu_count());
    sub_status.interrupt_initialized = true;

    display_init(1920, 1080, 32);
    sub_status.display_initialized = true;

    stardustui_init();
    sub_status.ui_initialized = true;

    filesystem_init();
    sub_status.filesystem_initialized = true;

    network_init();
    sub_status.network_initialized = true;

    security_init();
    sub_status.security_initialized = true;

    virt_init();
    sub_status.virtualization_initialized = true;

    sys_info.boot_time = get_current_time_ns();

    spin_unlock(&integration_lock);

    system_ready = true;
}

bool is_system_ready(void)
{
    return system_ready && sub_status.memory_initialized &&
           sub_status.scheduler_initialized && sub_status.interrupt_initialized &&
           sub_status.display_initialized && sub_status.ui_initialized &&
           sub_status.filesystem_initialized && sub_status.network_initialized &&
           sub_status.security_initialized && sub_status.virtualization_initialized;
}

subsystem_status_t* get_subsystem_status(void)
{
    return &sub_status;
}

system_info_t* get_system_info(void)
{
    return &sys_info;
}

int system_shutdown(int mode)
{
    if (!system_ready) return -EINVAL;

    switch (mode) {
        case SHUTDOWN_POWER_OFF:
            network_shutdown();
            filesystem_sync();
            display_shutdown();
            stardustui_shutdown();
            interrupt_shutdown();
            scheduler_shutdown();
            memory_cleanup();

            acpi_power_off();
            break;

        case SHUTDOWN_REBOOT:
            network_shutdown();
            filesystem_sync();
            display_shutdown();
            stardustui_shutdown();
            interrupt_shutdown();
            scheduler_shutdown();
            memory_cleanup();

            acpi_reset();
            break;

        case SHUTDOWN_HIBERNATE:
            filesystem_sync();
            memory_save_state();
            acpi_hibernate();
            break;

        case SHUTDOWN_SUSPEND:
            display_suspend();
            network_suspend();
            acpi_suspend();
            break;

        default:
            return -EINVAL;
    }

    system_ready = false;
    memset(&sub_status, 0, sizeof(subsystem_status_t));
    return 0;
}

int system_suspend_resume(void)
{
    if (!system_ready) return -EINVAL;

    acpi_resume();

    network_resume();
    display_resume();
    stardustui_resume();

    scheduler_rebalance();
    interrupt_reinit();

    return 0;
}

uint64_t get_system_uptime(void)
{
    if (!system_ready) return 0;
    return get_current_time_ns() - sys_info.boot_time;
}

float get_system_load_average(int minutes)
{
    if (!system_ready) return 0.0f;

    scheduler_stats_t* stats = scheduler_get_stats();
    if (!stats) return 0.0f;

    switch (minutes) {
        case 1:
            return stats->load_avg_1min;
        case 5:
            return stats->load_avg_5min;
        case 15:
            return stats->load_avg_15min;
        default:
            return 0.0f;
    }
}

memory_stats_t* get_memory_statistics(void)
{
    if (!sub_status.memory_initialized) return NULL;
    return memory_get_stats();
}

scheduler_stats_t* get_scheduler_statistics(void)
{
    if (!sub_status.scheduler_initialized) return NULL;
    return scheduler_get_stats();
}

interrupt_stats_t* get_interrupt_statistics(int cpu)
{
    if (!sub_status.interrupt_initialized) return NULL;
    return interrupt_get_stats(cpu);
}

display_stats_t* get_display_statistics(void)
{
    if (!sub_status.display_initialized) return NULL;
    return display_get_stats();
}

filesystem_stats_t* get_filesystem_statistics(void)
{
    if (!sub_status.filesystem_initialized) return NULL;
    return vfs_get_stats();
}

network_stats_t* get_network_statistics(void)
{
    if (!sub_status.network_initialized) return NULL;
    return net_get_stats();
}

int set_hostname(const char* name)
{
    if (!name || !system_ready) return -EINVAL;

    size_t len = strlen(name);
    if (len == 0 || len >= 255) return -EINVAL;

    spin_lock(&integration_lock);
    strncpy(sys_info.hostname, name, 255);
    sys_info.hostname[255] = '\0';
    spin_unlock(&integration_lock);

    return 0;
}

const char* get_hostname(void)
{
    if (!system_ready) return NULL;
    return sys_info.hostname;
}

int set_domain(const char* domain)
{
    if (!domain || !system_ready) return -EINVAL;

    size_t len = strlen(domain);
    if (len >= 255) return -EINVAL;

    spin_lock(&integration_lock);
    strncpy(sys_info.domain, domain, 255);
    sys_info.domain[255] = '\0';
    spin_unlock(&integration_lock);

    return 0;
}

const char* get_domain(void)
{
    if (!system_ready) return NULL;
    return sys_info.domain;
}

int system_create_process(const char* path, const char** argv, const char** envp,
                          process_flags_t flags, process_priority_t priority)
{
    if (!path || !system_ready) return -EINVAL;

    int pid = process_create(path, argv, envp);
    if (pid < 0) return pid;

    if (flags & PROCESS_FLAG_HIGH_PRIORITY) {
        process_set_priority(pid, PRIORITY_HIGH);
    } else if (flags & PROCESS_FLAG_LOW_PRIORITY) {
        process_set_priority(pid, PRIORITY_LOW);
    } else {
        process_set_priority(pid, priority);
    }

    if (flags & PROCESS_FLAG_REALTIME) {
        process_set_scheduler(pid, SCHED_FIFO);
    }

    if (!(flags & PROCESS_FLAG_CREATE_SUSPENDED)) {
        process_resume(pid);
    }

    return pid;
}

int system_create_thread(process_func_t func, void* arg, thread_flags_t flags,
                         thread_priority_t priority)
{
    if (!func || !system_ready) return -EINVAL;

    int tid = thread_create(func, arg);
    if (tid < 0) return tid;

    if (flags & THREAD_FLAG_AFFINITY_CPU0) {
        thread_set_affinity(tid, 0x1);
    } else if (flags & THREAD_FLAG_AFFINITY_CPU1) {
        thread_set_affinity(tid, 0x2);
    }

    thread_set_priority(tid, priority);

    if (!(flags & THREAD_FLAG_CREATE_SUSPENDED)) {
        thread_resume(tid);
    }

    return tid;
}

void* system_alloc(size_t size, alloc_flags_t flags)
{
    if (size == 0 || !sub_status.memory_initialized) return NULL;

    void* ptr = NULL;

    if (flags & ALLOC_FLAG_ZEROED) {
        ptr = kzalloc(size);
    } else if (flags & ALLOC_FLAG_DMA) {
        ptr = dma_alloc(size);
    } else if (flags & ALLOC_FLAG_CONTIGUOUS) {
        ptr = alloc_contiguous(size);
    } else {
        ptr = kmalloc(size);
    }

    if (ptr && (flags & ALLOC_FLAG_CACHE_WB)) {
        memory_set_cache_policy(ptr, size, CACHE_WRITE_BACK);
    } else if (ptr && (flags & ALLOC_FLAG_CACHE_WT)) {
        memory_set_cache_policy(ptr, size, CACHE_WRITE_THROUGH);
    } else if (ptr && (flags & ALLOC_FLAG_CACHE_UC)) {
        memory_set_cache_policy(ptr, size, CACHE_UNCACHED);
    }

    return ptr;
}

void system_free(void* ptr)
{
    if (!ptr || !sub_status.memory_initialized) return;
    kfree(ptr);
}

int system_open_file(const char* path, file_mode_t mode, file_flags_t flags)
{
    if (!path || !sub_status.filesystem_initialized) return -EINVAL;

    int fd = vfs_open(path, mode | flags);
    if (fd < 0) return fd;

    if (flags & FILE_FLAG_NONBLOCKING) {
        fcntl(fd, F_SETFL, O_NONBLOCK);
    }

    if (flags & FILE_FLAG_SYNC) {
        fcntl(fd, F_SETFL, O_SYNC);
    }

    return fd;
}

ssize_t system_read_file(int fd, void* buf, size_t count)
{
    if (!buf || count == 0 || !sub_status.filesystem_initialized) return -EINVAL;
    return vfs_read_enhanced(fd, buf, count);
}

ssize_t system_write_file(int fd, const void* buf, size_t count)
{
    if (!buf || count == 0 || !sub_status.filesystem_initialized) return -EINVAL;
    return vfs_write_enhanced(fd, buf, count);
}

int system_close_file(int fd)
{
    if (!sub_status.filesystem_initialized) return -EBADF;
    return vfs_close(fd);
}

int system_create_socket(int domain, int type, int protocol)
{
    if (!sub_status.network_initialized) return -ENOSYS;

    int sockfd = socket(domain, type, protocol);
    if (sockfd < 0) return sockfd;

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    return sockfd;
}

int system_connect_socket(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    if (!addr || !sub_status.network_initialized) return -EINVAL;
    return socket_connect(sockfd, addr, addrlen);
}

int system_bind_socket(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    if (!addr || !sub_status.network_initialized) return -EINVAL;
    return socket_bind(sockfd, addr, addrlen);
}

int system_listen_socket(int sockfd, int backlog)
{
    if (!sub_status.network_initialized) return -ENOSYS;
    return socket_listen(sockfd, backlog);
}

int system_accept_socket(int sockfd, struct sockaddr* addr, socklen_t* addrlen)
{
    if (!sub_status.network_initialized) return -ENOSYS;
    return socket_accept(sockfd, addr, addrlen);
}

ssize_t system_send_socket(int sockfd, const void* buf, size_t len, int flags)
{
    if (!buf || !sub_status.network_initialized) return -EINVAL;
    return socket_send(sockfd, buf, len, flags);
}

ssize_t system_recv_socket(int sockfd, void* buf, size_t len, int flags)
{
    if (!buf || !sub_status.network_initialized) return -EINVAL;
    return socket_recv(sockfd, buf, len, flags);
}

int system_close_socket(int sockfd)
{
    if (!sub_status.network_initialized) return -EBADF;
    return socket_close(sockfd);
}

window_context_t* system_create_window(const char* title, int width, int height,
                                       window_flags_t flags)
{
    if (!title || !sub_status.ui_initialized) return NULL;

    bool resizable = (flags & WINDOW_FLAG_RESIZABLE) != 0;
    bool decorated = !(flags & WINDOW_FLAG_BORDERLESS);

    window_context_t* win = stardustui_create_window(title, width, height, resizable, decorated);
    if (!win) return NULL;

    if (flags & WINDOW_FLAG_ALWAYS_ON_TOP) {
        win->always_on_top = true;
    }

    if (flags & WINDOW_FLAG_MAXIMIZED) {
        stardustui_maximize_window(win);
    }

    if (flags & WINDOW_FLAG_MINIMIZED) {
        stardustui_minimize_window(win);
    }

    if (!(flags & WINDOW_FLAG_HIDDEN)) {
        stardustui_show_window(win);
    }

    return win;
}

button_component_t* system_create_button(window_context_t* win, const char* text,
                                         rect_t bounds, button_callback_t callback)
{
    if (!win || !text || !sub_status.ui_initialized) return NULL;

    button_component_t* btn = stardustui_create_button(text, bounds, callback);
    if (!btn) return NULL;

    stardustui_add_component(win, (component_base_t*)btn);
    return btn;
}

label_component_t* system_create_label(window_context_t* win, const char* text,
                                       rect_t bounds)
{
    if (!win || !text || !sub_status.ui_initialized) return NULL;

    label_component_t* lbl = stardustui_create_label(text, bounds);
    if (!lbl) return NULL;

    stardustui_add_component(win, (component_base_t*)lbl);
    return lbl;
}

textbox_component_t* system_create_textbox(window_context_t* win, rect_t bounds,
                                           textbox_flags_t flags)
{
    if (!win || !sub_status.ui_initialized) return NULL;

    textbox_component_t* tb = stardustui_create_textbox(bounds, flags);
    if (!tb) return NULL;

    stardustui_add_component(win, (component_base_t*)tb);
    return tb;
}

int system_ui_event_loop(void)
{
    if (!sub_status.ui_initialized) return -ENOSYS;
    return stardustui_run_event_loop();
}

void system_invalidate_window(window_context_t* win)
{
    if (!win || !sub_status.ui_initialized) return;
    stardustui_invalidate_window(win);
}

security_context_t* system_create_security_context(security_level_t level)
{
    if (!sub_status.security_initialized) return NULL;
    return security_create_context(level);
}

int system_check_permission(security_context_t* ctx, const char* resource,
                            permission_t perm)
{
    if (!ctx || !resource || !sub_status.security_initialized) return -EINVAL;
    return security_check_permission(ctx, resource, perm);
}

virt_machine_t* system_create_virtual_machine(virt_config_t* config)
{
    if (!config || !sub_status.virtualization_initialized) return NULL;
    return virt_create_vm(config);
}

int system_start_virtual_machine(virt_machine_t* vm)
{
    if (!vm || !sub_status.virtualization_initialized) return -EINVAL;
    return virt_start_vm(vm);
}

int system_stop_virtual_machine(virt_machine_t* vm)
{
    if (!vm || !sub_status.virtualization_initialized) return -EINVAL;
    return virt_stop_vm(vm);
}

void system_debug_print(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    console_puts(buffer);
    log_message(LOG_DEBUG, buffer);

    va_end(args);
}

void system_panic(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    console_puts("\n*** KERNEL PANIC ***\n");
    console_puts(buffer);
    console_puts("\n");

    log_message(LOG_CRITICAL, buffer);

    va_end(args);

    display_show_panic_screen(buffer);

    __asm__ volatile("cli; hlt");
    for (;;);
}

int system_register_driver(device_driver_t* driver)
{
    if (!driver || !sub_status.interrupt_initialized) return -EINVAL;

    int ret = device_register(driver);
    if (ret < 0) return ret;

    if (driver->irq > 0) {
        ret = request_irq(driver->irq, driver->irq_handler,
                         IRQ_FLAG_SHARED, driver->name, driver);
        if (ret < 0) {
            device_unregister(driver);
            return ret;
        }
    }

    return 0;
}

int system_unregister_driver(device_driver_t* driver)
{
    if (!driver) return -EINVAL;

    if (driver->irq > 0) {
        free_irq(driver->irq, driver);
    }

    return device_unregister(driver);
}

timer_t* system_create_timer(uint64_t interval_ns, timer_callback_t callback,
                             void* data, timer_flags_t flags)
{
    if (!callback) return NULL;

    timer_t* timer = kzalloc(sizeof(timer_t));
    if (!timer) return NULL;

    timer->interval_ns = interval_ns;
    timer->callback = callback;
    timer->data = data;
    timer->flags = flags;
    timer->state = TIMER_STATE_STOPPED;

    if (flags & TIMER_FLAG_PERIODIC) {
        timer->remaining = interval_ns;
    } else {
        timer->remaining = interval_ns;
    }

    register_timer(timer);
    return timer;
}

int system_start_timer(timer_t* timer)
{
    if (!timer) return -EINVAL;
    return start_timer(timer);
}

int system_stop_timer(timer_t* timer)
{
    if (!timer) return -EINVAL;
    return stop_timer(timer);
}

void system_destroy_timer(timer_t* timer)
{
    if (!timer) return;
    stop_timer(timer);
    unregister_timer(timer);
    kfree(timer);
}

int system_add_workqueue(workqueue_func_t func, void* data, workqueue_flags_t flags)
{
    if (!func) return -EINVAL;

    work_item_t* item = kzalloc(sizeof(work_item_t));
    if (!item) return -ENOMEM;

    item->func = func;
    item->data = data;
    item->flags = flags;
    item->state = WORK_PENDING;

    if (flags & WORKQUEUE_FLAG_HIGH_PRIORITY) {
        enqueue_work(HIGH_PRIORITY_WQ, item);
    } else if (flags & WORKQUEUE_FLAG_LOW_PRIORITY) {
        enqueue_work(LOW_PRIORITY_WQ, item);
    } else {
        enqueue_work(DEFAULT_WQ, item);
    }

    return 0;
}

void system_flush_workqueue(workqueue_priority_t priority)
{
    flush_workqueue(priority);
}