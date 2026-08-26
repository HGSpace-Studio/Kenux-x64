/*
 * Display / GDI driver - freestanding kernel implementation.
 *
 * Provides a simple linear framebuffer display driver with the basic
 * drawing primitives (set/get pixel, fill rect, clear, blit) backed by
 * a single static DISPLAY_DRIVER_DATA structure.  All framebuffer
 * accesses go through a volatile pointer so writes are not optimised
 * away by the compiler.
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

extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* ptr);

/* ------------------------------------------------------------------ *
 * Driver state
 * ------------------------------------------------------------------ */
static DISPLAY_DRIVER_DATA g_display;

/* Convert (x,y) to a volatile framebuffer pointer.  Uses the virtual
 * framebuffer address when available, falling back to the physical
 * address otherwise.  Assumes a 32-bpp packed framebuffer. */
static inline volatile uint32_t* pixel_addr(int x, int y) {
    uintptr_t base = (uintptr_t)g_display.framebuffer_virt;
    if (base == 0) base = (uintptr_t)g_display.framebuffer_phys;
    return (volatile uint32_t*)(base + (y * g_display.pitch + x) * 4);
}

int display_driver_init(void) {
    g_display.width = 1024;
    g_display.height = 768;
    g_display.bpp = 32;
    g_display.format = 0;                 /* 0 = BGRA */
    g_display.pitch = g_display.width * (g_display.bpp / 8);
    g_display.framebuffer_phys = 0;
    g_display.framebuffer_virt = NULL;
    g_display.screen_stride = g_display.pitch;
    g_display.initialized = 1;
    return 0;
}

void display_set_pixel(int x, int y, uint32_t color) {
    if (!g_display.initialized) return;
    if (x < 0 || y < 0 ||
        (uint32_t)x >= g_display.width || (uint32_t)y >= g_display.height)
        return;
    *pixel_addr(x, y) = color;
}

void display_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_display.initialized || w <= 0 || h <= 0) return;
    int x1 = x + w;
    int y1 = y + h;
    for (int yy = y; yy < y1; yy++) {
        for (int xx = x; xx < x1; xx++) {
            display_set_pixel(xx, yy, color);
        }
    }
}

void display_clear(uint32_t color) {
    display_fill_rect(0, 0, (int)g_display.width, (int)g_display.height, color);
}

void display_blit(int x, int y, int w, int h, const void* data) {
    if (!g_display.initialized || data == NULL || w <= 0 || h <= 0) return;
    const uint32_t* src = (const uint32_t*)data;
    uint32_t src_pitch = (uint32_t)w;     /* pixels per row in source */
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            display_set_pixel(x + xx, y + yy, src[yy * src_pitch + xx]);
        }
    }
}

uint32_t display_get_pixel(int x, int y) {
    if (!g_display.initialized) return 0;
    if (x < 0 || y < 0 ||
        (uint32_t)x >= g_display.width || (uint32_t)y >= g_display.height)
        return 0;
    return *pixel_addr(x, y);
}

DISPLAY_DRIVER_DATA* display_get_info(void) {
    return &g_display;
}

int display_set_mode(uint32_t width, uint32_t height, uint32_t bpp) {
    g_display.width = width;
    g_display.height = height;
    g_display.bpp = bpp;
    g_display.pitch = width * (bpp / 8);
    return 0;
}
