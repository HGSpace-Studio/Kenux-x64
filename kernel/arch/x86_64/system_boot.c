#include <kapi.h>
#include "system_integration.h"
#include "performance_monitor.c"
#include "power_management.c"
#include "desktop_manager.c"

#define BOOT_LOG_MAX_ENTRIES 256
#define BOOT_PHASE_COUNT    8

typedef enum {
    BOOT_PHASE_EFI,
    BOOT_PHASE_KERNEL,
    BOOT_PHASE_MEMORY,
    BOOT_PHASE_CPU,
    BOOT_PHASE_DEVICES,
    BOOT_PHASE_FILESYSTEM,
    BOOT_NETWORK,
    BOOT_PHASE_DESKTOP
} boot_phase_t;

typedef struct {
    boot_phase_t phase;
    const char* phase_name;
    uint64_t start_time;
    uint64_t end_time;
    bool completed;
    bool success;
    char error_message[256];
} boot_log_entry_t;

typedef struct {
    boot_log_entry_t entries[BOOT_LOG_MAX_ENTRIES];
    int entry_count;
    int current_phase;
    spinlock_t lock;
    bool initialized;
    bool boot_complete;
    uint64_t total_boot_time;
} boot_tracker_t;

static boot_tracker_t boot_track;

void boot_init(void)
{
    if (boot_track.initialized) return;

    spin_init(&boot_track.lock);
    memset(&boot_track, sizeof(boot_tracker_t), 0);

    boot_track.entry_count = 0;
    boot_track.current_phase = BOOT_PHASE_EFI;
    boot_track.boot_complete = false;

    log_boot_phase_start(BOOT_PHASE_EFI, "EFI Initialization");

    boot_track.initialized = true;
}

void log_boot_phase_start(boot_phase_t phase, const char* name)
{
    if (!boot_track.initialized || boot_track.entry_count >= BOOT_LOG_MAX_ENTRIES) return;

    spin_lock(&boot_track.lock);

    boot_log_entry_t* entry = &boot_track.entries[boot_track.entry_count++];
    entry->phase = phase;
    entry->phase_name = name;
    entry->start_time = get_current_time_ns();
    entry->end_time = 0;
    entry->completed = false;
    entry->success = false;
    entry->error_message[0] = '\0';

    boot_track.current_phase = phase;

    console_puts("[BOOT] Starting: ");
    console_puts(name);
    console_puts("\n");

    spin_unlock(&boot_track.lock);
}

void log_boot_phase_end(bool success, const char* error_msg)
{
    if (!boot_track.initialized || boot_track.entry_count == 0) return;

    spin_lock(&boot_track.lock);

    boot_log_entry_t* entry = &boot_track.entries[boot_track.entry_count - 1];
    entry->end_time = get_current_time_ns();
    entry->completed = true;
    entry->success = success;

    if (error_msg && !success) {
        strncpy(entry->error_message, error_msg, 255);
        console_puts("[BOOT] ERROR: ");
        console_puts(error_msg);
        console_puts("\n");
    } else {
        uint64_t duration_ns = entry->end_time - entry->start_time;
        float duration_ms = (float)(duration_ns / 1000000.0);

        console_puts("[BOOT] Completed: ");
        console_puts(entry->phase_name);
        console_puts(" (");
        print_float(duration_ms);
        console_puts(" ms)\n");
    }

    spin_unlock(&boot_track.lock);
}

