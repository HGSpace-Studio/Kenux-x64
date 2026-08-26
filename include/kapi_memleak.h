#ifndef KAPI_MEMLEAK_H
#define KAPI_MEMLEAK_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 内存泄漏检测器会话管理 */
typedef struct kapi_memleak_detector kapi_memleak_detector_t;
typedef struct kapi_memleak_session kapi_memleak_session_t;

/* 内存分配类型 */
typedef enum {
    KAPI_MEMLEAK_MALLOC = 0,      /* malloc 分配 */
    KAPI_MEMLEAK_CALLOC,          /* calloc 分配 */
    KAPI_MEMLEAK_REALLOC,         /* realloc 分配 */
    KAPI_MEMLEAK_STRDUP,          /* strdup 分配 */
    KAPI_MEMLEAK_NEW,             /* new 操作符 */
    KAPI_MEMLEAK_NEW_ARRAY,       /* new[] 操作符 */
    KAPI_MEMLEAK_ALIGNED_ALLOC,   /* aligned_alloc 分配 */
    KAPI_MEMLEAK_POSIX_MEMALIGN,  /* posix_memalign 分配 */
    KAPI_MEMLEAK_VM_ALLOC,        /* 虚拟内存分配 */
    KAPI_MEMLEAK_UNKNOWN          /* 未知类型 */
} kapi_memleak_alloc_type_t;

/* 内存块状态 */
typedef enum {
    KAPI_MEMLEAK_STATE_ALLOCATED = 0,  /* 已分配 */
    KAPI_MEMLEAK_STATE_FREED,          /* 已释放 */
    KAPI_MEMLEAK_STATE_CORRUPTED,      /* 已损坏 */
    KAPI_MEMLEAK_STATE_INVALID         /* 无效状态 */
} kapi_memleak_state_t;

/* 检测严重程度 */
typedef enum {
    KAPI_MEMLEAK_SEVERITY_LOW = 0,      /* 低严重性 */
    KAPI_MEMLEAK_SEVERITY_MEDIUM,       /* 中等严重性 */
    KAPI_MEMLEAK_SEVERITY_HIGH,         /* 高严重性 */
    KAPI_MEMLEAK_SEVERITY_CRITICAL      /* 严重性 */
} kapi_memleak_severity_t;

/* 内存块信息 */
typedef struct kapi_memleak_block {
    uint64_t block_id;               /* 块ID */
    void *address;                   /* 内存地址 */
    size_t size;                     /* 内存大小 */
    kapi_memleak_alloc_type_t type;  /* 分配类型 */
    kapi_memleak_state_t state;      /* 内存状态 */
    
    /* 分配信息 */
    pid_t pid;                       /* 进程ID */
    time_t alloc_time;               /* 分配时间 */
    char alloc_file[256];            /* 分配文件名 */
    int alloc_line;                  /* 分配行号 */
    
    /* 释放信息 */
    time_t free_time;               /* 释放时间 */
    char free_file[256];            /* 释放文件名 */
    int free_line;                  /* 释放行号 */
    
    /* 使用统计 */
    uint64_t access_count;          /* 访问次数 */
    uint64_t read_count;            /* 读取次数 */
    uint64_t write_count;           /* 写入次数 */
    
    /* 内存模式 */
    uint64_t access_pattern;        /* 访问模式 */
    int has_corruption;             /* 是否有损坏 */
    size_t corruption_offset;       /* 损坏偏移量 */
    uint8_t expected_byte;          /* 期望字节值 */
    uint8_t actual_byte;            /* 实际字节值 */
    
    /* 引用信息 */
    int ref_count;                  /* 引用计数 */
    void *parent_block;             /* 父块地址 */
    void **child_blocks;            /* 子块地址列表 */
    int child_count;                /* 子块数量 */
    
    /* 内存标签 */
    char tag[64];                   /* 用户标签 */
    int has_tag;                    /* 是否有标签 */
    
    /* 有效标志 */
    int valid;                      /* 是否有效 */
} kapi_memleak_block_t;

