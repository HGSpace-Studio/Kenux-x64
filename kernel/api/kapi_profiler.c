#include "kapi_profiler.h"
#include "kapi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

/* 分析器内部结构 */
struct kapi_profiler {
    kapi_profile_session_t *sessions;   /* 会话链表 */
    int session_count;                 /* 会话数量 */
    int current_session_id;             /* 当前会话ID */
    void *internal_data;               /* 内部数据 */
    uint64_t profiler_id;              /* 分析器ID */
    int last_error;                    /* 最后错误 */
};

/* 性能分析器ID生成器 */
static uint64_t s_profiler_id_counter = 1;

/* 错误码定义 */
static const char *s_error_strings[] = {
    "Success",
    "Invalid argument",
    "Out of memory",
    "Session not found",
    "Session already exists",
    "Session not active",
    "Invalid configuration",
    "Profile data corrupted",
    "Export failed",
    "Sampling in progress",
    "Real-time monitoring active",
    "Threshold exceeded",
    "Permission denied",
    "File I/O error",
    "Buffer overflow",
    "Invalid format"
};

#define KAPI_PROFILER_MAX_SESSIONS    256
#define KAPI_PROFILER_MAX_SAMPLES     65536
#define KAPI_PROFILER_MAX_FUNCTIONS   4096
#define KAPI_PROFILER_MAX_IO_OPS      2048
#define KAPI_PROFILER_MAX_SYSCALLS    512

