#ifndef KAPI_LOGGING_H
#define KAPI_LOGGING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log levels */
#define KAPI_LOG_EMERG     0  /* Emergency: system is unusable */
#define KAPI_LOG_ALERT     1  /* Alert: action must be taken immediately */
#define KAPI_LOG_CRIT      2  /* Critical: critical conditions */
#define KAPI_LOG_ERR       3  /* Error: error conditions */
#define KAPI_LOG_WARN      4  /* Warning: warning conditions */
#define KAPI_LOG_NOTICE    5  /* Notice: normal but significant condition */
#define KAPI_LOG_INFO      6  /* Informational: normal informational message */
#define KAPI_LOG_DEBUG     7  /* Debug: debug-level messages */

/* Log targets */
#define KAPI_LOG_CONSOLE   0x01  /* Output to console */
#define KAPI_LOG_SERIAL    0x02  /* Output to serial port */
#define KAPI_LOG_FILE      0x04  /* Output to file */
#define KAPI_LOG_SYSLOG    0x08  /* Output to system log */
#define KAPI_LOG_NETWORK   0x10  /* Output to network log server */
#define KAPI_LOG_RINGBUF   0x20  /* Store in ring buffer */

/* Log flags */
#define KAPI_LOG_PRINT_TIME   0x01  /* Include timestamp */
#define KAPI_LOG_PRINT_PID    0x02  /* Include process ID */
#define KAPI_LOG_PRINT_TID    0x04  /* Include thread ID */
#define KAPI_LOG_PRINT_LEVEL  0x08  /* Include log level */
#define KAPI_LOG_PRINT_FILE   0x10  /* Include file information */
#define KAPI_LOG_PRINT_FUNC   0x20  /* Include function name */
#define KAPI_LOG_PRINT_LINE   0x40  /* Include line number */
#define KAPI_LOG_PRINT_COLOR  0x80  /* Include color codes */

/* Log format specifiers */
#define KAPI_LOG_TIME_FORMAT "[%Y-%m-%d %H:%M:%S] "
#define KAPI_LOG_PID_FMT "[PID:%d] "
#define KAPI_LOG_TID_FMT "[TID:%d] "
#define KAPI_LOG_LEVEL_FMT "[%s] "
#define KAPI_LOG_FILE_FMT "(%s:%s:%d) "

/* Log facility types */
#define KAPI_LOG_KERN      0   /* Kernel messages */
#define KAPI_LOG_USER      1   /* User-level messages */
#define KAPI_LOG_MAIL      2   /* Mail system */
#define KAPI_LOG_DAEMON    3   /* System daemons */
#define KAPI_LOG_AUTH      4   /* Security/authentication */
#define KAPI_LOG_SYSLOG    5   /* Syslog messages */
#define KAPI_LOG_LPR       6   /* Line printer */
#define KAPI_LOG_NEWS      7   /* News system */
#define KAPI_LOG_UUCP      8   /* UUCP system */
#define KAPI_LOG_CRON      9   /* Cron/at facility */
#define KAPI_LOG_AUTHPRIV 10   /* Security/auth (private) */

typedef struct kapi_logger kapi_logger_t;

/* Log message structure */
typedef struct {
    uint64_t timestamp;
    uint32_t pid;
    uint32_t tid;
    uint32_t level;
    uint32_t facility;
    char     filename[256];
    char     function[128];
    int      line;
    char     message[1024];
} kapi_log_message_t;

/* Log handler function type */
typedef void (*kapi_log_handler_t)(const kapi_log_message_t* message, void* user_data);

/* Logger configuration structure */
typedef struct {
    uint32_t level;          /* Minimum log level */
    uint32_t targets;        /* Output targets */
    uint32_t flags;          /* Output flags */
    uint32_t facility;       /* Log facility */
    char     prefix[64];     /* Message prefix */
    char     file_path[512]; /* Log file path */
    char     hostname[64];   /* Hostname for network logging */
    uint16_t port;           /* Port for network logging */
    kapi_log_handler_t custom_handler; /* Custom log handler */
    void*    user_data;      /* User data for custom handler */
} kapi_logger_config_t;

