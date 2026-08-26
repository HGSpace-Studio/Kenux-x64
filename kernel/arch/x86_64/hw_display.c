/*
 * Hardware-Accelerated Display Pipeline - kernel implementation.
 *
 * Provides VRR (Variable Refresh Rate), multi-monitor management,
 * swap chain (page flip + triple buffering), and VBLANK tracking
 * using fixed-size static pools so no dynamic allocator is required
 * beyond memory_alloc/memory_free for surface buffers.
 */

#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct _IRP IRP;
typedef struct _IO_STACK_LOCATION IO_STACK_LOCATION;
typedef DEVICE_OBJECT* PDEVICE_OBJECT;
typedef DRIVER_OBJECT* PDRIVER_OBJECT;
typedef IRP* PIRP;
typedef IO_STACK_LOCATION* PIO_STACK_LOCATION;
#endif

#include <arch/win32.h>
#include <string.h>
#include <stdio.h>

extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* ptr);

static uint64_t rdtsc(void) {
    uint32_t hi, lo;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* ------------------------------------------------------------------ *
 * Constants & pools
 * ------------------------------------------------------------------ */
#define HW_MAX_MONITORS   8
#define HW_MAX_SURFACES   16

static MONITOR_INFO g_monitors[HW_MAX_MONITORS];
static int          g_monitor_count;

static SWAP_SURFACE g_swap_surfaces[HW_MAX_SURFACES];
static PRESENT_STATS g_present_stats[HW_MAX_SURFACES];
static PRESENT_MODE  g_present_modes[HW_MAX_SURFACES];
static uint32_t      g_surface_monitor[HW_MAX_SURFACES];
static int           g_surface_in_use[HW_MAX_SURFACES];
static int           g_surface_count;

static uint64_t g_vblank_last_time[HW_MAX_MONITORS];
static uint32_t g_vblank_count[HW_MAX_MONITORS];
static void (*g_vblank_callbacks[HW_MAX_MONITORS])(uint32_t, uint64_t);

/* ------------------------------------------------------------------ *
 * Helpers
 * ------------------------------------------------------------------ */

/* Swap displayed and pending buffers (page flip). */
static void do_flip(SWAP_SURFACE* s) {
    uint32_t tmp = s->displayed_idx;
    s->displayed_idx = s->pending_idx;
    s->pending_idx = tmp;
    s->pending_flip = 0;
}

/* Fill in typical HDR10 mastering display metadata (BT.2020 / ST.2086). */
static void set_default_hdr_metadata(HDR_METADATA* meta) {
    memset(meta, 0, sizeof(*meta));
    /* BT.2020 primaries, values * 50000 */
    meta->display_primary_x[0] = 35400; meta->display_primary_y[0] = 14600;
    meta->display_primary_x[1] = 8500;  meta->display_primary_y[1] = 39850;
    meta->display_primary_x[2] = 6550;  meta->display_primary_y[2] = 2300;
    /* D65 white point * 50000 */
    meta->white_point_x = 15635;
    meta->white_point_y = 16450;
    meta->max_luminance = 1000;
    meta->min_luminance = 500;
    meta->max_content_light_level = 1000;
    meta->max_frame_average_light_level = 180;
    meta->valid = 1;
}

/* ================================================================== *
 * 1. Multi-monitor management
 * ================================================================== */

int monitor_init(void) {
    memset(g_monitors, 0, sizeof(g_monitors));
    g_monitor_count = 0;

    /* Monitor 0 - Primary Display */
    {
        MONITOR_INFO* m = &g_monitors[0];
        m->monitor_id = 0;
        strncpy(m->name, "Primary Display", sizeof(m->name) - 1);
        m->origin_x = 0;
        m->origin_y = 0;
        m->width = 1920;
        m->height = 1080;
        m->refresh_rate_mhz = 60000;
        m->min_refresh_mhz = 60000;
        m->max_refresh_mhz = 60000;
        m->dpi = 96;
        m->scale_percent = 100;
        m->color_space = COLOR_SPACE_SRGB;
        m->hdr_capable = 0;
        m->vrr_capable = 1;
        m->primary = 1;
        m->connected = 1;
        m->vrr.mode = VRR_FREE_SYNC;
        m->vrr.min_refresh_mhz = 48000;
        m->vrr.max_refresh_mhz = 60000;
        m->vrr.current_refresh_mhz = 60000;
        m->vrr.seamless = 1;
        m->vrr.active = 0;
        m->vrr.capability_valid = 1;
        memset(&m->hdr_meta, 0, sizeof(m->hdr_meta));
        g_monitor_count++;
    }

    /* Monitor 1 - Secondary HDR */
    {
        MONITOR_INFO* m = &g_monitors[1];
        m->monitor_id = 1;
        strncpy(m->name, "Secondary HDR", sizeof(m->name) - 1);
        m->origin_x = 1920;
        m->origin_y = 0;
        m->width = 3840;
        m->height = 2160;
        m->refresh_rate_mhz = 120000;
        m->min_refresh_mhz = 120000;
        m->max_refresh_mhz = 120000;
        m->dpi = 192;
        m->scale_percent = 200;
        m->color_space = COLOR_SPACE_BT2020_PQ;
        m->hdr_capable = 1;
        m->vrr_capable = 1;
        m->primary = 0;
        m->connected = 1;
        m->vrr.mode = VRR_G_SYNC;
        m->vrr.min_refresh_mhz = 40000;
        m->vrr.max_refresh_mhz = 120000;
        m->vrr.current_refresh_mhz = 120000;
        m->vrr.seamless = 1;
        m->vrr.active = 0;
        m->vrr.capability_valid = 1;
        set_default_hdr_metadata(&m->hdr_meta);
        g_monitor_count++;
    }

    /* Monitor 2 - Side Monitor */
    {
        MONITOR_INFO* m = &g_monitors[2];
        m->monitor_id = 2;
        strncpy(m->name, "Side Monitor", sizeof(m->name) - 1);
        m->origin_x = -2560;
        m->origin_y = 0;
        m->width = 2560;
        m->height = 1440;
        m->refresh_rate_mhz = 144000;
        m->min_refresh_mhz = 144000;
        m->max_refresh_mhz = 144000;
        m->dpi = 96;
        m->scale_percent = 100;
        m->color_space = COLOR_SPACE_SRGB;
        m->hdr_capable = 0;
        m->vrr_capable = 1;
        m->primary = 0;
        m->connected = 1;
        m->vrr.mode = VRR_VESA_ADAPTIVE;
        m->vrr.min_refresh_mhz = 48000;
        m->vrr.max_refresh_mhz = 144000;
        m->vrr.current_refresh_mhz = 144000;
        m->vrr.seamless = 1;
        m->vrr.active = 0;
        m->vrr.capability_valid = 1;
        memset(&m->hdr_meta, 0, sizeof(m->hdr_meta));
        g_monitor_count++;
    }

    return 0;
}

int monitor_register(const char* name, uint32_t w, uint32_t h,
                     uint32_t refresh_mhz, uint32_t dpi) {
    int i;
    for (i = 0; i < HW_MAX_MONITORS; i++) {
        if (g_monitors[i].monitor_id != (uint32_t)i) {
            MONITOR_INFO* m = &g_monitors[i];
            memset(m, 0, sizeof(*m));
            m->monitor_id = (uint32_t)i;
            if (name) {
                strncpy(m->name, name, sizeof(m->name) - 1);
            }
            m->width = w;
            m->height = h;
            m->refresh_rate_mhz = refresh_mhz;
            m->min_refresh_mhz = refresh_mhz;
            m->max_refresh_mhz = refresh_mhz;
            m->dpi = dpi;
            m->scale_percent = dpi * 100 / 96;
            m->color_space = COLOR_SPACE_SRGB;
            m->hdr_capable = 0;
            m->vrr_capable = 0;
            m->primary = 0;
            m->connected = 1;
            m->vrr.mode = VRR_DISABLED;
            m->vrr.min_refresh_mhz = refresh_mhz;
            m->vrr.max_refresh_mhz = refresh_mhz;
            m->vrr.current_refresh_mhz = refresh_mhz;
            m->vrr.seamless = 0;
            m->vrr.active = 0;
            m->vrr.capability_valid = 0;
            g_monitor_count++;
            return i;
        }
    }
    return -1;
}

int monitor_get_count(void) {
    int i;
    int count = 0;
    for (i = 0; i < HW_MAX_MONITORS; i++) {
        if (g_monitors[i].connected) {
            count++;
        }
    }
    return count;
}

MONITOR_INFO* monitor_get_info(uint32_t monitor_id) {
    if (monitor_id >= HW_MAX_MONITORS) return NULL;
    if (g_monitors[monitor_id].monitor_id != monitor_id) return NULL;
    return &g_monitors[monitor_id];
}

MONITOR_INFO* monitor_get_primary(void) {
    int i;
    for (i = 0; i < HW_MAX_MONITORS; i++) {
        if (g_monitors[i].connected && g_monitors[i].primary) {
            return &g_monitors[i];
        }
    }
    return NULL;
}

int monitor_set_refresh_rate(uint32_t monitor_id, uint32_t refresh_mhz) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return -1;
    if (m->vrr.active) {
        if (refresh_mhz < m->vrr.min_refresh_mhz ||
            refresh_mhz > m->vrr.max_refresh_mhz) {
            return -1;
        }
    }
    m->refresh_rate_mhz = refresh_mhz;
    return 0;
}