/* 内部辅助函数 */
static kapi_profile_session_t *find_session(kapi_profiler_t *profiler, const char *name)
{
    kapi_profile_session_t *current = profiler->sessions;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static int create_unique_session_id(kapi_profiler_t *profiler)
{
    return ++profiler->current_session_id;
}

static int resize_session_buffer(kapi_profile_session_t *session, size_t new_size)
{
    void *new_buffer = realloc(session->data_buffer, new_size);
    if (!new_buffer) {
        return -KAPI_ENOMEM;
    }
    
    session->data_buffer = new_buffer;
    session->data_size = new_size;
    return 0;
}

static uint64_t get_nanoseconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* 错误处理 */
static void profiler_set_error(kapi_profiler_t *profiler, int error_code)
{
    profiler->last_error = error_code;
}

const char *kapi_profiler_get_error_string(int error_code)
{
    if (error_code >= 0 && error_code < sizeof(s_error_strings) / sizeof(s_error_strings[0]))
        return s_error_strings[error_code];
    return "Unknown error";
}

/* 分析器初始化和销毁 */
int kapi_profiler_init(kapi_profiler_t **profiler)
{
    if (!profiler)
        return -KAPI_EINVAL;
    
    kapi_profiler_t *prof = calloc(1, sizeof(kapi_profiler_t));
    if (!prof)
        return -KAPI_ENOMEM;
    
    /* 初始化字段 */
    prof->sessions = NULL;
    prof->session_count = 0;
    prof->current_session_id = 0;
    prof->internal_data = NULL;
    prof->profiler_id = s_profiler_id_counter++;
    prof->last_error = 0;
    
    *profiler = prof;
    return 0;
}

int kapi_profiler_destroy(kapi_profiler_t *profiler)
{
    if (!profiler)
        return -KAPI_EINVAL;
    
    /* 销毁所有会话 */
    kapi_profile_session_t *current = profiler->sessions;
    while (current) {
        kapi_profile_session_t *next = current->next;
        
        /* 释放数据缓冲区 */
        if (current->data_buffer) {
            free(current->data_buffer);
        }
        
        /* 释放过滤器数组 */
        if (current->config.function_filters) {
            for (int i = 0; i < current->config.filter_count; i++) {
                free(current->config.function_filters[i]);
            }
            free(current->config.function_filters);
        }
        
        free(current);
        current = next;
    }
    
    free(profiler);
    return 0;
}

/* 会话管理 */
int kapi_profile_start_session(kapi_profiler_t *profiler, 
                              const char *name,
                              const kapi_profile_config_t *config)
{
    if (!profiler || !name || !config)
        return -KAPI_EINVAL;
    
    /* 检查会话是否已存在 */
    if (find_session(profiler, name)) {
        profiler_set_error(profiler, -KAPI_EBUSY);
        return -KAPI_EBUSY;
    }
    
    /* 检查会话数量限制 */
    if (profiler->session_count >= KAPI_PROFILER_MAX_SESSIONS) {
        profiler_set_error(profiler, -KAPI_EBUSY);
        return -KAPI_EBUSY;
    }
    
    /* 创建新会话 */
    kapi_profile_session_t *session = calloc(1, sizeof(kapi_profile_session_t));
    if (!session) {
        profiler_set_error(profiler, -KAPI_ENOMEM);
        return -KAPI_ENOMEM;
    }
    
    /* 复制配置信息 */
    memset(session, 0, sizeof(kapi_profile_session_t));
    strncpy(session->name, name, sizeof(session->name) - 1);
    session->config = *config;
    session->session_id = create_unique_session_id(profiler);
    session->start_time = time(NULL);
    session->status = 1; /* 活跃状态 */
    
    /* 初始化数据缓冲区 */
    size_t buffer_size = config->buffer_size ? config->buffer_size : 
                        (KAPI_PROFILER_MAX_SAMPLES * sizeof(kapi_profile_sample_t));
    if (resize_session_buffer(session, buffer_size) < 0) {
        free(session);
        profiler_set_error(profiler, -KAPI_ENOMEM);
        return -KAPI_ENOMEM;
    }
    
    /* 添加到会话链表 */
    session->next = profiler->sessions;
    profiler->sessions = session;
    profiler->session_count++;
    
    printf("Started profiling session: %s (ID: %d)\n", name, session->session_id);
    return 0;
}

int kapi_profile_stop_session(kapi_profiler_t *profiler, const char *name)
{
    if (!profiler || !name)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *session = find_session(profiler, name);
    if (!session) {
        profiler_set_error(profiler, -KAPI_ENOENT);
        return -KAPI_ENOENT;
    }
    
    session->end_time = time(NULL);
    session->status = 0; /* 非活跃状态 */
    
    printf("Stopped profiling session: %s\n", name);
    return 0;
}

int kapi_profile_get_session(kapi_profiler_t *profiler, const char *name,
                            kapi_profile_session_t **session)
{
    if (!profiler || !name || !session)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *sess = find_session(profiler, name);
    if (!sess) {
        profiler_set_error(profiler, -KAPI_ENOENT);
        return -KAPI_ENOENT;
    }
    
    *session = sess;
    return 0;
}

int kapi_profile_list_sessions(kapi_profiler_t *profiler,
                              kapi_profile_session_t **sessions, int *count)
{
    if (!profiler || !sessions || !count)
        return -KAPI_EINVAL;
    
    *count = profiler->session_count;
    if (*count == 0) {
        *sessions = NULL;
        return 0;
    }
    
    /* 分配内存 */
    *sessions = malloc(*count * sizeof(kapi_profile_session_t));
    if (!*sessions) {
        profiler_set_error(profiler, -KAPI_ENOMEM);
        return -KAPI_ENOMEM;
    }
    
    /* 复制会话信息 */
    kapi_profile_session_t *current = profiler->sessions;
    int index = 0;
    while (current && index < *count) {
        (*sessions)[index++] = *current;
        current = current->next;
    }
    
    return 0;
}

int kapi_profile_delete_session(kapi_profiler_t *profiler, const char *name)
{
    if (!profiler || !name)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *prev = NULL;
    kapi_profile_session_t *current = profiler->sessions;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            /* 从链表中移除 */
            if (prev) {
                prev->next = current->next;
            } else {
                profiler->sessions = current->next;
            }
            
            /* 释放资源 */
            if (current->data_buffer) {
                free(current->data_buffer);
            }
            
            if (current->config.function_filters) {
                for (int i = 0; i < current->config.filter_count; i++) {
                    free(current->config.function_filters[i]);
                }
                free(current->config.function_filters);
            }
            
            free(current);
            profiler->session_count--;
            
            printf("Deleted profiling session: %s\n", name);
            return 0;
        }
        
        prev = current;
        current = current->next;
    }
    
    profiler_set_error(profiler, -KAPI_ENOENT);
    return -KAPI_ENOENT;
}

