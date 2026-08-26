#include "kapi_logging.h"
#include "kapi_memory.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* Logger configuration */
static kapi_logger_config_t logger_config = {
    .level = KAPI_LOG_INFO,
    .targets = KAPI_LOG_CONSOLE,
    .flags = KAPI_LOG_PRINT_TIME | KAPI_LOG_PRINT_LEVEL,
    .facility = KAPI_LOG_KERN
};

/* Ring buffer for log messages */
static kapi_log_ringbuf_t* log_ringbuf = NULL;
static kapi_log_handler_t custom_handler = NULL;
static void* user_data = NULL;

/* Log level names */
static const char* log_level_names[] = {
    "EMERG", "ALERT", "CRIT", "ERR", "WARN", "NOTICE", "INFO", "DEBUG"
};

/* Log facility names */
static const char* log_facility_names[] = {
    "KERN", "USER", "MAIL", "DAEMON", "AUTH", "SYSLOG", "LPR", "NEWS", 
    "UUCP", "CRON", "AUTHPRIV"
};

/* Internal functions */
static void log_write_internal(uint32_t level, uint32_t facility,
                             const char* file, const char* func, int line,
                             const char* fmt, va_list args) {
    kapi_log_message_t msg;
    
    /* Check if message level meets minimum threshold */
    if (level < logger_config.level) {
        return;
    }
    
    /* Initialize message */
    memset(&msg, 0, sizeof(msg));
    msg.timestamp = 0; // Would be filled with actual timestamp
    msg.level = level;
    msg.facility = facility;
    msg.pid = 0; // Would be filled with actual PID
    msg.tid = 0; // Would be filled with actual TID
    
    /* Set file/function/line information if available */
    if (file) {
        strncpy(msg.filename, file, sizeof(msg.filename) - 1);
    }
    if (func) {
        strncpy(msg.function, func, sizeof(msg.function) - 1);
    }
    msg.line = line;
    
    /* Format message */
    vsnprintf(msg.message, sizeof(msg.message), fmt, args);
    
    /* Call custom handler if registered */
    if (custom_handler) {
        custom_handler(&msg, user_data);
    }
    
    /* Write to log targets */
    if (logger_config.targets & KAPI_LOG_CONSOLE) {
        log_write_console(&msg);
    }
    if (logger_config.targets & KAPI_LOG_SERIAL) {
        log_write_serial(&msg);
    }
    if (logger_config.targets & KAPI_LOG_FILE) {
        log_write_file(&msg);
    }
    
    /* Store in ring buffer */
    if (log_ringbuf) {
        kapi_log_ringbuf_put(log_ringbuf, &msg);
    }
}

static void log_write_console(const kapi_log_message_t* msg) {
    char prefix[256] = "";
    char output[2048];
    
    /* Build prefix based on flags */
    if (logger_config.flags & KAPI_LOG_PRINT_TIME) {
        char time_str[32];
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        strftime(time_str, sizeof(time_str), KAPI_LOG_TIME_FORMAT, tm_info);
        strcat(prefix, time_str);
    }
    
    if (logger_config.flags & KAPI_LOG_PRINT_LEVEL) {
        strcat(prefix, KAPI_LOG_LEVEL_FMT);
    }
    
    if (logger_config.flags & KAPI_LOG_PRINT_PID) {
        strcat(prefix, KAPI_LOG_PID_FMT);
    }
    
    if (logger_config.flags & KAPI_LOG_PRINT_TID) {
        strcat(prefix, KAPI_LOG_TID_FMT);
    }
    
    if (logger_config.flags & KAPI_LOG_PRINT_FILE) {
        strcat(prefix, KAPI_LOG_FILE_FMT);
    }
    
    /* Format output */
    snprintf(output, sizeof(output), prefix, 
             msg->level >= 8 ? "UNK" : log_level_names[msg->level],
             msg->pid, msg->tid, msg->filename, msg->function, msg->line,
             msg->message);
    
    /* Output to console */
    // In a real implementation, this would use console output functions
    // For now, using standard printf
    printf("%s", output);
}

static void log_write_serial(const kapi_log_message_t* msg) {
    /* Serial logging would be implemented here */
    /* This would output to serial port for debugging */
}

static void log_write_file(const kapi_log_message_t* msg) {
    if (!logger_config.file_path[0]) {
        return;
    }
    
    /* File logging would be implemented here */
    /* This would write to the configured log file */
}