int perform_boot_sequence(void)
{
    if (!boot_track.initialized) return -EINVAL;

    int ret;

    log_boot_phase_start(BOOT_PHASE_KERNEL, "Kernel Initialization");

    cpu_detect_features();
    gdt_init();
    idt_init();

    log_boot_phase_end(true, NULL);

    log_boot_phase_start(BOOT_PHASE_MEMORY, "Memory Management Init");

    ret = memory_init();
    if (ret < 0) {
        log_boot_phase_end(false, "Memory initialization failed");
        return ret;
    }

    init_kernel_heap(PAGE_SIZE * 256);

    log_boot_phase_end(true, NULL);

    log_boot_phase_start(BOOT_PHASE_CPU, "CPU and Scheduler Setup");

    smp_init();
    scheduler_init(get_cpu_count());

    ret = interrupt_init(get_cpu_count());
    if (ret < 0) {
        log_boot_phase_end(false, "Interrupt initialization failed");
        return ret;
    }

    enable_interrupts();

    log_boot_phase_end(true, NULL);

    log_boot_phase_start(BOOT_PHASE_DEVICES, "Device Detection and Init");

    pci_enumerate_devices();
    acpi_init();
    power_init();

    ret = display_init(1920, 1080, 32);
    if (ret < 0) {
        log_boot_phase_end(false, "Display initialization failed");
        console_puts("[BOOT] Warning: Using fallback text mode\n");
    } else {
        log_boot_phase_end(true, NULL);
    }

    keyboard_init();
    mouse_init();
    timer_init(1000);

    log_boot_phase_start(BOOT_PHASE_FILESYSTEM, "File System Mount");

    ret = filesystem_init();
    if (ret < 0) {
        log_boot_phase_end(false, "File system initialization failed");
        return ret;
    }

    vfs_mount("/", "ramfs", NULL);
    vfs_mount("/dev", "devfs", NULL);
    vfs_mount("/proc", "procfs", NULL);
    vfs_mount("/sys", "sysfs", NULL);

    ret = detect_and_mount_root_fs();
    if (ret < 0 && ret != -ENOENT) {
        log_boot_phase_end(false, "Root file system mount failed");
        return ret;
    }

    log_boot_phase_end(true, NULL);

    log_boot_phase_start(BOOT_PHASE_NETWORK, "Network Stack Start");

    ret = network_init();
    if (ret < 0) {
        log_boot_phase_end(false, "Network initialization failed");
        console_puts("[BOOT] Warning: Network not available\n");
    } else {
        detect_network_interfaces();
        configure_dhcp();
        log_boot_phase_end(true, NULL);
    }

    security_init();
    virt_init();

    log_boot_phase_start(BOOT_PHASE_DESKTOP, "Desktop Environment Start");

    stardustui_init();
    desktop_init(1920, 1080 - 48);

    system_integration_init();
    perf_init();

    start_system_services();

    log_boot_phase_end(true, NULL);

    boot_track.boot_complete = true;
    boot_track.total_boot_time = get_current_time_ns() -
                                 boot_track.entries[0].start_time;

    float boot_time_ms = (float)(boot_track.total_boot_time / 1000000.0);

    console_puts("\n========================================\n");
    console_puts("  Kenux OS Boot Complete\n");
    console_puts("  Boot time: ");
    print_float(boot_time_ms);
    console_puts(" ms\n");
    console_puts("========================================\n\n");

    show_boot_splash_screen();

    return 0;
}

int detect_and_mount_root_fs(void)
{
    const char* root_candidates[] = {
        "/dev/sda1",
        "/dev/nvme0n1p1",
        "/dev/hda1",
        NULL
    };

    for (int i = 0; root_candidates[i]; i++) {
        int fd = vfs_open(root_candidates[i], O_RDONLY);
        if (fd >= 0) {
            vfs_close(fd);

            int ret = vfs_mount("/", "ext4", root_candidates[i]);
            if (ret == 0) {
                console_puts("[BOOT] Mounted root: ");
                console_puts(root_candidates[i]);
                console_puts("\n");
                return 0;
            }

            ret = vfs_mount("/", "fat32", root_candidates[i]);
            if (ret == 0) {
                console_puts("[BOOT] Mounted root: ");
                console_puts(root_candidates[i]);
                console_puts(" (FAT32)\n");
                return 0;
            }
        }
    }

    console_puts("[BOOT] No root file system found, using ramdisk\n");
    return -ENOENT;
}

void start_system_services(void)
{
    system_create_process("/bin/init", NULL, NULL,
                          PROCESS_FLAG_HIGH_PRIORITY, PRIORITY_HIGH);

    system_create_process("/bin/windowmanager", NULL, NULL,
                          PROCESS_FLAG_NONE, PRIORITY_NORMAL);

    system_create_process("/bin/taskbar", NULL, NULL,
                          PROCESS_FLAG_NONE, PRIORITY_NORMAL);

    if (network_is_available()) {
        system_create_process("/bin/networkmanager", NULL, NULL,
                              PROCESS_FLAG_LOW_PRIORITY, PRIORITY_LOW);
    }

    system_create_process("/bin/powermanager", NULL, NULL,
                          PROCESS_FLAG_LOW_PRIORITY, PRIORITY_LOW);
}