/* 数据采集 */
int kapi_profile_start_sampling(kapi_profiler_t *profiler, const char *session_name)
{
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        return result;
    }
    
    if (!(session->config.flags & KAPI_PROFILE_FL_ENABLED)) {
        profiler_set_error(profiler, -KAPI_EINVAL);
        return -KAPI_EINVAL;
    }
    
    session->config.flags |= KAPI_PROFILE_FL_SAMPLING;
    printf("Started sampling for session: %s\n", session_name);
    return 0;
}

int kapi_profile_stop_sampling(kapi_profiler_t *profiler, const char *session_name)
{
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        return result;
    }
    
    session->config.flags &= ~KAPI_PROFILE_FL_SAMPLING;
    printf("Stopped sampling for session: %s\n", session_name);
    return 0;
}

int kapi_profile_collect_sample(kapi_profiler_t *profiler, const char *session_name,
                              const kapi_profile_sample_t *sample)
{
    if (!sample)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        return result;
    }
    
    if (!(session->config.flags & KAPI_PROFILE_FL_SAMPLING)) {
        profiler_set_error(profiler, -KAPI_EINVAL);
        return -KAPI_EINVAL;
    }
    
    /* 检查样本数量限制 */
    if (session->sample_count >= session->config.max_samples) {
        profiler_set_error(profiler, -KAPI_EBUSY);
        return -KAPI_EBUSY;
    }
    
    /* 存储样本 */
    if (session->data_buffer && session->sample_count < session->config.max_samples) {
        kapi_profile_sample_t *samples = (kapi_profile_sample_t *)session->data_buffer;
        samples[session->sample_count] = *sample;
        session->sample_count++;
    }
    
    return 0;
}

int kapi_profile_manual_sample(kapi_profiler_t *profiler, const char *session_name)
{
    kapi_profile_sample_t sample;
    memset(&sample, 0, sizeof(sample));
    
    sample.timestamp = get_nanoseconds();
    sample.sample_type = session->config.type;
    
    return kapi_profile_collect_sample(profiler, session_name, &sample);
}

/* 统计分析 */
int kapi_profile_analyze_functions(kapi_profiler_t *profiler, const char *session_name,
                                  kapi_profile_function_t **functions, int *count)
{
    if (!functions || !count)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        return result;
    }
    
    /* 模拟函数分析结果 */
    int func_count = 10;
    *functions = malloc(func_count * sizeof(kapi_profile_function_t));
    if (!*functions) {
        profiler_set_error(profiler, -KAPI_ENOMEM);
        return -KAPI_ENOMEM;
    }
    
    /* 模拟生成函数统计 */
    for (int i = 0; i < func_count; i++) {
        kapi_profile_function_t *func = &(*functions)[i];
        memset(func, 0, sizeof(*func));
        
        snprintf(func->name, sizeof(func->name), "function_%d", i);
        snprintf(func->module, sizeof(func->module), "module_%d", i % 3);
        
        func->call_count = 100 + i * 10;
        func->total_time = 1000000ULL + i * 100000ULL;
        func->min_time = 50000ULL + i * 5000ULL;
        func->max_time = 200000ULL + i * 20000ULL;
        func->avg_time = func->total_time / func->call_count;
        func->depth = i % 5;
        func->memory_allocated = 1024ULL * (i + 1) * 64;
        func->valid = 1;
    }
    
    *count = func_count;
    printf("Analyzed %d functions for session: %s\n", func_count, session_name);
    return 0;
}