/* 泄漏报告 */
typedef struct kapi_memleak_report {
    uint64_t report_id;             /* 报告ID */
    time_t timestamp;               /* 时间戳 */
    pid_t pid;                      /* 进程ID */
    
    /* 统计信息 */
    uint64_t total_allocated;      /* 总分配量 */
    uint64_t total_freed;           /* 总释放量 */
    uint64_t current_allocated;     /* 当前分配量 */
    uint64_t peak_allocated;       /* 峰值分配量 */
    uint64_t leak_count;            /* 泄漏块数量 */
    uint64_t leak_size;             /* 泄漏总量 */
    uint64_t corruption_count;      /* 损坏块数量 */
    
    /* 严重性统计 */
    int low_severity_count;         /* 低严重性数量 */
    int medium_severity_count;      /* 中等严重性数量 */
    int high_severity_count;        /* 高严重性数量 */
    int critical_severity_count;    /* 严重性数量 */
    
    /* 详细信息 */
    kapi_memleak_block_t **leaked_blocks;  /* 泄漏块列表 */
    int leaked_block_count;         /* 泄漏块数量 */
    kapi_memleak_block_t **corrupted_blocks; /* 损坏块列表 */
    int corrupted_block_count;      /* 损坏块数量 */
    
    /* 报告配置 */
    char output_file[512];          /* 输出文件 */
    int generate_graph;             /* 是否生成图形报告 */
    int include_stacktrace;         /* 是否包含栈跟踪 */
    int verbose;                    /* 详细模式 */
    
    int valid;                      /* 是否有效 */
} kapi_memleak_report_t;

/* 检测器配置 */
typedef struct kapi_memleak_config {
    /* 基本配置 */
    size_t max_memory_mb;           /* 最大内存限制 (MB) */
    size_t leak_threshold_size;     /* 泄漏阈值大小 */
    int enable_stacktrace;          /* 启用栈跟踪 */
    int enable_access_tracking;     /* 启用访问跟踪 */
    int enable_corruption_detection; /* 启用损坏检测 */
    int enable_pattern_analysis;     /* 启用模式分析 */
    
    /* 采样配置 */
    uint64_t sample_interval_ms;    /* 采样间隔 (毫秒) */
    int sampling_percentage;        /* 采样百分比 */
    int enable_random_sampling;     /* 启用随机采样 */
    
    /* 报告配置 */
    char default_output_path[512];   /* 默认输出路径 */
    int auto_generate_reports;      /* 自动生成报告 */
    int report_interval_minutes;     /* 报告间隔 (分钟) */
    
    /* 过滤器配置 */
    pid_t target_pid;               /* 目标进程ID (0表示所有进程) */
    char **file_filters;            /* 文件过滤器 */
    int file_filter_count;          /* 文件过滤器数量 */
    char **function_filters;        /* 函数过滤器 */
    int function_filter_count;      /* 函数过滤器数量 */
    
    /* 性能配置 */
    int max_blocks_tracked;         /* 最大跟踪块数 */
    int enable_pool_tracking;       /* 启用内存池跟踪 */
    int enable_fragmentation_analysis; /* 启用碎片化分析 */
    
    /* 严重性阈值 */
    size_t low_severity_threshold;  /* 低严重性阈值 */
    size_t medium_severity_threshold; /* 中等严重性阈值 */
    size_t high_severity_threshold;  /* 高严重性阈值 */
    
    /* 标志 */
    uint64_t flags;                 /* 配置标志 */
    
    /* 回调函数 */
    void (*leak_detected_callback)(const kapi_memleak_block_t *block, void *user_data);
    void (*corruption_detected_callback)(const kapi_memleak_block_t *block, void *user_data);
    void (*threshold_exceeded_callback)(size_t current_usage, size_t threshold, void *user_data);
    void *user_data;                /* 用户数据 */
} kapi_memleak_config_t;

/* 检测器会话 */
struct kapi_memleak_session {
    char name[256];                 /* 会话名称 */
    kapi_memleak_config_t config;   /* 配置信息 */
    time_t start_time;              /* 开始时间 */
    time_t end_time;                /* 结束时间 */
    int session_id;                 /* 会话ID */
    int status;                     /* 会话状态 */
    
    /* 统计信息 */
    uint64_t total_allocations;      /* 总分配次数 */
    uint64_t total_frees;           /* 总释放次数 */
    uint64_t total_bytes_allocated; /* 总分配字节数 */
    uint64_t total_bytes_freed;     /* 总释放字节数 */
    uint64_t current_bytes_used;    /* 当前使用字节数 */
    uint64_t peak_bytes_used;      /* 峰值使用字节数 */
    
