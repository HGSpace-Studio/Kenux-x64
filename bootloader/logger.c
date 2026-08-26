#include <stddef.h>
#include "logger.h"

extern void *memcpy(void *dest, const void *src, uint64_t n);

static screen_t *log_screen = NULL;
static uint32_t log_y = 0;
static uint32_t log_start_y = 0;
static const uint32_t LOG_FONT_HEIGHT = 20;
static const uint32_t LOG_PADDING = 10;

void get_log_color(log_level_t level, uint8_t *r, uint8_t *g, uint8_t *b) {
    switch (level) {
        case LOG_INFO:    *r = 255; *g = 255; *b = 255; break;
        case LOG_SUCCESS: *r = 0;   *g = 255; *b = 0;   break;
        case LOG_WARNING: *r = 255; *g = 200; *b = 0;   break;
        case LOG_ERROR:   *r = 255; *g = 50;  *b = 50;  break;
    }
}

void init_logger(screen_t *scr) {
    log_screen = scr;
    log_start_y = scr->height - 200;
    log_y = log_start_y;
}

uint32_t get_log_height(void) {
    return log_y;
}

void log_message(log_level_t level, const char *format, ...) {
    if (!log_screen) return;
    
    uint8_t r = 255, g = 255, b = 255;
    get_log_color(level, &r, &g, &b);
    
    const char *prefix = "";
    switch (level) {
        case LOG_INFO:    prefix = "[INFO] "; break;
        case LOG_SUCCESS: prefix = "[OK] "; break;
        case LOG_WARNING: prefix = "[WARN] "; break;
        case LOG_ERROR:   prefix = "[ERROR] "; break;
    }
    
    if (log_y >= log_screen->height - LOG_FONT_HEIGHT) {
        uint32_t copy_height = log_start_y - LOG_FONT_HEIGHT;
        uint32_t pixels_to_copy = copy_height * log_screen->pixels_per_scanline * 4;
        volatile uint8_t *src = (volatile uint8_t*)(log_screen->frame_buffer);
        volatile uint8_t *dst = (volatile uint8_t*)(log_screen->frame_buffer + log_screen->pixels_per_scanline * 4 * LOG_FONT_HEIGHT);
        for (uint64_t i = 0; i < pixels_to_copy; i++) {
            dst[i] = src[i];
        }
        log_y = log_start_y;
    }
    
    draw_string(log_screen, LOG_PADDING, log_y, prefix, r, g, b);
    
    char buffer[256];
    const char *p = format;
    int buf_pos = 0;
    while (*p && buf_pos < 255) {
        buffer[buf_pos++] = *p++;
    }
    buffer[buf_pos] = '\0';
    
    draw_string(log_screen, LOG_PADDING + 8 * 7, log_y, buffer, r, g, b);
    
    log_y += LOG_FONT_HEIGHT;
}