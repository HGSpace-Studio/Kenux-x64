#ifndef _KAPI_UNIFIED_LOG_H
#define _KAPI_UNIFIED_LOG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Unified kernel/user log system.
 *
 * Fuses the kernel printk stream with structured user-space log records into a
 * single ring buffer with multiple output sinks (ring buffer, serial, VGA,
 * file). Records carry a timestamp, level, domain, PID and a formatted message.
 * A per-sink and global filter gates records by level and domain bitmask.
 * Subscribers receive each accepted record through a callback.
 *
 * Freestanding: all storage is statically sized, no dynamic allocation.
 */

#define KAPI_ULOG_RING_SIZE     1024    /* Ring buffer entry count           */
#define KAPI_ULOG_MAX_SINKS     8       /* Max registered output sinks       */
#define KAPI_ULOG_MAX_SUBS      8       /* Max subscribers                    */
#define KAPI_ULOG_MSG_LEN       256      /* Max message length per entry      */

/* ---- Log levels (0 = most severe) ------------------------------------ */
#define KAPI_ULOG_LEVEL_EMERG   0
#define KAPI_ULOG_LEVEL_ALERT   1
#define KAPI_ULOG_LEVEL_CRIT    2
#define KAPI_ULOG_LEVEL_ERR     3
#define KAPI_ULOG_LEVEL_WARN    4
#define KAPI_ULOG_LEVEL_NOTICE  5
#define KAPI_ULOG_LEVEL_INFO    6
#define KAPI_ULOG_LEVEL_DEBUG   7

/* ---- Log domains ------------------------------------------------------ */
typedef enum {
    KAPI_ULOG_DOMAIN_KERNEL = 0,
    KAPI_ULOG_DOMAIN_USER,
    KAPI_ULOG_DOMAIN_SYSCALL,
    KAPI_ULOG_DOMAIN_IRQ,
    KAPI_ULOG_DOMAIN_DRIVER,
    KAPI_ULOG_DOMAIN_FS,
    KAPI_ULOG_DOMAIN_NET,
    KAPI_ULOG_DOMAIN_SECURITY,
    KAPI_ULOG_DOMAIN_MAX,
} kapi_ulog_domain_t;

/* Convenience domain bitmask helpers. */
#define KAPI_ULOG_DOM_BIT(d)   (1UL << (d))
#define KAPI_ULOG_DOM_ALL      ((uint32_t)((1UL << KAPI_ULOG_DOMAIN_MAX) - 1))

/* ---- Sink types ------------------------------------------------------- */
typedef enum {
    KAPI_ULOG_SINK_RING = 0,    /* Ring buffer (always implicit) */
    KAPI_ULOG_SINK_SERIAL,      /* Serial port output           */
    KAPI_ULOG_SINK_VGA,         /* VGA console output           */
    KAPI_ULOG_SINK_FILE,        /* File backed (stub in kernel) */
} kapi_ulog_sink_type_t;

/* ---- Structured log entry --------------------------------------------- */
typedef struct kapi_ulog_entry {
    uint64_t   timestamp;                 /* TSC or kernel tick          */
    uint8_t    level;                     /* Log level 0..7              */
    uint8_t    domain;                    /* Log domain                   */
    uint32_t   pid;                       /* Process/thread id (0=kernel) */
    char       message[KAPI_ULOG_MSG_LEN];/* Formatted message           */
} kapi_ulog_entry_t;

/* ---- Log filter (by level + domain bitmask) --------------------------- */
typedef struct kapi_ulog_filter {
    uint8_t   min_level;     /* Pass records with level <= min_level        */
    uint32_t  domain_mask;   /* Bitmask of accepted domains                 */
    int       enabled;       /* Master switch (0 = pass all, 1 = filter)   */
} kapi_ulog_filter_t;

/* Sink emit callback: receives an accepted entry. */
typedef void (*kapi_ulog_sink_fn)(const kapi_ulog_entry_t* entry, void* user_data);