    /* 块跟踪 */
    kapi_memleak_block_t **blocks;  /* 内存块列表 */
    int block_count;                /* 内存块数量 */
    int block_capacity;             /* 内存块容量 */
    
    /* 检测器指针 */
    kapi_memleak_detector_t *detector; /* 所属检测器 */
    kapi_memleak_session_t *next;   /* 下一个会话 */
    
    /* 临时数据 */
    void *temp_data;                /* 临时数据 */
    size_t temp_data_size;         /* 临时数据大小 */
};

/* 检测器 */
struct kapi_memleak_detector {
    kapi_memleak_session_t *sessions;  /* 会话链表 */
    int session_count;                 /* 会话数量 */
    
    /* 全局配置 */
    kapi_memleak_config_t global_config; /* 全局配置 */
    
    /* 内部数据 */
    void *internal_data;               /* 内部数据 */
    uint64_t detector_id;              /* 检测器ID */
    int last_error;                    /* 最后错误 */
    int initialized;                   /* 是否已初始化 */
};

/* 内存分配替代函数 */
void *kapi_memleak_malloc(size_t size, const char *file, int line);
void *kapi_memleak_calloc(size_t nmemb, size_t size, const char *file, int line);
void *kapi_memleak_realloc(void *ptr, size_t size, const char *file, int line);
void *kapi_memleak_strdup(const char *s, const char *file, int line);
void *kapi_memleak_aligned_alloc(size_t alignment, size_t size, const char *file, int line);
int kapi_memleak_posix_memalign(void **memptr, size_t alignment, size_t size, const char *file, int line);
void *kapi_memleak_vm_alloc(size_t size, const char *file, int line);

/* 内存释放替代函数 */
void kapi_memleak_free(void *ptr, const char *file, int line);
void kapi_memleak_safe_free(void *ptr, const char *file, int line);

/* 内存操作包装函数 */
void *kapi_memleak_memcpy(void *dest, const void *src, size_t n, const char *file, int line);
void *kapi_memleak_memset(void *s, int c, size_t n, const char *file, int line);
void *kapi_memleak_memmove(void *dest, const void *src, size_t n, const char *file, int line);

/* 检测器初始化和销毁 */
int kapi_memleak_init_detector(kapi_memleak_detector_t **detector);
int kapi_memleak_destroy_detector(kapi_memleak_detector_t *detector);

/* 会话管理 */
int kapi_memleak_start_session(kapi_memleak_detector_t *detector,
                             const char *name,
                             const kapi_memleak_config_t *config);
int kapi_memleak_stop_session(kapi_memleak_detector_t *detector, const char *name);
int kapi_memleak_get_session(kapi_memleak_detector_t *detector, const char *name,
                            kapi_memleak_session_t **session);
int kapi_memleak_list_sessions(kapi_memleak_detector_t *detector,
                             kapi_memleak_session_t **sessions, int *count);
int kapi_memleak_delete_session(kapi_memleak_detector_t *detector, const char *name);

/* 检测控制 */
int kapi_memleak_start_detection(kapi_memleak_detector_t *detector, const char *session_name);
int kapi_memleak_stop_detection(kapi_memleak_detector_t *detector, const char *session_name);
int kapi_memleak_force_check(kapi_memleak_detector_t *detector, const char *session_name);

/* 报告生成 */
int kapi_memleak_check_for_leaks(kapi_memleak_detector_t *detector, const char *session_name,
                                kapi_memleak_report_t **report);
int kapi_memleak_generate_report(kapi_memleak_detector_t *detector, const char *session_name,
                               const char *output_path);
int kapi_memleak_export_json(kapi_memleak_detector_t *detector, const char *session_name,
                            const char *output_path);
int kapi_memleak_export_xml(kapi_memleak_detector_t *detector, const char *session_name,
                           const char *output_path);
int kapi_memleak_export_graph(kapi_memleak_detector_t *detector, const char *session_name,
                            const char *output_path);