int monitor_set_dpi(uint32_t monitor_id, uint32_t dpi) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return -1;
    m->dpi = dpi;
    m->scale_percent = dpi * 100 / 96;
    return 0;
}

int monitor_set_scale(uint32_t monitor_id, uint32_t scale_percent) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return -1;
    m->scale_percent = scale_percent;
    m->dpi = scale_percent * 96 / 100;
    return 0;
}

int monitor_set_position(uint32_t monitor_id, int32_t x, int32_t y) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return -1;
    m->origin_x = x;
    m->origin_y = y;
    return 0;
}

int monitor_set_primary(uint32_t monitor_id) {
    int i;
    if (monitor_get_info(monitor_id) == NULL) return -1;
    for (i = 0; i < HW_MAX_MONITORS; i++) {
        g_monitors[i].primary = 0;
    }
    g_monitors[monitor_id].primary = 1;
    return 0;
}

int monitor_set_color_space(uint32_t monitor_id, COLOR_SPACE cs) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return -1;
    m->color_space = cs;
    return 0;
}

uint32_t monitor_get_desktop_width(void) {
    int i;
    int32_t min_x = 0x7FFFFFFF;
    int32_t max_x = -0x7FFFFFFF;
    int found = 0;
    for (i = 0; i < HW_MAX_MONITORS; i++) {
        int32_t right;
        if (!g_monitors[i].connected) continue;
        if (g_monitors[i].origin_x < min_x) min_x = g_monitors[i].origin_x;
        right = (int32_t)((uint32_t)g_monitors[i].origin_x + g_monitors[i].width);
        if (right > max_x) max_x = right;
        found = 1;
    }
    if (!found) return 0;
    return (uint32_t)(max_x - min_x);
}