/* ---- Output sink ------------------------------------------------------ */
typedef struct kapi_ulog_sink {
    int                 in_use;
    kapi_ulog_sink_type_t type;
    kapi_ulog_sink_fn   emit;             /* Delivery callback           */
    void*               user_data;        /* Caller context              */
    kapi_ulog_filter_t  filter;           /* Per-sink gate               */
} kapi_ulog_sink_t;

/* Subscriber callback (same shape as a sink emit). */
typedef kapi_ulog_sink_fn kapi_ulog_callback_t;

/* ---- API -------------------------------------------------------------- */

/* Initialize the unified log subsystem (clears ring, sinks, subscribers). */
void kapi_ulog_init(void);

/* Core logging routine. Formats the message, stores it in the ring buffer,
 * dispatches to all registered/accepted sinks, notifies subscribers and
 * mirrors to printk for console output. */
void kapi_ulog_log(uint8_t level, uint8_t domain, const char* fmt, ...);

/* Subscribe a callback. Returns subscription id (>=0) or -1. */
int kapi_ulog_subscribe(kapi_ulog_callback_t cb, void* user_data,
                        const kapi_ulog_filter_t* filter);

/* Install the global filter (NULL resets to "pass everything"). */
void kapi_ulog_set_filter(const kapi_ulog_filter_t* filter);

/* Drain all pending records to sinks (best-effort, immediate). */
void kapi_ulog_flush(void);

/* Read up to `count` entries from the ring buffer starting at the oldest
 * unread record. Returns the number of entries copied. */
int kapi_ulog_read(kapi_ulog_entry_t* out, int count);

/* Register an output sink. Returns sink id (>=0) or -1. */
int kapi_ulog_add_sink(kapi_ulog_sink_type_t type, kapi_ulog_sink_fn emit,
                       void* user_data, const kapi_ulog_filter_t* filter);

/* ---- Convenience level macros (take a domain argument) ---------------- */
#define KAPI_ULOG_EMERG(domain, fmt, ...)   \
    kapi_ulog_log(KAPI_ULOG_LEVEL_EMERG,  (uint8_t)(domain), (fmt), ##__VA_ARGS__)
#define KAPI_ULOG_ALERT(domain, fmt, ...)   \
    kapi_ulog_log(KAPI_ULOG_LEVEL_ALERT,  (uint8_t)(domain), (fmt), ##__VA_ARGS__)
#define KAPI_ULOG_CRIT(domain, fmt, ...)    \
    kapi_ulog_log(KAPI_ULOG_LEVEL_CRIT,   (uint8_t)(domain), (fmt), ##__VA_ARGS__)
#define KAPI_ULOG_ERR(domain, fmt, ...)     \
    kapi_ulog_log(KAPI_ULOG_LEVEL_ERR,    (uint8_t)(domain), (fmt), ##__VA_ARGS__)
#define KAPI_ULOG_WARN(domain, fmt, ...)    \
    kapi_ulog_log(KAPI_ULOG_LEVEL_WARN,   (uint8_t)(domain), (fmt), ##__VA_ARGS__)
#define KAPI_ULOG_NOTICE(domain, fmt, ...) \
    kapi_ulog_log(KAPI_ULOG_LEVEL_NOTICE, (uint8_t)(domain), (fmt), ##__VA_ARGS__)
#define KAPI_ULOG_INFO(domain, fmt, ...)    \
    kapi_ulog_log(KAPI_ULOG_LEVEL_INFO,   (uint8_t)(domain), (fmt), ##__VA_ARGS__)
#define KAPI_ULOG_DEBUG(domain, fmt, ...)  \
    kapi_ulog_log(KAPI_ULOG_LEVEL_DEBUG,  (uint8_t)(domain), (fmt), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* _KAPI_UNIFIED_LOG_H */
