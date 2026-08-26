/* ============================================================
 * kenux_render.c - KENUX Render / Curve Icon & Resource Layer
 *
 * Implements all functions declared in kenux_render.h.
 *
 * Environment notes:
 *   - Bare-metal kernel, no host stdlib. Only kernel/lib/libc is
 *     available (string.h etc.) and the GUI drawing primitives.
 *   - All KAPI_* data functions now use REAL kernel data sources:
 *       Memory  : buddy allocator (arch/buddy.h)
 *       CPU     : SMP / per-CPU stats (arch/smp.h)
 *       Process : process table (arch/process.h)
 *       Uptime  : timer jiffies (timer.h)
 *       SMBIOS  : hardware info (arch/smbios.h)
 *       Disk    : kapi_blkdev block device layer (kapi_blkdev.h)
 *       Network : VLAN device stats (arch/vlan.h)
 *       VFS     : mount point table (arch/fs.h)
 *
 * Drawing dependencies (all from kernel/gui/include):
 *   framebuffer.h : fb_fill_rect, fb_blit_alpha, fb_blit_scaled
 *   graphics.h    : gfx_draw_line / hline / vline / rect / circle
 *   font.h        : font_draw_text
 *   color.h       : RGB macro, COL_* constants
 *   icon.h        : icon_draw, icon_id_t
 *   icon_data.h   : icon_get_bitmap, icon_bitmap_t (curvo-CN pack)
 * ============================================================ */

#include "kenux_render.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "color.h"
#include "icon.h"
#include "icon_data.h"
#include "msf.h"

/* Real kernel data sources */
#include <arch/buddy.h>
#include <arch/process.h>
#include <arch/elf.h>
#include <arch/smp.h>
#include <arch/smbios.h>
#include <arch/vlan.h>
#include <arch/fs.h>
#include <timer.h>
#include <string.h>
#include "kapi_blkdev.h"
#include "kapi_netdevice.h"

/* ============================================================
 * Extern declarations for kernel data not exposed in headers
 * ============================================================ */

extern process_t processes[PROCESS_MAX];
extern uint64_t process_count;
extern uint64_t kenux_uptime(void);

/* Page size for buddy memory calculations */
#define KENUX_PAGE_SIZE 4096

/* ============================================================
 * Internal state
 * ============================================================ */

/* LCG retained for GPU/TEMP pseudo-data in update_history only */
static uint32_t s_lcg_seed = 0x12345678;
static bool s_icon_pkg_loaded[4] = { false, true, false, false };
static uint32_t s_icon_pkg_version[4] = { 0, 1, 0, 0 };

/* Curve history storage (one per resource type) */
static curve_history_t s_curve_history[6];

static uint32_t s_lcg_next(void) {
    s_lcg_seed = s_lcg_seed * 1103515245u + 12345u;
    return (s_lcg_seed >> 16) & 0x7FFF;
}

/* ============================================================
 * Internal helpers
 * ============================================================ */