int kapi_profile_analyze_io(kapi_profiler_t *profiler, const char *session_name,
                          kapi_profile_io_t **io_stats, int *count)
{
    if (!io_stats || !count)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        return result;
    }
    
    /* 模拟 I/O 分析结果 */
    int io_count = 5;
    *io_stats = malloc(io_count * sizeof(kapi_profile_io_t));
    if (!*io_stats) {
        profiler_set_error(profiler, -KAPI_ENOMEM);
        return -KAPI_ENOMEM;
    }
    
    /* 模拟生成 I/O 统计 */
    for (int i = 0; i < io_count; i++) {
        kapi_profile_io_t *io = &(*io_stats)[i];
        memset(io, 0, sizeof(*io));
        
        io->operation_count = 50 + i * 20;
        io->total_bytes = 1024ULL * 1024ULL * (i + 1) * 10;
        io->min_bytes = 1024ULL * (i + 1);
        io->max_bytes = 1024ULL * 1024ULL * (i + 1);
        io->avg_bytes = io->total_bytes / io->operation_count;
        io->total_time = 1000000ULL + i * 500000ULL;
        io->min_time = 10000ULL + i * 1000ULL;
        io->max_time = 100000ULL + i * 10000ULL;
        io->avg_time = io->total_time / io->operation_count;
        io->device_id = 1000 + i;
        io->process_id = 100 + i;
        io->io_type = i % 3; /* 读/写/其他 */
        io->valid = 1;
    }
    
    *count = io_count;
    printf("Analyzed %d I/O operations for session: %s\n", io_count, session_name);
    return 0;
}

int kapi_profile_analyze_memory(kapi_profiler_t *profiler, const char *session_name,
                               kapi_profile_memory_t **memory_stats, int *count)
{
    if (!memory_stats || !count)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        return result;
    }
    
    /* 模拟内存分析结果 */
    *memory_stats = malloc(sizeof(kapi_profile_memory_t));
    if (!*memory_stats) {
        profiler_set_error(profiler, -KAPI_ENOMEM);
        return -KAPI_ENOMEM;
    }
    
    kapi_profile_memory_t *mem = *memory_stats;
    memset(mem, 0, sizeof(*mem));
    
    mem->total_allocated = 1024ULL * 1024ULL * 1024ULL; /* 1GB */
    mem->total_freed = 768ULL * 1024ULL * 1024ULL; /* 768MB */
    mem->current_usage = mem->total_allocated - mem->total_freed;
    mem->peak_usage = 1200ULL * 1024ULL * 1024ULL; /* 1.2GB */
    mem->allocation_count = 5000;
    mem->free_count = 4800;
    mem->fragmentation_ratio = 15; /* 15% */
    mem->large_alloc_count = 50;
    mem->small_alloc_count = 4950;
    mem->process_id = 100;
    mem->valid = 1;
    
    *count = 1;
    printf("Analyzed memory usage for session: %s\n", session_name);
    return 0;
}

int kapi_profile_analyze_syscalls(kapi_profiler_t *profiler, const char *session_name,
                                 kapi_profile_syscall_t **syscall_stats, int *count)
{
    if (!syscall_stats || !count)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        return result;
    }
    
    /* 模拟系统调用分析结果 */
    int syscall_count = 8;
    *syscall_stats = malloc(syscall_count * sizeof(kapi_profile_syscall_t));
    if (!*syscall_stats) {
        profiler_set_error(profiler, -KAPI_ENOMEM);
        return -KAPI_ENOMEM;
    }
    
    /* 常见系统调用 */
    const char *syscall_names[] = {
        "open", "read", "write", "close", "mmap", "munmap", "fork", "exec"
    };
    
    for (int i = 0; i < syscall_count; i++) {
        kapi_profile_syscall_t *sc = &(*syscall_stats)[i];
        memset(sc, 0, sizeof(*sc));
        
        sc->syscall_number = i + 1;
        strncpy(sc->name, syscall_names[i], sizeof(sc->name) - 1);
        sc->call_count = 100 + i * 25;
        sc->total_time = 500000ULL + i * 100000ULL;
        sc->min_time = 1000ULL + i * 100ULL;
        sc->max_time = 10000ULL + i * 1000ULL;
        sc->avg_time = sc->total_time / sc->call_count;
        sc->error_count = i * 2; /* 模拟错误次数 */
        sc->process_id = 100 + i;
        sc->valid = 1;
    }
    
    *count = syscall_count;
    printf("Analyzed %d system calls for session: %s\n", syscall_count, session_name);
    return 0;
}

