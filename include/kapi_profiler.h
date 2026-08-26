#ifndef KAPI_PROFILER_H
#define KAPI_PROFILER_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 性能分析器会话管理 */
typedef struct kapi_profiler kapi_profiler_t;
typedef struct kapi_profile_session kapi_profile_session_t;

/* 性能分析类型 */
typedef enum {
    KAPI_PROFILE_CPU = 0,      /* CPU 性能分析 */
    KAPI_PROFILE_MEMORY,      /* 内存性能分析 */
    KAPI_PROFILE_IO,          /* I/O 性能分析 */
    KAPI_PROFILE_NETWORK,     /* 网络性能分析 */
    KAPI_PROFILE_SCHEDULING,  /* 调度性能分析 */
    KAPI_PROFILE_SYSTEMCALL   /* 系统调用分析 */
} kapi_profile_type_t;

/* 采样模式 */
typedef enum {
    KAPI_PROFILE_SAMPLE_TIME = 0,    /* 基于时间的采样 */
    KAPI_PROFILE_SAMPLE_EVENT,      /* 基于事件的采样 */
    KAPI_PROFILE_SAMPLE_MANUAL      /* 手动采样 */
} kapi_profile_sample_mode_t;

/* 分析模式 */
typedef enum {
    KAPI_PROFILE_MODE_SAMPLE = 0,    /* 采样模式 */
    KAPI_PROFILE_MODE_COUNT,        /* 计数模式 */
    KAPI_PROFILE_MODE_TRACE,        /* 追踪模式 */
    KAPI_PROFILE_MODE_AGGR          /* 聚合模式 */
} kapi_profile_mode_t;

/* 性能分析标志 */
#define KAPI_PROFILE_FL_ENABLED     (1U << 0)
#define KAPI_PROFILE_FL_SAMPLING    (1U << 1)
#define KAPI_PROFILE_FL_TRACING     (1U << 2)
#define KAPI_PROFILE_FL_AUTOSAVE    (1U << 3)
#define KAPI_PROFILE_FL_REALTIME    (1U << 4)

/* 采样数据结构 */
typedef struct kapi_profile_sample {
    uint64_t timestamp;          /* 时间戳 */
    uint64_t process_id;         /* 进程ID */
    uint64_t thread_id;          /* 线程ID */
    uint64_t instruction_ptr;   /* 指令指针 */
    uint64_t stack_ptr;         /* 栈指针 */
    uint64_t cpu_id;            /* CPU ID */
    uint64_t event_data;        /* 事件数据 */
    int sample_type;            /* 样本类型 */
    uint64_t custom_data[4];     /* 自定义数据 */
} kapi_profile_sample_t;

/* 函数调用统计 */
typedef struct kapi_profile_function {
    char name[256];                      /* 函数名 */
    char module[256];                   /* 所属模块 */
    uint64_t call_count;                /* 调用次数 */
    uint64_t total_time;                /* 总执行时间 (纳秒) */
    uint64_t min_time;                  /* 最小执行时间 */
    uint64_t max_time;                  /* 最大执行时间 */
    uint64_t avg_time;                  /* 平均执行时间 */
    uint64_t self_time;                 /* 自身执行时间 (排除调用者) */
    uint64_t parent_address;            /* 调用者地址 */
    uint64_t child_address;             /* 被调用者地址 */
    int depth;                          /* 调用深度 */
    uint64_t hot_spot_count;            /* 热点命中次数 */
    uint64_t memory_allocated;          /* 分配的内存 */
    uint64_t memory_freed;              /* 释放的内存 */
    int valid;                          /* 是否有效 */
} kapi_profile_function_t;

/* I/O 操作统计 */
typedef struct kapi_profile_io {
    uint64_t operation_count;            /* 操作次数 */
    uint64_t total_bytes;               /* 总字节数 */
    uint64_t min_bytes;                 /* 最小字节数 */
    uint64_t max_bytes;                 /* 最大字节数 */
    uint64_t avg_bytes;                 /* 平均字节数 */
    uint64_t total_time;                /* 总耗时 (纳秒) */
    uint64_t min_time;                  /* 最小耗时 */
    uint64_t max_time;                  /* 最大耗时 */
    uint64_t avg_time;                  /* 平均耗时 */
    uint64_t device_id;                 /* 设备ID */
    uint64_t process_id;                /* 进程ID */
    int io_type;                        /* I/O 类型 (读/写/其他) */
    int valid;                          /* 是否有效 */
} kapi_profile_io_t;