/* Public API implementation */
int kapi_logging_init(const kapi_logger_config_t* config) {
    if (!config) {
        return -1;
    }
    
    /* Copy configuration */
    memcpy(&logger_config, config, sizeof(logger_config));
    
    /* Initialize ring buffer */
    if (config->targets & KAPI_LOG_RINGBUF) {
        log_ringbuf = (kapi_log_ringbuf_t*)kapi_malloc(sizeof(kapi_log_ringbuf_t));
        if (!log_ringbuf) {
            return -1;
        }
        
        /* Create ring buffer with default size */
        if (kapi_log_ringbuf_create(log_ringbuf, 1024) != 0) {
            kapi_free(log_ringbuf);
            log_ringbuf = NULL;
            return -1;
        }
    }
    
    kapi_log_info("Logging system initialized with level %d, targets %u", 
                 config->level, config->targets);
    
    return 0;
}

void kapi_logging_cleanup(void) {
    if (log_ringbuf) {
        kapi_log_ringbuf_destroy(log_ringbuf);
        kapi_free(log_ringbuf);
        log_ringbuf = NULL;
    }
    
    custom_handler = NULL;
    user_data = NULL;
}

int kapi_logging_reconfigure(const kapi_logger_config_t* config) {
    if (!config) {
        return -1;
    }
    
    /* Update configuration */
    memcpy(&logger_config, config, sizeof(logger_config));
    
    /* Recreate ring buffer if needed */
    if (config->targets & KAPI_LOG_RINGBUF && !log_ringbuf) {
        log_ringbuf = (kapi_log_ringbuf_t*)kapi_malloc(sizeof(kapi_log_ringbuf_t));
        if (!log_ringbuf) {
            return -1;
        }
        
        if (kapi_log_ringbuf_create(log_ringbuf, 1024) != 0) {
            kapi_free(log_ringbuf);
            log_ringbuf = NULL;
            return -1;
        }
    }
    
    /* Destroy ring buffer if no longer needed */
    if (!(config->targets & KAPI_LOG_RINGBUF) && log_ringbuf) {
        kapi_log_ringbuf_destroy(log_ringbuf);
        kapi_free(log_ringbuf);
        log_ringbuf = NULL;
    }
    
    kapi_log_info("Logging system reconfigured with level %d, targets %u", 
                 config->level, config->targets);
    
    return 0;
}

/* Logging functions */
void kapi_log_write(uint32_t level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(level, logger_config.facility, NULL, NULL, 0, fmt, args);
    va_end(args);
}

void kapi_log_emerg(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(KAPI_LOG_EMERG, logger_config.facility, NULL, NULL, 0, fmt, args);
    va_end(args);
}

void kapi_log_alert(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(KAPI_LOG_ALERT, logger_config.facility, NULL, NULL, 0, fmt, args);
    va_end(args);
}

void kapi_log_crit(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(KAPI_LOG_CRIT, logger_config.facility, NULL, NULL, 0, fmt, args);
    va_end(args);
}

void kapi_log_err(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(KAPI_LOG_ERR, logger_config.facility, NULL, NULL, 0, fmt, args);
    va_end(args);
}

void kapi_log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(KAPI_LOG_WARN, logger_config.facility, NULL, NULL, 0, fmt, args);
    va_end(args);
}

void kapi_log_notice(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(KAPI_LOG_NOTICE, logger_config.facility, NULL, NULL, 0, fmt, args);
    va_end(args);
}

void kapi_log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(KAPI_LOG_INFO, logger_config.facility, NULL, NULL, 0, fmt, args);
    va_end(args);
}

void kapi_log_debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(KAPI_LOG_DEBUG, logger_config.facility, NULL, NULL, 0, fmt, args);
    va_end(args);
}

/* Advanced logging */
void kapi_log_write_ext(uint32_t level, uint32_t facility, 
                       const char* file, const char* func, int line,
                       const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_internal(level, facility, file, func, line, fmt, args);
    va_end(args);
}

void kapi_log_stack_trace(uint32_t level) {
    /* Stack trace logging would be implemented here */
    kapi_log_write_ext(level, logger_config.facility, __FILE__, __func__, __LINE__,
                      "Stack trace: (would be implemented)");
}

void kapi_log_memory_usage(uint32_t level) {
    /* Memory usage logging would be implemented here */
    kapi_log_write_ext(level, logger_config.facility, __FILE__, __func__, __LINE__,
                      "Memory usage: (would be implemented)");
}

/* Logger management */
int kapi_logger_register_handler(kapi_log_handler_t handler, void* data) {
    custom_handler = handler;
    user_data = data;
    return 0;
}

int kapi_logger_unregister_handler(kapi_log_handler_t handler) {
    if (custom_handler == handler) {
        custom_handler = NULL;
        user_data = NULL;
        return 0;
    }
    return -1;
}