/* Bounded string copy (freestanding-safe) */
static void kapi_strncpy(char* dst, const char* src, uint32_t max) {
    uint32_t i = 0;
    if (!dst || !src || max == 0) return;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* Retrieve CPU brand string via CPUID (leaf 0x80000002-0x80000004).
 * Falls back to vendor string (leaf 0) if extended leaves unavailable. */
static void kapi_get_cpuid_brand(char* buf, uint32_t buf_size) {
    if (!buf || buf_size == 0) return;
    buf[0] = '\0';

    uint32_t eax, ebx, ecx, edx;
    uint32_t max_ext;

    /* Check maximum extended CPUID leaf */
    __asm__ volatile ("cpuid"
        : "=a"(max_ext), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0x80000000));

    if (max_ext >= 0x80000004 && buf_size >= 49) {
        /* Brand string: three 16-byte leaves -> 48 chars */
        uint32_t* p = (uint32_t*)buf;
        __asm__ volatile ("cpuid"
            : "=a"(p[0]), "=b"(p[1]), "=c"(p[2]), "=d"(p[3])
            : "a"(0x80000002));
        __asm__ volatile ("cpuid"
            : "=a"(p[4]), "=b"(p[5]), "=c"(p[6]), "=d"(p[7])
            : "a"(0x80000003));
        __asm__ volatile ("cpuid"
            : "=a"(p[8]), "=b"(p[9]), "=c"(p[10]), "=d"(p[11])
            : "a"(0x80000004));
        buf[48] = '\0';

        /* Trim leading spaces */
        uint32_t start = 0;
        while (buf[start] == ' ' && buf[start] != '\0') start++;
        if (start > 0) {
            uint32_t j = 0;
            while (buf[start]) buf[j++] = buf[start++];
            buf[j] = '\0';
        }
    } else {
        /* Fallback: vendor string from leaf 0 (12 bytes) */
        __asm__ volatile ("cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(0));
        uint32_t* p = (uint32_t*)buf;
        p[0] = ebx; p[1] = edx; p[2] = ecx;
        buf[12] = '\0';
        if (buf_size > 12) {
            kapi_strncpy(buf + 12, " x86_64", buf_size - 12 - 1);
        }
    }
}

/* Map process_t.state to kapi_process_info_t.status (0=running,1=suspended,2=terminated) */
static uint32_t process_state_to_status(uint64_t state) {
    switch (state) {
        case PROCESS_RUNNING:
        case PROCESS_READY:
            return 0;  /* running */
        case PROCESS_SLEEPING:
        case PROCESS_WAITING:
        case PROCESS_STOPPED:
            return 1;  /* suspended */
        case PROCESS_TERMINATED:
        case PROCESS_ZOMBIE:
        case PROCESS_DEAD:
        case PROCESS_UNUSED:
        default:
            return 2;  /* terminated */
    }
}

/* Map process_t.state to an icon_id_t */
static icon_id_t process_state_to_icon(uint64_t state) {
    switch (state) {
        case PROCESS_RUNNING:
        case PROCESS_READY:
            return ICON_CHECK;
        case PROCESS_SLEEPING:
        case PROCESS_WAITING:
        case PROCESS_STOPPED:
            return ICON_WARNING;
        case PROCESS_TERMINATED:
        case PROCESS_ZOMBIE:
        case PROCESS_DEAD:
        case PROCESS_UNUSED:
        default:
            return ICON_ERROR;
    }
}

/* Draw a red circle with a white X at the given position */
static void draw_error_icon_at(uint32_t x, uint32_t y, uint32_t size) {
    uint32_t red     = RGB(0xE0, 0x3A, 0x3A);
    uint32_t darkred = RGB(0x8B, 0x00, 0x00);
    uint32_t white   = RGB(0xFF, 0xFF, 0xFF);

    int32_t cx = (int32_t)(x + size / 2);
    int32_t cy = (int32_t)(y + size / 2);
    int32_t r  = (int32_t)(size / 2);
    if (r < 2) r = 2;

    /* Filled red circle with dark-red outline */
    gfx_draw_filled_circle(cx, cy, r - 1, red);
    gfx_draw_circle(cx, cy, r - 1, darkred);

    /* White X inside the circle */
    int32_t inset = r / 3;
    if (inset < 1) inset = 1;
    gfx_draw_line(cx - r + inset, cy - r + inset,
                  cx + r - inset, cy + r - inset, white);
    gfx_draw_line(cx + r - inset, cy - r + inset,
                  cx - r + inset, cy + r - inset, white);
}

/* ============================================================
 * Icon package lifecycle
 * ============================================================ */

int32_t KENUX_Render_LoadIconPackage(curve_icon_package_id_t pkg_id) {
    if (pkg_id < 0 || pkg_id > 3) return KAPI_ERROR_NOT_FOUND;
    s_icon_pkg_loaded[pkg_id] = true;
    s_icon_pkg_version[pkg_id] = 1;
    return KAPI_OK;
}

void KENUX_Render_UnloadIconPackage(curve_icon_package_id_t pkg_id) {
    if (pkg_id < 0 || pkg_id > 3) return;
    s_icon_pkg_loaded[pkg_id] = false;
}

void KENUX_Render_SetIconVersion(curve_icon_package_id_t pkg_id, uint32_t version) {
    if (pkg_id < 0 || pkg_id > 3) return;
    s_icon_pkg_version[pkg_id] = version;
}

/* ============================================================
 * Icon rendering
 * ============================================================ */

void KENUX_Render_SetIcon(icon_id_t icon_id) {
    /* Placeholder: could set a current-icon state for batch drawing */
    (void)icon_id;
}

void KENUX_Render_DrawIcon(uint32_t x, uint32_t y, icon_id_t icon_id, uint32_t size) {
    /* Check if the default icon package (curvo-CN) is loaded */
    if (s_icon_pkg_loaded[CURVE_ICON_PKG_DEFAULT]) {
        const icon_bitmap_t* bmp = icon_get_bitmap((int)icon_id);
        if (bmp && bmp->data) {
            /* Render the curvo-CN bitmap directly */
            if (size == bmp->width && size == bmp->height) {
                fb_blit_alpha(x, y, size, size, bmp->data);
            } else {
                fb_blit_scaled(x, y, size, size, bmp->data,
                               bmp->width, bmp->height);
            }
            return;
        }
    }

    /* Icon package not loaded or bitmap not found */
    draw_error_icon_at(x, y, size);
    KENUX_Render_ShowErrorIcon(KAPI_ERROR_ICON_PACKAGE_LOAD_FAILED,
                               CURVE_ICON_PKG_DEFAULT);
}

void KENUX_Render_AdaptCurveIcon(icon_id_t icon_id, uint32_t container_size) {
    (void)icon_id;
    (void)container_size;
}

/* ============================================================
 * Curve binding & drawing
 * ============================================================ */

void KENUX_Render_BindCurveIcon(uint32_t column_id, kapi_event_type_t event,
                                curve_icon_package_id_t pkg_id) {
    (void)column_id;
    (void)event;
    (void)pkg_id;
}

void KENUX_Render_DrawCurve(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            const curve_history_t* history, uint32_t color,
                            kenux_resource_t resource_type) {
    if (!history || history->count == 0) return;
    (void)resource_type;

    /* Draw background */
    fb_fill_rect(x, y, w, h, RGB(0x1A, 0x1A, 0x1A));

    /* Draw grid lines */
    uint32_t grid_color = RGB(0x33, 0x33, 0x33);
    for (uint32_t i = 0; i <= 4; i++) {
        uint32_t gy = y + (h * i) / 4;
        gfx_draw_hline(x, gy, w, grid_color);
    }

    /* Draw curve */
    uint32_t n = history->count;
    if (n > CURVE_HISTORY_MAX) n = CURVE_HISTORY_MAX;

    for (uint32_t i = 0; i + 1 < n; i++) {
        uint32_t v0 = history->points[i].value;
        uint32_t v1 = history->points[i + 1].value;
        if (v0 > 100) v0 = 100;
        if (v1 > 100) v1 = 100;

        int32_t x0 = (int32_t)(x + (w * i) / n);
        int32_t x1 = (int32_t)(x + (w * (i + 1)) / n);
        int32_t y0 = (int32_t)(y + h - (h * v0) / 100);
        int32_t y1 = (int32_t)(y + h - (h * v1) / 100);

        gfx_draw_line(x0, y0, x1, y1, color);
    }

    /* Fill area under curve */
    uint32_t fill_color = (color & 0x00FFFFFF) | 0x20000000;
    for (uint32_t i = 0; i + 1 < n; i++) {
        uint32_t v0 = history->points[i].value;
        uint32_t v1 = history->points[i + 1].value;
        if (v0 > 100) v0 = 100;
        if (v1 > 100) v1 = 100;

        int32_t x0 = (int32_t)(x + (w * i) / n);
        int32_t x1 = (int32_t)(x + (w * (i + 1)) / n);
        int32_t y0 = (int32_t)(y + h - (h * v0) / 100);
        int32_t y1 = (int32_t)(y + h - (h * v1) / 100);
        int32_t yb = (int32_t)(y + h);

        /* Simple fill: vertical lines between curve and bottom */
        for (int32_t px = x0; px < x1 && px < (int32_t)(x + w); px++) {
            int32_t frac = (px - x0);
            int32_t span = (x1 - x0);
            if (span <= 0) span = 1;
            int32_t py = y0 + (y1 - y0) * frac / span;
            if (py < yb) {
                gfx_draw_vline((uint32_t)px, (uint32_t)py, (uint32_t)(yb - py), fill_color);
            }
        }
    }
}

void KENUX_Render_DrawDashboardCard(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                    kenux_resource_t resource, uint32_t value,
                                    curve_icon_package_id_t pkg_id) {
    (void)pkg_id;

    /* Card background */
    fb_fill_rect(x, y, w, h, RGB(0x20, 0x20, 0x20));
    gfx_draw_rect(x, y, w, h, RGB(0xFF, 0xFF, 0xFF));

    /* Title */
    const char* title = "CPU";
    uint32_t title_color = RGB(0x00, 0xD4, 0xFF);
    switch (resource) {
        case KENUX_RESOURCE_CPU:  title = "CPU";  title_color = RGB(0x00, 0xD4, 0xFF); break;
        case KENUX_RESOURCE_MEM:  title = "Memory"; title_color = RGB(0x00, 0xFF, 0x88); break;
        case KENUX_RESOURCE_DISK: title = "Disk";  title_color = RGB(0xFF, 0xD7, 0x00); break;
        case KENUX_RESOURCE_NET:  title = "Network"; title_color = RGB(0xFF, 0x6B, 0x6B); break;
        case KENUX_RESOURCE_GPU:  title = "GPU";   title_color = RGB(0xBB, 0x88, 0xFF); break;
        case KENUX_RESOURCE_TEMP: title = "Temp";  title_color = RGB(0xFF, 0xA5, 0x00); break;
    }

    font_draw_text(x + 8, y + 6, title, title_color);

    /* Value text */
    char val_str[16];
    val_str[0] = '\0';
    /* Simple integer to string */
    {
        uint32_t v = value;
        char tmp[16];
        int32_t i = 0;
        if (v == 0) { tmp[0] = '0'; i = 1; }
        while (v > 0) { tmp[i++] = '0' + (v % 10); v /= 10; }
        int32_t j = 0;
        while (i > 0) val_str[j++] = tmp[--i];
        val_str[j] = '\0';
    }

    /* Append % */
    {
        int32_t k = 0;
        while (val_str[k]) k++;
        val_str[k] = '%';
        val_str[k + 1] = '\0';
    }

    font_draw_text(x + 8, y + 22, val_str, RGB(0xFF, 0xFF, 0xFF));

    /* Mini progress bar */
    uint32_t bar_y = y + h - 12;
    uint32_t bar_h = 6;
    fb_fill_rect(x + 8, bar_y, w - 16, bar_h, RGB(0x33, 0x33, 0x33));
    uint32_t fill_w = ((w - 16) * (value > 100 ? 100 : value)) / 100;
    if (fill_w > 0) {
        fb_fill_rect(x + 8, bar_y, fill_w, bar_h, title_color);
    }
}

void KENUX_Render_BindDashboardCurve(uint32_t card_id, kapi_event_type_t event,
                                     curve_icon_package_id_t pkg_id) {
    (void)card_id; (void)event; (void)pkg_id;
}

void KENUX_Render_BindStorageCurve(uint32_t disk_id, kapi_event_type_t event,
                                   curve_icon_package_id_t pkg_id) {
    (void)disk_id; (void)event; (void)pkg_id;
}

void KENUX_Render_BindLogCurve(uint32_t sandbox_id, kapi_event_type_t event,
                               curve_icon_package_id_t pkg_id) {
    (void)sandbox_id; (void)event; (void)pkg_id;
}

void KENUX_Render_BindUptimeCurve(uint32_t uptime_sec, curve_icon_package_id_t pkg_id) {
    (void)uptime_sec; (void)pkg_id;
}

void KENUX_Render_LoadCurveHistory(uint32_t pid, curve_icon_package_id_t pkg_id) {
    (void)pid; (void)pkg_id;
}

void KENUX_Render_ExportReportCurve(uint32_t sandbox_id, curve_icon_package_id_t pkg_id) {
    (void)sandbox_id; (void)pkg_id;
}

void KENUX_Render_ShowErrorIcon(int32_t error_code, curve_icon_package_id_t pkg_id) {
    (void)error_code;
    (void)pkg_id;
    /* Draw a red circle with white X at a default position */
    draw_error_icon_at(2, 2, 32);
}

/* ============================================================
 * KAPI data access - System monitoring (real kernel data)
 * ============================================================ */

uint32_t KAPI_System_GetCPUUsage(void) {
    /* Compute CPU usage from SMP per-CPU idle/sched ticks.
     * usage = sum(sched_ticks) / sum(sched_ticks + idle_ticks) * 100 */
    uint32_t cpu_count = smp_cpu_count();
    if (cpu_count == 0) cpu_count = 1;

    uint64_t total_sched = 0;
    uint64_t total_idle = 0;

    for (uint32_t i = 0; i < cpu_count; i++) {
        cpu_info_t* cpu = smp_cpu_info(i);
        if (cpu) {
            total_sched += cpu->sched_ticks;
            total_idle  += cpu->idle_ticks;
        }
    }

    uint64_t total = total_sched + total_idle;
    if (total == 0) return 0;
    uint32_t usage = (uint32_t)((total_sched * 100) / total);
    if (usage > 100) usage = 100;
    return usage;
}

uint32_t KAPI_System_GetMemUsage(void) {
    /* Compute memory usage from buddy allocator stats.
     * usage = used_pages / total_pages * 100 */
    buddy_zone_t* zone = buddy_get_main_zone();
    if (!zone) return 0;

    uint64_t total = 0, free = 0, used = 0;
    buddy_get_stats(zone, &total, &free, &used);
    if (total == 0) return 0;

    uint32_t usage = (uint32_t)((used * 100) / total);
    if (usage > 100) usage = 100;
    return usage;
}

uint32_t KAPI_System_GetDiskUsage(void) {
    /* Compute aggregate disk usage across all block devices.
     * The block-device layer exposes capacity but not per-filesystem
     * used space, so we report device presence as a baseline. */
    int dev_count = kapi_blkdev_count();
    if (dev_count <= 0) return 0;

    /* Enumerate devices and sum total capacity (bytes) */
    uint64_t total_capacity = 0;
    for (int i = 0; i < dev_count; i++) {
        kapi_blkdev_t dev = kapi_blkdev_get_by_index(i);
        if (dev) {
            kapi_blkdev_info_t info;
            if (kapi_blkdev_get_info(dev, &info) == 0) {
                total_capacity += info.capacity;
            }
            kapi_blkdev_put(dev);
        }
    }

    /* Without filesystem-level statfs, we cannot determine used space.
     * Return 0 to indicate no usage data available. */
    (void)total_capacity;
    return 0;
}

uint32_t KAPI_System_GetNetUsage(void) {
    /* Compute network activity from VLAN device stats.
     * Scale total bytes to a 0-100 percentage. */
    uint32_t dev_count = vlan_device_count();
    if (dev_count == 0) return 0;

    uint64_t total_bytes = 0;
    for (uint32_t i = 0; i < dev_count; i++) {
        vlan_device_t* dev = vlan_get_device_by_index(i);
        if (dev) {
            total_bytes += (uint64_t)dev->rx_bytes + (uint64_t)dev->tx_bytes;
        }
    }

    /* Every 1 MiB of cumulative traffic ~= 1% usage (capped at 100) */
    uint32_t usage = (uint32_t)(total_bytes / (1024 * 1024));
    if (usage > 100) usage = 100;
    return usage;
}

uint32_t KAPI_System_Uptime(void) {
    /* Return uptime in seconds from timer jiffies.
     * timer_jiffies_to_ms() converts jiffies to milliseconds (1 tick = 1 ms). */
    uint64_t jiffies = timer_get_jiffies();
    uint64_t ms = timer_jiffies_to_ms(jiffies);
    return (uint32_t)(ms / 1000);
}

uint32_t KAPI_System_GetMemTotal(void) {
    /* Return total memory in KB from buddy allocator.
     * total_pages * PAGE_SIZE / 1024 = total_pages * 4 (KB) */
    buddy_zone_t* zone = buddy_get_main_zone();
    if (!zone) return 0;

    uint64_t total = 0, free = 0, used = 0;
    buddy_get_stats(zone, &total, &free, &used);

    return (uint32_t)((total * KENUX_PAGE_SIZE) / 1024);
}

uint32_t KAPI_System_GetMemFree(void) {
    /* Return free memory in KB from buddy allocator.
     * free_pages * PAGE_SIZE / 1024 = free_pages * 4 (KB) */
    buddy_zone_t* zone = buddy_get_main_zone();
    if (!zone) return 0;

    uint64_t total = 0, free = 0, used = 0;
    buddy_get_stats(zone, &total, &free, &used);

    return (uint32_t)((free * KENUX_PAGE_SIZE) / 1024);
}

void KAPI_System_GetCPUModel(char* buf, uint32_t buf_size) {
    if (!buf || buf_size == 0) return;

    /* Try SMBIOS system info first (manufacturer/product strings are
     * string-table indices, not directly usable here). SMBIOS does not
     * expose a processor type in the current struct set, so fall back
     * to CPUID brand string. */
    smbios_system_t* sys = smbios_get_system();
    if (sys) {
        /* SMBIOS available but no CPU model field; proceed to CPUID */
    }

    /* Use CPUID to retrieve the processor brand string */
    kapi_get_cpuid_brand(buf, buf_size);

    /* If CPUID returned empty, use a generic fallback */
    if (buf[0] == '\0') {
        kapi_strncpy(buf, "Kenux x86_64 Generic", buf_size);
    }
}

void KAPI_System_GetOSVersion(char* buf, uint32_t buf_size) {
    kapi_strncpy(buf, "Kenux 4.0 (KenuxK 26.7.9)", buf_size);
}

icon_id_t KAPI_System_GetHardwareIcon(uint32_t hardware_id) {
    switch (hardware_id) {
        case 0: return ICON_COMPUTER;
        case 1: return ICON_SETTINGS;
        case 2: return ICON_NETWORK;
        default: return ICON_INFO;
    }
}

icon_id_t KAPI_System_GetRefreshIcon(void) {
    return ICON_REFRESH;
}

/* ============================================================
 * KAPI data access - Process management (real kernel data)
 * ============================================================ */

static uint64_t process_last_sched[PROCESS_MAX];
static uint32_t process_last_cpu[PROCESS_MAX];

static uint32_t process_memory_kb(const process_t* proc) {
    uint64_t bytes = proc->mem_usage + proc->rss;
    if (proc->stack && proc->stack_bottom) bytes += PROCESS_STACK_SIZE;
    if ((proc->flags & PROC_FLAG_USER) && proc->stack_top) bytes += 32u * 4096u;
    return (uint32_t)((bytes + 1023) / 1024);
}

uint32_t KAPI_Process_GetCount(void) {
    /* Count live processes (not DEAD or UNUSED) */
    uint32_t count = 0;
    for (int i = 0; i < process_count; i++) {
        if (processes[i].state != PROCESS_DEAD &&
            processes[i].state != PROCESS_UNUSED) {
            count++;
        }
    }
    return count;
}

int32_t KAPI_Process_GetList(kapi_process_info_t* list, uint32_t max_count) {
    if (!list || max_count == 0) return KAPI_ERROR_NOT_FOUND;

    uint64_t total_delta = 0;
    for (int i = 0; i < process_count; i++) {
        process_t* proc = &processes[i];
        if (proc->state == PROCESS_DEAD || proc->state == PROCESS_UNUSED) continue;
        uint64_t delta = proc->sched_count - process_last_sched[i];
        total_delta += delta;
    }

    uint32_t out = 0;
    for (int i = 0; i < process_count && out < max_count; i++) {
        process_t* proc = &processes[i];
        if (proc->state == PROCESS_DEAD || proc->state == PROCESS_UNUSED)
            continue;

        list[out].pid = (uint32_t)proc->id;
        list[out].status = process_state_to_status(proc->state);
        list[out].mem_usage = process_memory_kb(proc);

        uint64_t delta = proc->sched_count - process_last_sched[i];
        uint32_t cpu_pct = total_delta ? (uint32_t)((delta * 100) / total_delta) : process_last_cpu[i];
        if (cpu_pct > 100) cpu_pct = 100;
        list[out].cpu_usage = cpu_pct;
        process_last_sched[i] = proc->sched_count;
        process_last_cpu[i] = cpu_pct;

        kapi_strncpy(list[out].name, proc->name, sizeof(list[out].name));
        out++;
    }

    return (int32_t)out;
}

int32_t KAPI_Process_Details(uint32_t pid, kapi_process_info_t* info) {
    if (!info || pid == 0) return KAPI_ERROR_NOT_FOUND;

    process_t* proc = process_get((uint64_t)pid);
    if (!proc) return KAPI_ERROR_NOT_FOUND;

    info->pid = (uint32_t)proc->id;
    info->status = process_state_to_status(proc->state);
    info->mem_usage = process_memory_kb(proc);
    info->cpu_usage = pid < PROCESS_MAX ? process_last_cpu[pid] : 0;

    kapi_strncpy(info->name, proc->name, sizeof(info->name));
    return KAPI_OK;
}

icon_id_t KAPI_Process_GetStatusIcon(uint32_t pid) {
    process_t* proc = process_get((uint64_t)pid);
    if (!proc) return ICON_ERROR;
    return process_state_to_icon(proc->state);
}

int32_t KAPI_Process_Kill(uint32_t pid) {
    if (pid == 0) return KAPI_ERROR_NOT_FOUND;
    process_t* proc = process_get((uint64_t)pid);
    if (!proc) return KAPI_ERROR_NOT_FOUND;
    process_kill((uint64_t)pid, 9);
    return KAPI_OK;
}

/* ============================================================
 * KAPI data access - VFS
 * ============================================================ */

icon_id_t KAPI_VFS_GetFileIcon(const char* path, curve_icon_package_id_t pkg_id) {
    (void)pkg_id;
    if (!path) return ICON_FILE;
    /* Simple heuristic based on path */
    if (path[0] == '/') return ICON_FOLDER;
    return ICON_FILE;
}

icon_id_t KAPI_VFS_GetMountStatusIcon(uint32_t mount_id) {
    if (mount_id < (uint32_t)mount_count) {
        if (mount_points[mount_id].mounted) return ICON_CHECK;
        return ICON_WARNING;
    }
    return ICON_ERROR;
}

int32_t KAPI_VFS_PathParse(const char* path) {
    if (!path) return KAPI_ERROR_INVALID_PATH;
    if (path[0] != '/') return KAPI_ERROR_INVALID_PATH;
    return KAPI_OK;
}

uint32_t KAPI_VFS_GetMountCount(void) {
    return (uint32_t)mount_count;
}

int32_t KAPI_VFS_GetMountList(kapi_mount_info_t* list, uint32_t max_count) {
    if (!list || max_count == 0) return KAPI_ERROR_NOT_FOUND;

    uint32_t out = 0;
    for (int i = 0; i < mount_count && out < max_count; i++) {
        kapi_strncpy(list[out].mountpoint, mount_points[i].mountpoint,
                     sizeof(list[out].mountpoint));
        kapi_strncpy(list[out].fstype, mount_points[i].fstype,
                     sizeof(list[out].fstype));
        /* mount_point_t does not store a device/source field */
        list[out].device[0] = '\0';
        list[out].mounted = mount_points[i].mounted ? true : false;
        out++;
    }

    return KAPI_OK;
}

/* ============================================================
 * KAPI data access - Network interfaces (VLAN devices)
 * ============================================================ */

uint32_t KAPI_Net_GetInterfaceCount(void) {
    return vlan_device_count();
}

int32_t KAPI_Net_GetInterfaceList(kapi_net_info_t* list, uint32_t max_count) {
    if (!list || max_count == 0) return KAPI_ERROR_NOT_FOUND;

    uint32_t dev_count = vlan_device_count();
    uint32_t out = 0;

    for (uint32_t i = 0; i < dev_count && out < max_count; i++) {
        vlan_device_t* dev = vlan_get_device_by_index(i);
        if (!dev) continue;

        kapi_strncpy(list[out].name, dev->name, sizeof(list[out].name));
        /* VLAN devices do not track IP address in the current net stack */
        list[out].ip_addr = 0;
        list[out].rx_bytes = dev->rx_bytes;
        list[out].tx_bytes = dev->tx_bytes;
        list[out].rx_packets = dev->rx_packets;
        list[out].tx_packets = dev->tx_packets;
        out++;
    }

    return KAPI_OK;
}

int32_t KAPI_Connectivity_GetStatus(kapi_connectivity_status_t* status) {
    return kapi_connectivity_get_status(status);
}

/* ============================================================
 * KAPI data access - Disk / block devices
 * ============================================================ */

uint32_t KAPI_Disk_GetCount(void) {
    int count = kapi_blkdev_count();
    if (count < 0) count = 0;
    return (uint32_t)count;
}

int32_t KAPI_Disk_GetList(kapi_disk_info_t* list, uint32_t max_count) {
    if (!list || max_count == 0) return KAPI_ERROR_NOT_FOUND;

    int dev_count = kapi_blkdev_count();
    uint32_t out = 0;

    for (int i = 0; i < dev_count && out < max_count; i++) {
        kapi_blkdev_t dev = kapi_blkdev_get_by_index(i);
        if (!dev) continue;

        kapi_blkdev_info_t info;
        if (kapi_blkdev_get_info(dev, &info) != 0) {
            kapi_blkdev_put(dev);
            continue;
        }

        kapi_strncpy(list[out].name, info.name, sizeof(list[out].name));
        list[out].sector_size = info.sector_size;
        list[out].capacity_bytes = info.capacity;
        list[out].readonly = info.readonly ? true : false;

        /* Compute total sectors from capacity and sector size */
        if (info.sector_size > 0) {
            list[out].total_sectors = info.capacity / info.sector_size;
        } else {
            list[out].total_sectors = 0;
        }

        kapi_blkdev_put(dev);
        out++;
    }

    return KAPI_OK;
}

/* ============================================================
 * KAPI data access - Sandbox (simulated)
 * ============================================================ */

uint32_t KAPI_Sandbox_GetCount(void) {
    return 0;
}

int32_t KAPI_Sandbox_GetList(kapi_sandbox_info_t* list, uint32_t max_count) {
    (void)list; (void)max_count;
    return KAPI_OK;
}

icon_id_t KAPI_Sandbox_GetStatusIcon(uint32_t sandbox_id) {
    (void)sandbox_id;
    return ICON_INFO;
}

int32_t KAPI_Sandbox_SetEnv(uint32_t sandbox_id, const char* key, const char* value) {
    (void)sandbox_id; (void)key; (void)value;
    return KAPI_OK;
}

int32_t KAPI_Sandbox_SetWorkDir(uint32_t sandbox_id, const char* path) {
    (void)sandbox_id; (void)path;
    return KAPI_OK;
}

/* ============================================================
 * Curve history management
 * ============================================================ */

void kenux_render_update_history(void) {
    /* Update all resource histories with current values.
     * CPU/MEM/DISK/NET use real kernel data; GPU/TEMP still use
     * pseudo-random values (no real sensors available yet). */
    static uint32_t tick = 0;
    tick++;

    for (uint32_t r = 0; r < 6; r++) {
        curve_history_t* h = &s_curve_history[r];
        uint32_t val = 0;
        switch (r) {
            case KENUX_RESOURCE_CPU:  val = KAPI_System_GetCPUUsage(); break;
            case KENUX_RESOURCE_MEM:  val = KAPI_System_GetMemUsage(); break;
            case KENUX_RESOURCE_DISK: val = KAPI_System_GetDiskUsage(); break;
            case KENUX_RESOURCE_NET:  val = KAPI_System_GetNetUsage(); break;
            case KENUX_RESOURCE_GPU:  val = s_lcg_next() % 25; break;
            case KENUX_RESOURCE_TEMP: val = 45 + (s_lcg_next() % 15); break;
        }

        if (h->count < CURVE_HISTORY_MAX) {
            h->points[h->count].timestamp = tick;
            h->points[h->count].value = val;
            h->count++;
        } else {
            /* Shift left */
            for (uint32_t i = 0; i < CURVE_HISTORY_MAX - 1; i++) {
                h->points[i] = h->points[i + 1];
            }
            h->points[CURVE_HISTORY_MAX - 1].timestamp = tick;
            h->points[CURVE_HISTORY_MAX - 1].value = val;
        }
        h->current_value = val;
    }
}

const curve_history_t* kenux_render_get_history(kenux_resource_t resource) {
    if (resource > 5) return NULL;
    return &s_curve_history[resource];
}