/* 热点分析 */
int kapi_profile_find_hotspots(kapi_profiler_t *profiler, const char *session_name,
                              kapi_profile_hotspot_t **hotspots, int *max_count,
                              kapi_profile_type_t type)
{
    if (!hotspots || !max_count || *max_count <= 0)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        return result;
    }
    
    /* 模拟热点分析 */
    int hotspot_count = *max_count;
    *hotspots = malloc(hotspot_count * sizeof(kapi_profile_hotspot_t));
    if (!*hotspots) {
        profiler_set_error(profiler, -KAPI_ENOMEM);
        return -KAPI_ENOMEM;
    }
    
    uint64_t total_samples = session->sample_count;
    
    for (int i = 0; i < hotspot_count; i++) {
        kapi_profile_hotspot_t *hotspot = &(*hotspots)[i];
        memset(hotspot, 0, sizeof(*hotspot));
        
        snprintf(hotspot->name, sizeof(hotspot->name), "hotspot_%d", i);
        hotspot->address = 0x1000ULL + i * 0x100ULL;
        hotspot->hit_count = total_samples / (i + 1);
        hotspot->total_time = hotspot->hit_count * 1000ULL;
        hotspot->avg_time = hotspot->total_time / hotspot->hit_count;
        hotspot->percentage = (hotspot->hit_count * 100) / total_samples;
        hotspot->type = type;
        hotspot->valid = 1;
    }
    
    printf("Found %d hotspots for session: %s\n", hotspot_count, session_name);
    return 0;
}

/* 报告生成 */
int kapi_profile_generate_report(kapi_profiler_t *profiler, const char *session_name,
                                const char *output_path, const char *format)
{
    if (!profiler || !session_name || !output_path || !format)
        return -KAPI_EINVAL;
    
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        return result;
    }
    
    /* 根据格式生成报告 */
    if (strcmp(format, "json") == 0) {
        return kapi_profile_export_json(profiler, session_name, output_path);
    } else if (strcmp(format, "csv") == 0) {
        return kapi_profile_export_csv(profiler, session_name, output_path);
    } else if (strcmp(format, "svg") == 0) {
        return kapi_profile_export_svg(profiler, session_name, output_path, "flame");
    } else {
        profiler_set_error(profiler, -KAPI_EINVAL);
        return -KAPI_EINVAL;
    }
}

int kapi_profile_export_json(kapi_profiler_t *profiler, const char *session_name,
                           const char *output_path)
{
    FILE *file = fopen(output_path, "w");
    if (!file) {
        profiler_set_error(profiler, -KAPI_EIO);
        return -KAPI_EIO;
    }
    
    kapi_profile_session_t *session;
    int result = kapi_profile_get_session(profiler, session_name, &session);
    if (result != 0) {
        fclose(file);
        return result;
    }
    
    /* 生成 JSON 报告 */
    fprintf(file, "{\n");
    fprintf(file, "  \"session_name\": \"%s\",\n", session->name);
    fprintf(file, "  \"session_id\": %d,\n", session->session_id);
    fprintf(file, "  \"type\": %d,\n", session->config.type);
    fprintf(file, "  \"start_time\": %ld,\n", session->start_time);
    fprintf(file, "  \"end_time\": %ld,\n", session->end_time);
    fprintf(file, "  \"sample_count\": %lu,\n", session->sample_count);
    fprintf(file, "  \"status\": %d\n", session->status);
    fprintf(file, "}\n");
    
    fclose(file);
    printf("Exported JSON report for session: %s to %s\n", session_name, output_path);
    return 0;
}

int kapi_profile_export_csv(kapi_profiler_t *profiler, const char *session_name,
                          const char *output_path)
{
    FILE *file = fopen(output_path, "w");
    if (!file) {
        profiler_set_error(profiler, -KAPI_EIO);
        return -KAPI_EIO;
    }
    
    /* 导出函数统计为 CSV */
    kapi_profile_function_t *functions = NULL;
    int func_count = 0;
    
    if (kapi_profile_analyze_functions(profiler, session_name, &functions, &func_count) == 0) {
        fprintf(file, "Function,Module,Call Count,Total Time,Min Time,Max Time,Avg Time,Memory Allocated\n");
        
        for (int i = 0; i < func_count; i++) {
            kapi_profile_function_t *func = &functions[i];
            fprintf(file, "%s,%s,%lu,%lu,%lu,%lu,%lu,%lu\n",
                   func->name, func->module,
                   func->call_count, func->total_time, func->min_time, func->max_time, func->avg_time,
                   func->memory_allocated);
        }
        
        free(functions);
    }
    
    fclose(file);
    printf("Exported CSV report for session: %s to %s\n", session_name, output_path);
    return 0;
}