uint32_t monitor_get_desktop_height(void) {
    int i;
    int32_t min_y = 0x7FFFFFFF;
    int32_t max_y = -0x7FFFFFFF;
    int found = 0;
    for (i = 0; i < HW_MAX_MONITORS; i++) {
        int32_t bottom;
        if (!g_monitors[i].connected) continue;
        if (g_monitors[i].origin_y < min_y) min_y = g_monitors[i].origin_y;
        bottom = (int32_t)((uint32_t)g_monitors[i].origin_y + g_monitors[i].height);
        if (bottom > max_y) max_y = bottom;
        found = 1;
    }
    if (!found) return 0;
    return (uint32_t)(max_y - min_y);
}

int monitor_get_at_point(int32_t x, int32_t y) {
    int i;
    for (i = 0; i < HW_MAX_MONITORS; i++) {
        MONITOR_INFO* m = &g_monitors[i];
        int32_t right;
        int32_t bottom;
        if (!m->connected) continue;
        right = (int32_t)((uint32_t)m->origin_x + m->width);
        bottom = (int32_t)((uint32_t)m->origin_y + m->height);
        if (x >= m->origin_x && x < right && y >= m->origin_y && y < bottom) {
            return i;
        }
    }
    return -1;
}

/* ================================================================== *
 * 2. VRR control
 * ================================================================== */