/* 内存块操作 */
int kapi_memleak_tag_block(void *ptr, const char *tag);
const char *kapi_memleak_get_tag(void *ptr);
int kapi_memleak_add_reference(void *ptr, void *parent_ptr);
int kapi_memleak_remove_reference(void *ptr);
int kapi_memleak_verify_block(void *ptr, int *is_valid, size_t *corruption_offset);

/* 统计和查询 */
int kapi_memleak_get_stats(kapi_memleak_detector_t *detector, const char *session_name,
                          uint64_t *total_alloc, uint64_t *total_free,
                          uint64_t *current_used, uint64_t *peak_used);
int kapi_memleak_get_leak_summary(kapi_memleak_detector_t *detector, const char *session_name,
                                int *leak_count, size_t *leak_size,
                                int *corruption_count);
int kapi_memleak_find_blocks_by_tag(kapi_memleak_detector_t *detector, const char *session_name,
                                   const char *tag, kapi_memleak_block_t **blocks, int *count);
int kapi_memleak_find_blocks_by_size(kapi_memleak_detector_t *detector, const char *session_name,
                                    size_t min_size, size_t max_size,
                                    kapi_memleak_block_t **blocks, int *count);

/* 内存分析工具 */
int kapi_memleak_analyze_fragmentation(kapi_memleak_detector_t *detector, const char *session_name,
                                      double *fragmentation_ratio, size_t *avg_block_size);
int kapi_memleak_detect_double_free(kapi_memleak_detector_t *detector, const char *session_name,
                                   kapi_memleak_block_t **doubly_freed_blocks, int *count);
int kapi_memleak_detect_use_after_free(kapi_memleak_detector_t *detector, const char *session_name,
                                      kapi_memleak_block_t **use_after_free_blocks, int *count);
int kapi_memleak_detect_memory_corruption(kapi_memleak_detector_t *detector, const char *session_name,
                                         kapi_memleak_block_t **corrupted_blocks, int *count);

/* 内存压力测试 */
int kapi_memleak_stress_test(kapi_memleak_detector_t *detector, const char *session_name,
                            size_t max_memory_mb, int test_duration_seconds);
int kapi_memleak_leak_simulation(kapi_memleak_detector_t *detector, const char *session_name,
                               int leak_count, size_t leak_size);
int kapi_memleak_corruption_simulation(kapi_memleak_detector_t *detector, const char *session_name,
                                     int corruption_count);

/* 实时监控 */
int kapi_memleak_start_realtime_monitoring(kapi_memleak_detector_t *detector, const char *session_name,
                                         int interval_seconds);
int kapi_memleak_stop_realtime_monitoring(kapi_memleak_detector_t *detector, const char *session_name);
int kapi_memleak_get_realtime_stats(kapi_memleak_detector_t *detector, const char *session_name,
                                    char **stats_json);

/* 配置管理 */
int kapi_memleak_set_global_config(kapi_memleak_detector_t *detector,
                                  const kapi_memleak_config_t *config);
int kapi_memleak_get_global_config(kapi_memleak_detector_t *detector,
                                  kapi_memleak_config_t *config);
int kapi_memleak_update_session_config(kapi_memleak_detector_t *detector, const char *session_name,
                                     const kapi_memleak_config_t *new_config);

/* 工具函数 */
const char *kapi_memleak_get_error_string(int error_code);
const char *kapi_memleak_alloc_type_to_string(kapi_memleak_alloc_type_t type);
kapi_memleak_alloc_type_t kapi_memleak_string_to_alloc_type(const char *str);
const char *kapi_memleak_state_to_string(kapi_memleak_state_t state);
const char *kapi_memleak_severity_to_string(kapi_memleak_severity_t severity);

/* 内存池支持 */
int kapi_memleak_init_memory_pool(kapi_memleak_detector_t *detector, const char *session_name,
                                 const char *pool_name, size_t pool_size);
int kapi_memleak_destroy_memory_pool(kapi_memleak_detector_t *detector, const char *session_name,
                                    const char *pool_name);
void *kapi_memleak_pool_alloc(kapi_memleak_detector_t *detector, const char *session_name,
                             const char *pool_name, size_t size, const char *file, int line);
void kapi_memleak_pool_free(kapi_memleak_detector_t *detector, const char *session_name,
                           const char *pool_name, void *ptr, const char *file, int line);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_MEMLEAK_H */