void show_boot_splash_screen(void)
{
    fb_info_t* fb = display_get_info();
    if (!fb) return;

    uint32_t* buffer = display_get_back_buffer();
    if (!buffer) return;

    color_t bg_color = {20, 25, 45, 255};
    color_t text_color = {200, 220, 255, 255};

    for (int y = 0; y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            buffer[y * fb->width + x] = (bg_color.a << 24) |
                                         (bg_color.r << 16) |
                                         (bg_color.g << 8) |
                                         bg_color.b;
        }
    }

    const char* title = "Kenux OS";
    const char* version = "Version 2.0 Enhanced";

    int title_x = (fb->width - strlen(title) * 24) / 2;
    int title_y = fb->height / 3;

    draw_large_text(buffer, fb->width, title_x, title_y, title, text_color, 24);

    int ver_x = (fb->width - strlen(version) * 16) / 2;
    draw_large_text(buffer, fb->width, ver_x, title_y + 40, version, text_color, 16);

    float boot_time_sec = (float)(boot_track.total_boot_time / 1000000000.0);
    char time_str[64];
    snprintf(time_str, sizeof(time_str), "Boot time: %.2f seconds", boot_time_sec);

    int time_x = (fb->width - strlen(time_str) * 12) / 2;
    int time_y = title_y + 80;
    draw_text(buffer, fb->width, time_x, time_y, time_str, text_color, 12);

    display_swap_buffers();

    delay_ms(2000);
}

char* generate_boot_report(char* buffer, size_t size)
{
    if (!buffer || size == 0 || !boot_track.initialized) return NULL;

    int written = snprintf(buffer, size,
        "=== Kenux Boot Report ===\n"
        "Boot Complete: %s\n"
        "Total Boot Time: %.2f ms (%.3f seconds)\n"
        "Phases Completed: %d\n\n",
        boot_track.boot_complete ? "Yes" : "No",
        (float)(boot_track.total_boot_time / 1000000.0),
        (float)(boot_track.total_boot_time / 1000000000.0),
        boot_track.entry_count);

    if (written > 0 && (size_t)written < size) {
        written += snprintf(buffer + written, size - written,
            "--- Phase Details ---\n");

        for (int i = 0; i < boot_track.entry_count && written > 0; i++) {
            boot_log_entry_t* entry = &boot_track.entries[i];

            written += snprintf(buffer + written, size - written,
                "%d. %-30s %s (%.2f ms)\n",
                i + 1,
                entry->phase_name,
                entry->success ? "OK" : "FAILED",
                (float)((entry->end_time - entry->start_time) / 1000000.0));

            if (!entry->success && entry->error_message[0]) {
                written += snprintf(buffer + written, size - written,
                    "   Error: %s\n", entry->error_message);
            }

            if ((size_t)written >= size) break;
        }
    }

    return buffer;
}

bool is_boot_complete(void)
{
    return boot_track.boot_complete;
}

uint64_t get_total_boot_time(void)
{
    return boot_track.total_boot_time;
}

int get_boot_phase_count(void)
{
    return boot_track.entry_count;
}

boot_log_entry_t* get_boot_phase(int index)
{
    if (index < 0 || index >= boot_track.entry_count) return NULL;
    return &boot_track.entries[index];
}

void reboot_system(void)
{
    console_puts("\n[SYSTEM] Rebooting...\n");
    sync_filesystems();
    acpi_reset();
}

void shutdown_system(void)
{
    console_puts("\n[SYSTEM] Shutting down...\n");
    sync_filesystems();
    stop_all_services();
    acpi_power_off();
}

void emergency_shutdown(const char* reason)
{
    console_puts("\n*** EMERGENCY SHUTDOWN ***\n");
    if (reason) {
        console_puts("Reason: ");
        console_puts(reason);
        console_puts("\n");
    }

    disable_interrupts();
    acpi_power_off();
}

void sync_filesystems(void)
{
    console_puts("[SYSTEM] Syncing file systems...\n");
    vfs_sync("/");
    console_puts("[SYSTEM] File systems synced\n");
}

void stop_all_services(void)
{
    console_puts("[SYSTEM] Stopping services...\n");

    perf_cleanup();
    desktop_cleanup();
    network_shutdown();
    filesystem_sync();
    display_shutdown();
    stardustui_shutdown();
    interrupt_shutdown();
    scheduler_shutdown();
    memory_cleanup();

    console_puts("[SYSTEM] All services stopped\n");
}