int vrr_enable(uint32_t monitor_id, VRR_MODE mode) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return -1;
    if (!m->vrr_capable) return -1;
    m->vrr.mode = mode;
    m->vrr.active = 1;
    m->vrr.current_refresh_mhz = m->vrr.max_refresh_mhz;
    m->vrr.capability_valid = 1;
    return 0;
}

int vrr_disable(uint32_t monitor_id) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return -1;
    m->vrr.active = 0;
    m->vrr.mode = VRR_DISABLED;
    m->vrr.current_refresh_mhz = m->refresh_rate_mhz;
    return 0;
}

int vrr_set_target_fps(uint32_t monitor_id, uint32_t fps_x1000) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return -1;
    if (!m->vrr.active) return -1;
    if (fps_x1000 < m->vrr.min_refresh_mhz) {
        m->vrr.current_refresh_mhz = m->vrr.min_refresh_mhz;
    } else if (fps_x1000 > m->vrr.max_refresh_mhz) {
        m->vrr.current_refresh_mhz = m->vrr.max_refresh_mhz;
    } else {
        m->vrr.current_refresh_mhz = fps_x1000;
    }
    return 0;
}

VRR_INFO* vrr_get_info(uint32_t monitor_id) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return NULL;
    return &m->vrr;
}

int vrr_is_capable(uint32_t monitor_id) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return 0;
    return m->vrr_capable;
}

uint32_t vrr_get_effective_refresh(uint32_t monitor_id) {
    MONITOR_INFO* m = monitor_get_info(monitor_id);
    if (m == NULL) return 0;
    if (m->vrr.active) {
        return m->vrr.current_refresh_mhz;
    }
    return m->refresh_rate_mhz;
}

/* ================================================================== *
 * 3. Swap chain (triple buffering + page flip)
 * ================================================================== */

int swapchain_create(uint32_t monitor_id, uint32_t width, uint32_t height,
                     uint32_t format, SWAP_EFFECT effect,
                     PRESENT_MODE mode, uint32_t* surface_id) {
    int i;
    int j;
    uint32_t bpp;
    uint64_t buf_size;
    MONITOR_INFO* m;

    (void)effect;

    if (surface_id == NULL) return -1;
    if (width == 0 || height == 0) return -1;

    /* Find a free slot */
    for (i = 0; i < HW_MAX_SURFACES; i++) {
        if (!g_surface_in_use[i]) break;
    }
    if (i >= HW_MAX_SURFACES) return -1;

    /* Bytes per pixel: 4 for BGRA8/RGB10A2, 8 for RGBA16F */
    bpp = (format == 2) ? 8u : 4u;
    buf_size = (uint64_t)width * (uint64_t)height * (uint64_t)bpp;

    m = monitor_get_info(monitor_id);

    {
        SWAP_SURFACE* s = &g_swap_surfaces[i];
        memset(s, 0, sizeof(*s));
        for (j = 0; j < 3; j++) {
            s->buffer[j] = memory_alloc(buf_size);
            if (s->buffer[j] == NULL) {
                int k;
                for (k = 0; k < j; k++) {
                    memory_free(s->buffer[k]);
                    s->buffer[k] = NULL;
                }
                return -1;
            }
            memset(s->buffer[j], 0, (size_t)buf_size);
        }
        s->surface_id = (uint32_t)i;
        s->width = width;
        s->height = height;
        s->pitch = width * bpp;
        s->format = format;
        s->color_space = m ? m->color_space : COLOR_SPACE_SRGB;
        s->current_idx = 0;
        s->displayed_idx = 1;
        s->pending_idx = 2;
        s->pending_flip = 0;
    }

    g_surface_in_use[i] = 1;
    g_surface_count++;
    g_surface_monitor[i] = monitor_id;
    g_present_modes[i] = mode;
    memset(&g_present_stats[i], 0, sizeof(g_present_stats[i]));

    *surface_id = (uint32_t)i;
    return 0;
}