/* 内存使用统计 */
typedef struct kapi_profile_memory {
    uint64_t total_allocated;           /* 总分配内存 */
    uint64_t total_freed;               /* 总释放内存 */
    uint64_t current_usage;             /* 当前使用量 */
    uint64_t peak_usage;                /* 峰值使用量 */
    uint64_t allocation_count;         /* 分配次数 */
    uint64_t free_count;               /* 释放次数 */
    uint64_t fragmentation_ratio;       /* 碎片化比率 */
    uint64_t large_alloc_count;         /* 大内存分配次数 */
    uint64_t small_alloc_count;         /* 小内存分配次数 */
    uint64_t process_id;                /* 进程ID */
    int valid;                          /* 是否有效 */
} kapi_profile_memory_t;

/* 系统调用统计 */
typedef struct kapi_profile_syscall {
    int syscall_number;                 /* 系统调用号 */
    char name[128];                     /* 系统调用名 */
    uint64_t call_count;                /* 调用次数 */
    uint64_t total_time;                /* 总耗时 (纳秒) */
    uint64_t min_time;                  /* 最小耗时 */
    uint64_t max_time;                  /* 最大耗时 */
    uint64_t avg_time;                  /* 平均耗时 */
    uint64_t error_count;               /* 错误次数 */
    uint64_t process_id;                /* 进程ID */
    int valid;                          /* 是否有效 */
} kapi_profile_syscall_t;

/* 会话配置 */
typedef struct kapi_profile_config {
    kapi_profile_type_t type;           /* 分析类型 */
    kapi_profile_mode_t mode;           /* 分析模式 */
    kapi_profile_sample_mode_t sample_mode; /* 采样模式 */
    uint64_t sample_interval;          /* 采样间隔 (纳秒) */
    uint64_t buffer_size;              /* 缓冲区大小 */
    uint64_t max_samples;              /* 最大样本数 */
    uint64_t pid_filter;               /* 进程ID过滤器 */
    uint64_t tid_filter;               /* 线程ID过滤器 */
    char **function_filters;          /* 函数过滤器 */
    int filter_count;                  /* 过滤器数量 */
    uint64_t flags;                    /* 会话标志 */
    char output_file[512];             /* 输出文件路径 */
    int realtime_interval;            /* 实时输出间隔 (秒) */
} kapi_profile_config_t;

/* 性能分析会话 */
struct kapi_profile_session {
    char name[256];                     /* 会话名称 */
    kapi_profile_config_t config;      /* 配置信息 */
    time_t start_time;                 /* 开始时间 */
    time_t end_time;                   /* 结束时间 */
    uint64_t sample_count;             /* 样本数量 */
    uint64_t data_size;                /* 数据大小 */
    void *data_buffer;                 /* 数据缓冲区 */
    int session_id;                    /* 会话ID */
    int status;                        /* 会话状态 */
    kapi_profile_session_t *next;      /* 下一个会话 */
};

/* 性能分析器 */
struct kapi_profiler {
    kapi_profile_session_t *sessions;   /* 会话链表 */
    int session_count;                 /* 会话数量 */
    int current_session_id;             /* 当前会话ID */
    void *internal_data;               /* 内部数据 */
    uint64_t profiler_id;              /* 分析器ID */
    int last_error;                    /* 最后错误 */
};

/* 分析器初始化和销毁 */
int kapi_profiler_init(kapi_profiler_t **profiler);
int kapi_profiler_destroy(kapi_profiler_t *profiler);

/* 会话管理 */
int kapi_profile_start_session(kapi_profiler_t *profiler, 
                              const char *name,
                              const kapi_profile_config_t *config);
int kapi_profile_stop_session(kapi_profiler_t *profiler, const char *name);
int kapi_profile_get_session(kapi_profiler_t *profiler, const char *name,
                            kapi_profile_session_t **session);
int kapi_profile_list_sessions(kapi_profiler_t *profiler,
                              kapi_profile_session_t **sessions, int *count);
int kapi_profile_delete_session(kapi_profiler_t *profiler, const char *name);

