#ifndef KAPI_TRACE_H
#define KAPI_TRACE_H

#include <stdint.h>
#include <stddef.h>
#include "kapi_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Trace categories */
#define KAPI_TRACE_KERNEL     (1 << 0)
#define KAPI_TRACE_MEMORY     (1 << 1)
#define KAPI_TRACE_IO         (1 << 2)
#define KAPI_TRACE_NETWORK    (1 << 3)
#define KAPI_TRACE_PROCESS    (1 << 4)
#define KAPI_TRACE_SCHEDULING (1 << 5)
#define KAPI_TRACE_SYSCALL    (1 << 6)
#define KAPI_TRACE_INTERRUPT  (1 << 7)
#define KAPI_TRACE_DRIVER     (1 << 8)
#define KAPI_TRACE_FILESYSTEM (1 << 9)
#define KAPI_TRACE_VIRTUAL    (1 << 10)
#define KAPI_TRACE_SECURITY   (1 << 11)
#define KAPI_TRACE_ALL        0xFFFFFFFF

/* Trace buffer sizes */
#define KAPI_TRACE_BUFFER_SMALL   1024
#define KAPI_TRACE_BUFFER_MEDIUM  8192
#define KAPI_TRACE_BUFFER_LARGE   65536
#define KAPI_TRACE_BUFFER_HUGE    262144

/* Trace events */
typedef enum {
    KAPI_TRACE_EVENT_START,
    KAPI_TRACE_EVENT_END,
    KAPI_TRACE_EVENT_POINT,
    KAPI_TRACE_EVENT_ERROR,
    KAPI_TRACE_EVENT_WARNING,
    KAPI_TRACE_EVENT_INFO
} kapi_trace_event_type_t;

/* Trace point structure */
typedef struct {
    uint64_t          timestamp;
    uint32_t          cpu_id;
    uint32_t          event_type;
    uint32_t          category;
    uint32_t          pid;
    uint32_t          tid;
    char              name[64];
    char              func[128];
    int               line;
    uint64_t          duration;  /* For START/END events */
    uint64_t          data[8];   /* Custom data */
    size_t            data_size;
} kapi_trace_point_t;

/* Trace context structure */
typedef struct {
    uint64_t          start_time;
    uint64_t          end_time;
    uint64_t          duration;
    uint32_t          pid;
    uint32_t          tid;
    uint32_t          count;
    char              name[64];
} kapi_trace_context_t;

/* Trace filter structure */
typedef struct {
    uint32_t          enabled_categories;
    uint32_t          disabled_categories;
    char              process_name[64];
    uint32_t          min_duration_us;  /* Minimum duration in microseconds */
    int               include_children;
    int               show_details;
} kapi_trace_filter_t;

/* Trace session structure */
typedef struct kapi_trace_session kapi_trace_session_t;

/* Trace API */
int kapi_trace_init(uint32_t buffer_size, uint32_t enabled_categories);
void kapi_trace_cleanup(void);

int kapi_trace_start_session(const char* name, uint32_t categories);
int kapi_trace_stop_session(kapi_trace_session_t* session);

/* Trace point registration */
int kapi_trace_register_point(const char* name, const char* func, int line,
                             uint32_t category, uint32_t data_size);

int kapi_trace_unregister_point(const char* name);

/* Trace point recording */
void kapi_trace_start(const char* name, uint32_t category, uint64_t* data);
void kapi_trace_end(const char* name, uint32_t category, uint64_t* data);
void kapi_trace_point(const char* name, uint32_t category, uint64_t* data);

void kapi_trace_start_ex(const char* name, const char* func, int line,
                        uint32_t category, uint64_t* data);
void kapi_trace_end_ex(const char* name, const char* func, int line,
                      uint32_t category, uint64_t* data);
void kapi_trace_point_ex(const char* name, const char* func, int line,
                        uint32_t category, uint64_t* data);

/* Trace data analysis */
int kapi_trace_analyze(kapi_trace_session_t* session, 
                      kapi_trace_filter_t* filter,
                      kapi_trace_context_t** contexts,
                      uint32_t* context_count);