int swapchain_destroy(uint32_t surface_id) {
    SWAP_SURFACE* s;
    int j;
    if (surface_id >= (uint32_t)HW_MAX_SURFACES) return -1;
    if (!g_surface_in_use[surface_id]) return -1;
    s = &g_swap_surfaces[surface_id];
    for (j = 0; j < 3; j++) {
        if (s->buffer[j] != NULL) {
            memory_free(s->buffer[j]);
            s->buffer[j] = NULL;
        }
    }
    g_surface_in_use[surface_id] = 0;
    if (g_surface_count > 0) g_surface_count--;
    return 0;
}

void* swapchain_get_backbuffer(uint32_t surface_id) {
    SWAP_SURFACE* s;
    if (surface_id >= (uint32_t)HW_MAX_SURFACES) return NULL;
    if (!g_surface_in_use[surface_id]) return NULL;
    s = &g_swap_surfaces[surface_id];
    return s->buffer[s->current_idx];
}

int swapchain_present(uint32_t surface_id, uint32_t sync_interval) {
    SWAP_SURFACE* s;
    PRESENT_STATS* st;
    PRESENT_MODE mode;
    uint64_t now;
    uint64_t prev;

    (void)sync_interval;

    if (surface_id >= (uint32_t)HW_MAX_SURFACES) return -1;
    if (!g_surface_in_use[surface_id]) return -1;
    s = &g_swap_surfaces[surface_id];
    st = &g_present_stats[surface_id];
    mode = g_present_modes[surface_id];

    now = rdtsc();
    st->present_count++;

    switch (mode) {
    case PRESENT_MODE_IMMEDIATE:
        /* Immediately swap current and displayed (may cause tearing). */
        {
            uint32_t tmp = s->displayed_idx;
            s->displayed_idx = s->current_idx;
            s->current_idx = tmp;
            s->pending_flip = 0;
        }
        st->frames_torn++;
        st->frames_displayed++;
        break;

    case PRESENT_MODE_VSYNC:
        /* Queue the flip; the actual flip happens at the next vblank. */
        s->pending_idx = s->current_idx;
        s->pending_flip = 1;
        break;

    case PRESENT_MODE_VSYNC_RELAXED:
        /* Like VSYNC, but if we missed vblank (pending already set),
           flip immediately - allow tearing only when late. */
        if (s->pending_flip) {
            do_flip(s);
            st->frames_torn++;
            st->frames_displayed++;
        }
        s->pending_idx = s->current_idx;
        s->pending_flip = 1;
        break;

    case PRESENT_MODE_VRR:
        /* Flip immediately at a variable rate; VRR handles tear-free. */
        {
            uint32_t tmp = s->displayed_idx;
            s->displayed_idx = s->current_idx;
            s->current_idx = tmp;
            s->pending_flip = 0;
        }
        st->frames_displayed++;
        break;

    case PRESENT_MODE_TRIPLE_BUFFER:
        /* Queue the flip and rotate buffers so the producer never stalls.
           The displayed buffer updates at the next vblank from pending. */
        s->pending_idx = s->current_idx;
        s->pending_flip = 1;
        s->current_idx = (s->current_idx + 1) % 3;
        break;

    default:
        /* Unknown mode - treat as VSYNC. */
        s->pending_idx = s->current_idx;
        s->pending_flip = 1;
        break;
    }

    /* Track timing: compute FPS / frame time from the delta between the
       two most recent presents.  rdtsc is used as the time source. */
    prev = st->last_present_time_ns;
    if (prev != 0) {
        uint64_t delta = now - prev;
        if (delta > 0) {
            st->current_frame_time_us = (uint32_t)(delta / 1000ULL);
            st->current_fps = (uint32_t)(1000000000ULL / delta);
        }
    }
    st->last_present_time_ns = now;
    {
        uint32_t mon = g_surface_monitor[surface_id];
        if (mon < (uint32_t)HW_MAX_MONITORS) {
            st->last_vblank_time_ns = g_vblank_last_time[mon];
            st->vrr_effective_hz = (float)vrr_get_effective_refresh(mon) / 1000.0f;
        }
    }

    return 0;
}