int kapi_logger_set_level(uint32_t level) {
    if (level > KAPI_LOG_DEBUG) {
        return -1;
    }
    logger_config.level = level;
    return 0;
}

int kapi_logger_get_level(uint32_t* level) {
    if (!level) {
        return -1;
    }
    *level = logger_config.level;
    return 0;
}

int kapi_logger_set_facility(uint32_t facility) {
    if (facility > KAPI_LOG_AUTHPRIV) {
        return -1;
    }
    logger_config.facility = facility;
    return 0;
}

int kapi_logger_get_facility(uint32_t* facility) {
    if (!facility) {
        return -1;
    }
    *facility = logger_config.facility;
    return 0;
}

int kapi_logger_add_target(uint32_t targets) {
    logger_config.targets |= targets;
    return 0;
}

int kapi_logger_remove_target(uint32_t targets) {
    logger_config.targets &= ~targets;
    return 0;
}

int kapi_logger_flush(void) {
    /* Flush pending log messages */
    if (logger_config.targets & KAPI_LOG_FILE) {
        /* Flush file buffers */
    }
    return 0;
}

/* Ring buffer implementation */
int kapi_log_ringbuf_create(kapi_log_ringbuf_t* rb, size_t capacity) {
    if (!rb || capacity == 0) {
        return -1;
    }
    
    rb->messages = (kapi_log_message_t*)kapi_malloc(capacity * sizeof(kapi_log_message_t));
    if (!rb->messages) {
        return -1;
    }
    
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    
    return 0;
}

void kapi_log_ringbuf_destroy(kapi_log_ringbuf_t* rb) {
    if (rb) {
        if (rb->messages) {
            kapi_free(rb->messages);
        }
    }
}

int kapi_log_ringbuf_put(kapi_log_ringbuf_t* rb, const kapi_log_message_t* msg) {
    if (!rb || !msg || !rb->messages) {
        return -1;
    }
    
    /* Check if ring buffer is full */
    if (rb->count >= rb->capacity) {
        /* Remove oldest message */
        rb->head = (rb->head + 1) % rb->capacity;
        rb->count--;
    }
    
    /* Copy message */
    memcpy(&rb->messages[rb->tail], msg, sizeof(kapi_log_message_t));
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    
    return 0;
}

int kapi_log_ringbuf_get(kapi_log_ringbuf_t* rb, kapi_log_message_t* msg) {
    if (!rb || !msg || rb->count == 0) {
        return -1;
    }
    
    /* Copy message */
    memcpy(msg, &rb->messages[rb->head], sizeof(kapi_log_message_t));
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    
    return 0;
}

int kapi_log_ringbuf_peek(kapi_log_ringbuf_t* rb, kapi_log_message_t* msg) {
    if (!rb || !msg || rb->count == 0) {
        return -1;
    }
    
    /* Copy message without removing it */
    memcpy(msg, &rb->messages[rb->head], sizeof(kapi_log_message_t));
    
    return 0;
}

int kapi_log_ringbuf_iterate(kapi_log_ringbuf_t* rb, 
                             int (*callback)(const kapi_log_message_t*, void*), 
                             void* user_data) {
    if (!rb || !callback || rb->count == 0) {
        return -1;
    }
    
    size_t current = rb->head;
    for (size_t i = 0; i < rb->count; i++) {
        if (callback(&rb->messages[current], user_data) != 0) {
            return -1;
        }
        current = (current + 1) % rb->capacity;
    }
    
    return 0;
}

/* Debug logging utilities */
void kapi_log_debug_hex_data(uint32_t level, const uint8_t* data, size_t len,
                           int bytes_per_line, const char* prefix) {
    if (!data || len == 0) {
        return;
    }
    
    kapi_log_write_ext(level, logger_config.facility, __FILE__, __func__, __LINE__,
                      "%sBinary data (len=%zu, %d per line):", prefix, len, bytes_per_line);
    
    for (size_t i = 0; i < len; i += bytes_per_line) {
        char ascii[32] = "";
        char hex[64] = "";
        
        for (int j = 0; j < bytes_per_line && (i + j) < len; j++) {
            char byte_str[4];
            snprintf(byte_str, sizeof(byte_str), "%02x ", data[i + j]);
            strcat(hex, byte_str);
            
            if (data[i + j] >= 32 && data[i + j] <= 126) {
                ascii[j] = data[i + j];
            } else {
                ascii[j] = '.';
            }
        }
        
        ascii[bytes_per_line] = '\0';
        kapi_log_write_ext(level, logger_config.facility, __FILE__, __func__, __LINE__,
                          "%s%s  %s", prefix, hex, ascii);
    }
}