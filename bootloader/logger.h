#ifndef KEGD_LOGGER_H
#define KEGD_LOGGER_H

#include "graphics.h"
#include <stdarg.h>

typedef enum {
    LOG_INFO,
    LOG_SUCCESS,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

void init_logger(screen_t *scr);

void log_message(log_level_t level, const char *format, ...);

uint32_t get_log_height(void);

void get_log_color(log_level_t level, uint8_t *r, uint8_t *g, uint8_t *b);

#endif