int swapchain_set_present_mode(uint32_t surface_id, PRESENT_MODE mode) {
    if (surface_id >= (uint32_t)HW_MAX_SURFACES) return -1;
    if (!g_surface_in_use[surface_id]) return -1;
    g_present_modes[surface_id] = mode;
    return 0;
}

int swapchain_get_stats(uint32_t surface_id, PRESENT_STATS* stats) {
    if (stats == NULL) return -1;
    if (surface_id >= (uint32_t)HW_MAX_SURFACES) return -1;
    if (!g_surface_in_use[surface_id]) return -1;
    *stats = g_present_stats[surface_id];
    return 0;
}

int swapchain_wait_for_vblank(uint32_t surface_id) {
    uint32_t mon;
    if (surface_id >= (uint32_t)HW_MAX_SURFACES) return -1;
    if (!g_surface_in_use[surface_id]) return -1;
    mon = g_surface_monitor[surface_id] % (uint32_t)HW_MAX_MONITORS;
    /* Simulate waiting for vblank by incrementing the vblank counter. */
    g_vblank_count[mon]++;
    g_vblank_last_time[mon] = rdtsc();
    /* Process pending flip for this surface. */
    if (g_swap_surfaces[surface_id].pending_flip) {
        do_flip(&g_swap_surfaces[surface_id]);
        g_present_stats[surface_id].frames_displayed++;
    }
    return 0;
}

/* ================================================================== *
 * 4. VBLANK tracking
 * ================================================================== */

uint64_t vblank_get_last_time(uint32_t monitor_id) {
    if (monitor_id >= (uint32_t)HW_MAX_MONITORS) return 0;
    return g_vblank_last_time[monitor_id];
}

uint32_t vblank_get_count(uint32_t monitor_id) {
    if (monitor_id >= (uint32_t)HW_MAX_MONITORS) return 0;
    return g_vblank_count[monitor_id];
}

int vblank_register_callback(uint32_t monitor_id,
                              void (*cb)(uint32_t monitor_id, uint64_t time_ns)) {
    if (monitor_id >= (uint32_t)HW_MAX_MONITORS) return -1;
    g_vblank_callbacks[monitor_id] = cb;
    return 0;
}

void vblank_signal(uint32_t monitor_id) {
    if (monitor_id >= (uint32_t)HW_MAX_MONITORS) return;
    g_vblank_count[monitor_id]++;
    g_vblank_last_time[monitor_id] = rdtsc();
    /* Process pending flips for surfaces on this monitor via callback. */
    if (g_vblank_callbacks[monitor_id]) {
        g_vblank_callbacks[monitor_id](monitor_id, g_vblank_last_time[monitor_id]);
    }
}

/* ================================================================== *
 * 5. Full pipeline init
 * ================================================================== */

int hw_display_init(void) {
    /* Initialize monitors */
    monitor_init();
    /* Initialize swap chains */
    memset(g_swap_surfaces, 0, sizeof(g_swap_surfaces));
    memset(g_present_stats, 0, sizeof(g_present_stats));
    memset(g_present_modes, 0, sizeof(g_present_modes));
    memset(g_surface_in_use, 0, sizeof(g_surface_in_use));
    memset(g_surface_monitor, 0, sizeof(g_surface_monitor));
    g_surface_count = 0;
    /* Initialize VBLANK */
    memset(g_vblank_count, 0, sizeof(g_vblank_count));
    memset(g_vblank_last_time, 0, sizeof(g_vblank_last_time));
    memset(g_vblank_callbacks, 0, sizeof(g_vblank_callbacks));
    return 0;
}