int kapi_profile_export_svg(kapi_profiler_t *profiler, const char *session_name,
                          const char *output_path, const char *chart_type)
{
    FILE *file = fopen(output_path, "w");
    if (!file) {
        profiler_set_error(profiler, -KAPI_EIO);
        return -KAPI_EIO;
    }
    
    /* 生成简单的 SVG 图表 */
    fprintf(file, "<svg width=\"800\" height=\"600\" xmlns=\"http://www.w3.org/2000/svg\">\n");
    fprintf(file, "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");
    fprintf(file, "  <text x=\"400\" y=\"50\" text-anchor=\"middle\" font-size=\"24\" font-weight=\"bold\">\n");
    fprintf(file, "    Performance Report: %s\n", session_name);
    fprintf(file, "  </text>\n");
    fprintf(file, "  <text x=\"400\" y=\"100\" text-anchor=\"middle\" font-size=\"16\">\n");
    fprintf(file, "    Chart Type: %s\n", chart_type);
    fprintf(file, "  </text>\n");
    fprintf(file, "</svg>\n");
    
    fclose(file);
    printf("Exported SVG report for session: %s to %s\n", session_name, output_path);
    return 0;
}

/* 工具函数 */
const char *kapi_profile_type_to_string(kapi_profile_type_t type)
{
    switch (type) {
        case KAPI_PROFILE_CPU: return "CPU";
        case KAPI_PROFILE_MEMORY: return "Memory";
        case KAPI_PROFILE_IO: return "I/O";
        case KAPI_PROFILE_NETWORK: return "Network";
        case KAPI_PROFILE_SCHEDULING: return "Scheduling";
        case KAPI_PROFILE_SYSTEMCALL: return "SystemCall";
        default: return "Unknown";
    }
}

kapi_profile_type_t kapi_profile_string_to_type(const char *str)
{
    if (!str) return KAPI_PROFILE_CPU;
    
    if (strcmp(str, "CPU") == 0) return KAPI_PROFILE_CPU;
    if (strcmp(str, "Memory") == 0) return KAPI_PROFILE_MEMORY;
    if (strcmp(str, "IO") == 0) return KAPI_PROFILE_IO;
    if (strcmp(str, "Network") == 0) return KAPI_PROFILE_NETWORK;
    if (strcmp(str, "Scheduling") == 0) return KAPI_PROFILE_SCHEDULING;
    if (strcmp(str, "SystemCall") == 0) return KAPI_PROFILE_SYSTEMCALL;
    
    return KAPI_PROFILE_CPU;
}

const char *kapi_profile_mode_to_string(kapi_profile_mode_t mode)
{
    switch (mode) {
        case KAPI_PROFILE_MODE_SAMPLE: return "Sample";
        case KAPI_PROFILE_MODE_COUNT: return "Count";
        case KAPI_PROFILE_MODE_TRACE: return "Trace";
        case KAPI_PROFILE_MODE_AGGR: return "Aggregate";
        default: return "Unknown";
    }
}

kapi_profile_mode_t kapi_profile_string_to_mode(const char *str)
{
    if (!str) return KAPI_PROFILE_MODE_SAMPLE;
    
    if (strcmp(str, "Sample") == 0) return KAPI_PROFILE_MODE_SAMPLE;
    if (strcmp(str, "Count") == 0) return KAPI_PROFILE_MODE_COUNT;
    if (strcmp(str, "Trace") == 0) return KAPI_PROFILE_MODE_TRACE;
    if (strcmp(str, "Aggregate") == 0) return KAPI_PROFILE_MODE_AGGR;
    
    return KAPI_PROFILE_MODE_SAMPLE;
}