/* Logger API */
int kapi_logging_init(const kapi_logger_config_t* config);
void kapi_logging_cleanup(void);

int kapi_logging_reconfigure(const kapi_logger_config_t* config);

/* Logging functions */
void kapi_log_write(uint32_t level, const char* fmt, ...);
void kapi_log_emerg(const char* fmt, ...);
void kapi_log_alert(const char* fmt, ...);
void kapi_log_crit(const char* fmt, ...);
void kapi_log_err(const char* fmt, ...);
void kapi_log_warn(const char* fmt, ...);
void kapi_log_notice(const char* fmt, ...);
void kapi_log_info(const char* fmt, ...);
void kapi_log_debug(const char* fmt, ...);

/* Advanced logging */
void kapi_log_write_ext(uint32_t level, uint32_t facility, 
                       const char* file, const char* func, int line,
                       const char* fmt, ...);

void kapi_log_stack_trace(uint32_t level);
void kapi_log_memory_usage(uint32_t level);

/* Logger management */
int kapi_logger_register_handler(kapi_log_handler_t handler, void* user_data);
int kapi_logger_unregister_handler(kapi_log_handler_t handler);

int kapi_logger_set_level(uint32_t level);
int kapi_logger_get_level(uint32_t* level);

int kapi_logger_set_facility(uint32_t facility);
int kapi_logger_get_facility(uint32_t* facility);

int kapi_logger_add_target(uint32_t targets);
int kapi_logger_remove_target(uint32_t targets);

int kapi_logger_flush(void);

/* Ring buffer for log storage */
typedef struct {
    kapi_log_message_t* messages;
    size_t              capacity;
    size_t              head;
    size_t              tail;
    size_t              count;
} kapi_log_ringbuf_t;

int kapi_log_ringbuf_create(kapi_log_ringbuf_t* rb, size_t capacity);
void kapi_log_ringbuf_destroy(kapi_log_ringbuf_t* rb);
int kapi_log_ringbuf_put(kapi_log_ringbuf_t* rb, const kapi_log_message_t* msg);
int kapi_log_ringbuf_get(kapi_log_ringbuf_t* rb, kapi_log_message_t* msg);
int kapi_log_ringbuf_peek(kapi_log_ringbuf_t* rb, kapi_log_message_t* msg);
int kapi_log_ringbuf_iterate(kapi_log_ringbuf_t* rb, 
                             int (*callback)(const kapi_log_message_t*, void*), 
                             void* user_data);

/* Log rotation */
typedef struct {
    size_t max_size;        /* Maximum log file size (bytes) */
    int    max_files;       /* Maximum number of rotated files */
    char   file_prefix[256]; /* Log file prefix */
    int    compress;         /* Compress rotated files */
} kapi_log_rotation_config_t;

int kapi_logging_enable_rotation(const kapi_log_rotation_config_t* config);
int kapi_logging_disable_rotation(void);

/* Performance monitoring */
typedef struct {
    uint64_t total_messages;
    uint64_t messages_by_level[8];
    uint64_t messages_by_facility[16];
    uint64_t disk_bytes_written;
    uint64_t network_bytes_sent;
    uint32_t active_loggers;
    uint32_t ringbuf_usage;
} kapi_log_stats_t;

int kapi_logging_get_stats(kapi_log_stats_t* stats);

/* Debug logging utilities */
#define kapi_log_debug_hex(level, data, len) \
    kapi_log_debug_hex_data(level, data, len, 16, " ")

void kapi_log_debug_hex_data(uint32_t level, const uint8_t* data, size_t len,
                           int bytes_per_line, const char* prefix);

#define kapi_log_pointer(level, ptr) \
    kapi_log_debug("Pointer: %p", ptr)

#ifdef __cplusplus
}
#endif

#endif