/* 数据采集 */
int kapi_profile_start_sampling(kapi_profiler_t *profiler, const char *session_name);
int kapi_profile_stop_sampling(kapi_profiler_t *profiler, const char *session_name);
int kapi_profile_collect_sample(kapi_profiler_t *profiler, const char *session_name,
                              const kapi_profile_sample_t *sample);
int kapi_profile_manual_sample(kapi_profiler_t *profiler, const char *session_name);

/* 统计分析 */
int kapi_profile_analyze_functions(kapi_profiler_t *profiler, const char *session_name,
                                  kapi_profile_function_t **functions, int *count);
int kapi_profile_analyze_io(kapi_profiler_t *profiler, const char *session_name,
                          kapi_profile_io_t **io_stats, int *count);
int kapi_profile_analyze_memory(kapi_profiler_t *profiler, const char *session_name,
                               kapi_profile_memory_t **memory_stats, int *count);
int kapi_profile_analyze_syscalls(kapi_profiler_t *profiler, const char *session_name,
                                 kapi_profile_syscall_t **syscall_stats, int *count);

/* 热点分析 */
typedef struct kapi_profile_hotspot {
    char name[256];                     /* 名称 */
    uint64_t address;                  /* 地址 */
    uint64_t hit_count;                /* 命中次数 */
    uint64_t total_time;                /* 总时间 */
    uint64_t avg_time;                  /* 平均时间 */
    uint64_t percentage;                /* 占比 */
    int type;                          /* 类型 (函数/内存块等) */
    int valid;                          /* 是否有效 */
} kapi_profile_hotspot_t;

int kapi_profile_find_hotspots(kapi_profiler_t *profiler, const char *session_name,
                              kapi_profile_hotspot_t **hotspots, int *max_count,
                              kapi_profile_type_t type);

/* 实时监控 */
int kapi_profile_start_realtime(kapi_profiler_t *profiler, const char *session_name,
                               int interval_seconds);
int kapi_profile_stop_realtime(kapi_profiler_t *profiler, const char *session_name);
int kapi_profile_get_realtime_stats(kapi_profiler_t *profiler, const char *session_name,
                                   char **stats_json);

/* 报告生成 */
int kapi_profile_generate_report(kapi_profiler_t *profiler, const char *session_name,
                                const char *output_path, const char *format);
int kapi_profile_export_json(kapi_profiler_t *profiler, const char *session_name,
                           const char *output_path);
int kapi_profile_export_csv(kapi_profiler_t *profiler, const char *session_name,
                          const char *output_path);
int kapi_profile_export_svg(kapi_profiler_t *profiler, const char *session_name,
                          const char *output_path, const char *chart_type);

/* 数据过滤和排序 */
int kapi_profile_set_filters(kapi_profiler_t *profiler, const char *session_name,
                           uint64_t pid_filter, uint64_t tid_filter,
                           char **function_filters, int filter_count);
int kapi_profile_sort_functions(kapi_profiler_t *profiler, const char *session_name,
                              int sort_by, int ascending);
int kapi_profile_filter_by_time(kapi_profiler_t *profiler, const char *session_name,
                               uint64_t start_time, uint64_t end_time);

/* 性能阈值 */
typedef struct kapi_profile_threshold {
    uint64_t time_threshold_ns;         /* 时间阈值 (纳秒) */
    uint64_t memory_threshold_mb;       /* 内存阈值 (MB) */
    uint64_t io_threshold_bytes;       /* I/O 阈值 (字节) */
    uint64_t call_threshold;            /* 调用次数阈值 */
    char notification_email[256];       /* 通知邮箱 */
    int enabled;                        /* 是否启用 */
} kapi_profile_threshold_t;

int kapi_profile_set_thresholds(kapi_profiler_t *profiler, const char *session_name,
                               const kapi_profile_threshold_t *thresholds);
int kapi_profile_check_thresholds(kapi_profiler_t *profiler, const char *session_name,
                                 char **alert_messages, int *alert_count);

/* 工具函数 */
const char *kapi_profile_get_error_string(int error_code);
const char *kapi_profile_type_to_string(kapi_profile_type_t type);
kapi_profile_type_t kapi_profile_string_to_type(const char *str);
const char *kapi_profile_mode_to_string(kapi_profile_mode_t mode);
kapi_profile_mode_t kapi_profile_string_to_mode(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_PROFILER_H */