int kapi_trace_get_points(kapi_trace_session_t* session,
                         kapi_trace_point_t** points,
                         uint32_t* point_count);

int kapi_trace_filter_points(kapi_trace_point_t* points, uint32_t point_count,
                            kapi_trace_filter_t* filter,
                            kapi_trace_point_t** filtered_points,
                            uint32_t* filtered_count);

/* Trace statistics */
typedef struct {
    uint64_t          total_points;
    uint64_t          total_duration_ns;
    uint64_t          average_duration_ns;
    uint64_t          min_duration_ns;
    uint64_t          max_duration_ns;
    uint32_t          top_events[10];  /* Top event IDs by count */
    uint32_t          top_events_count;
    double            cpu_usage_percent;
} kapi_trace_stats_t;

int kapi_trace_get_stats(kapi_trace_session_t* session, kapi_trace_stats_t* stats);

/* Trace output functions */
int kapi_trace_print_session(kapi_trace_session_t* session);
int kapi_trace_print_points(kapi_trace_point_t* points, uint32_t count);
int kapi_trace_export_csv(const char* filename, kapi_trace_point_t* points, uint32_t count);

/* Trace visualization */
int kapi_trace_visualize_timeline(kapi_trace_session_t* session);
int kapi_trace_visualize_hierarchy(kapi_trace_session_t* session);

/* System call tracing */
typedef struct {
    uint64_t          syscall_id;
    uint64_t          start_time;
    uint64_t          end_time;
    uint64_t          duration;
    uint32_t          pid;
    uint32_t          tid;
    uint64_t          args[6];  /* System call arguments */
    int              result;    /* Return value */
    char              name[64];
} kapi_trace_syscall_t;

int kapi_trace_syscall_enable(void);
int kapi_trace_syscall_disable(void);
int kapi_trace_syscall_get(kapi_trace_syscall_t** syscalls, uint32_t* count);

/* Interrupt tracing */
typedef struct {
    uint32_t          irq;
    uint64_t          timestamp;
    uint64_t          duration;
    uint32_t          cpu_id;
    char              handler[128];
    char              device[64];
} kapi_trace_irq_t;

int kapi_trace_interrupt_enable(void);
int kapi_trace_interrupt_disable(void);
int kapi_trace_interrupt_get(kapi_trace_irq_t** irqs, uint32_t* count);

/* Function call tracing */
typedef struct {
    uint64_t          call_count;
    uint64_t          total_time_ns;
    uint64_t          min_time_ns;
    uint64_t          max_time_ns;
    uint64_t          avg_time_ns;
    char              function[256];
    uint32_t          pid;
    uint32_t          tid;
} kapi_trace_func_stats_t;

int kapi_trace_function_enable(const char* pattern);
int kapi_trace_function_disable(const char* pattern);
int kapi_trace_function_get_stats(kapi_trace_func_stats_t** stats, uint32_t* count);

/* Real-time tracing */
int kapi_trace_start_realtime(uint32_t categories, uint32_t buffer_size);
int kapi_trace_stop_realtime(void);
int kapi_trace_get_realtime_buffer(kapi_trace_point_t** points, uint32_t* count);

/* Trace event hooks */
typedef void (*kapi_trace_event_hook_t)(const kapi_trace_point_t* point);

int kapi_trace_register_event_hook(kapi_trace_event_hook_t hook);
int kapi_trace_unregister_event_hook(kapi_trace_event_hook_t hook);

/* Trace configuration */
int kapi_trace_set_buffer_size(uint32_t size);
int kapi_trace_set_categories(uint32_t categories);
int kapi_trace_get_categories(uint32_t* categories);

/* Performance monitoring integration */
int kapi_trace_enable_performance_monitoring(void);
int kapi_trace_disable_performance_monitoring(void);
int kapi_trace_get_performance_overhead(void);

/* Debug utilities */
#define kapi_trace_debug(category, fmt, ...) \
    kapi_trace_log_debug(category, __func__, __LINE__, fmt, ##__VA_ARGS__)

void kapi_trace_log_debug(uint32_t category, const char* func, int line,
                         const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif