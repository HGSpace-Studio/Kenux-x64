/**
 * Kenux Kernel 性能分析器使用示例
 * 
 * 演示如何使用 kapi_profiler API 进行性能分析
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/kapi_profiler.h"

/* 模拟被分析的函数 */
void simulate_work(int work_type, int iterations)
{
    for (int i = 0; i < iterations; i++) {
        /* 模拟不同类型的工作 */
        switch (work_type) {
            case 0: /* CPU 密集型 */
                /* 模拟一些计算 */
                volatile int sum = 0;
                for (int j = 0; j < 1000; j++) {
                    sum += j;
                }
                break;
                
            case 1: /* 内存密集型 */
                /* 模拟内存分配 */
                volatile char *buffer = malloc(1024);
                if (buffer) {
                    memset(buffer, 0, 1024);
                    free(buffer);
                }
                break;
                
            case 2: /* I/O 密集型 */
                /* 模拟 I/O 操作 */
                FILE *file = fopen("/tmp/test_file", "a");
                if (file) {
                    fprintf(file, "test data\n");
                    fclose(file);
                }
                break;
        }
    }
}

/* 性能分析配置 */
static kapi_profile_config_t create_cpu_profile_config(void)
{
    kapi_profile_config_t config;
    memset(&config, 0, sizeof(config));
    
    config.type = KAPI_PROFILE_CPU;
    config.mode = KAPI_PROFILE_MODE_SAMPLE;
    config.sample_mode = KAPI_PROFILE_SAMPLE_TIME;
    config.sample_interval = 1000000; /* 1ms */
    config.buffer_size = 65536;
    config.max_samples = 1000;
    config.flags = KAPI_PROFILE_FL_ENABLED;
    strncpy(config.output_file, "/tmp/cpu_profile.json", sizeof(config.output_file) - 1);
    
    return config;
}

static kapi_profile_config_t create_memory_profile_config(void)
{
    kapi_profile_config_t config;
    memset(&config, 0, sizeof(config));
    
    config.type = KAPI_PROFILE_MEMORY;
    config.mode = KAPI_PROFILE_MODE_COUNT;
    config.sample_mode = KAPI_PROFILE_SAMPLE_MANUAL;
    config.buffer_size = 4096;
    config.max_samples = 500;
    config.flags = KAPI_PROFILE_FL_ENABLED;
    strncpy(config.output_file, "/tmp/memory_profile.json", sizeof(config.output_file) - 1);
    
    return config;
}

static kapi_profile_config_t create_io_profile_config(void)
{
    kapi_profile_config_t config;
    memset(&config, 0, sizeof(config));
    
    config.type = KAPI_PROFILE_IO;
    config.mode = KAPI_PROFILE_MODE_COUNT;
    config.sample_mode = KAPI_PROFILE_SAMPLE_EVENT;
    config.buffer_size = 8192;
    config.max_samples = 1000;
    config.flags = KAPI_PROFILE_FL_ENABLED;
    strncpy(config.output_file, "/tmp/io_profile.json", sizeof(config.output_file) - 1);
    
    return config;
}

static kapi_profile_config_t create_syscall_profile_config(void)
{
    kapi_profile_config_t config;
    memset(&config, 0, sizeof(config));
    
    config.type = KAPI_PROFILE_SYSTEMCALL;
    config.mode = KAPI_PROFILE_MODE_COUNT;
    config.sample_mode = KAPI_PROFILE_SAMPLE_EVENT;
    config.buffer_size = 4096;
    config.max_samples = 1000;
    config.flags = KAPI_PROFILE_FL_ENABLED;
    strncpy(config.output_file, "/tmp/syscall_profile.json", sizeof(config.output_file) - 1);
    
    return config;
}

/* 收集 CPU 性能样本 */
void collect_cpu_samples(kapi_profiler_t *profiler, const char *session_name, int count)
{
    printf("Collecting %d CPU samples...\n", count);
    
    for (int i = 0; i < count; i++) {
        kapi_profile_sample_t sample;
        memset(&sample, 0, sizeof(sample));
        
        sample.timestamp = 0; /* 实际使用时会设置时间戳 */
        sample.process_id = getpid();
        sample.thread_id = gettid();
        sample.sample_type = KAPI_PROFILE_CPU;
        
        /* 模拟采样 */
        simulate_work(0, 1000);
        
        kapi_profile_collect_sample(profiler, session_name, &sample);
        
        /* 小延迟 */
        usleep(100000); /* 100ms */
    }
}

/* 收集内存性能样本 */
void collect_memory_samples(kapi_profiler_t *profiler, const char *session_name)
{
    printf("Collecting memory samples...\n");
    
    /* 模拟内存分配和释放 */
    for (int i = 0; i < 100; i++) {
        /* 分配内存 */
        void *ptr1 = malloc(1024 * 1024); /* 1MB */
        void *ptr2 = malloc(512 * 1024);  /* 512KB */
        void *ptr3 = malloc(256 * 1024);  /* 256KB */
        
        /* 使用内存 */
        if (ptr1) memset(ptr1, 0, 1024 * 1024);
        if (ptr2) memset(ptr2, 0, 512 * 1024);
        if (ptr3) memset(ptr3, 0, 256 * 1024);
        
        /* 手动采集样本 */
        kapi_profile_manual_sample(profiler, session_name);
        
        /* 释放部分内存 */
        if (ptr2) free(ptr2);
        
        /* 再分配一些 */
        void *ptr4 = malloc(128 * 1024); /* 128KB */
        if (ptr4) memset(ptr4, 0, 128 * 1024);
        
        /* 释放剩余内存 */
        if (ptr1) free(ptr1);
        if (ptr3) free(ptr3);
        if (ptr4) free(ptr4);
        
        /* 手动采集样本 */
        kapi_profile_manual_sample(profiler, session_name);
        
        usleep(50000); /* 50ms */
    }
}

/* 收集 I/O 性能样本 */
void collect_io_samples(kapi_profiler_t *profiler, const char *session_name)
{
    printf("Collecting I/O samples...\n");
    
    /* 模拟各种 I/O 操作 */
    for (int i = 0; i < 50; i++) {
        /* 文件写入 */
        FILE *file = fopen("/tmp/test_io_file", "w");
        if (file) {
            for (int j = 0; j < 1000; j++) {
                fprintf(file, "This is test line %d\n", j);
            }
            fclose(file);
        }
        
        /* 手动采集样本 */
        kapi_profile_manual_sample(profiler, session_name);
        
        /* 文件读取 */
        file = fopen("/tmp/test_io_file", "r");
        if (file) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), file)) {
                /* 处理数据 */
            }
            fclose(file);
        }
        
        /* 手动采集样本 */
        kapi_profile_manual_sample(profiler, session_name);
        
        /* 网络操作模拟 */
        simulate_work(2, 10); /* 模拟 I/O 操作 */
        
        usleep(100000); /* 100ms */
    }
}

/* 收集系统调用样本 */
void collect_syscall_samples(kapi_profiler_t *profiler, const char *session_name)
{
    printf("Collecting syscall samples...\n");
    
    /* 模拟各种系统调用 */
    for (int i = 0; i < 100; i++) {
        /* 文件系统调用 */
        FILE *file = fopen("/tmp/test_syscall", "a");
        if (file) {
            fprintf(file, "test data %d\n", i);
            fclose(file);
        }
        
        /* 进程相关调用 */
        pid_t pid = getpid();
        uid_t uid = getuid();
        
        /* 内存相关调用 */
        void *ptr = malloc(1024);
        if (ptr) {
            free(ptr);
        }
        
        /* 手动采集样本 */
        kapi_profile_manual_sample(profiler, session_name);
        
        usleep(50000); /* 50ms */
    }
}

/* 分析并打印结果 */
void analyze_and_print_results(kapi_profiler_t *profiler, const char *session_name)
{
    printf("\n=== 分析结果: %s ===\n", session_name);
    
    /* 分析函数 */
    printf("\n1. 函数分析:\n");
    kapi_profile_function_t *functions = NULL;
    int func_count = 0;
    
    if (kapi_profile_analyze_functions(profiler, session_name, &functions, &func_count) == 0) {
        printf("找到 %d 个函数:\n", func_count);
        for (int i = 0; i < func_count && i < 5; i++) {
            kapi_profile_function_t *func = &functions[i];
            printf("  %s: 调用 %lu 次, 平均时间 %lu ns, 分配内存 %lu B\n",
                   func->name, func->call_count, func->avg_time, func->memory_allocated);
        }
        free(functions);
    }
    
    /* 分析 I/O 操作 */
    printf("\n2. I/O 操作分析:\n");
    kapi_profile_io_t *io_stats = NULL;
    int io_count = 0;
    
    if (kapi_profile_analyze_io(profiler, session_name, &io_stats, &io_count) == 0) {
        printf("找到 %d 个 I/O 操作:\n", io_count);
        for (int i = 0; i < io_count && i < 3; i++) {
            kapi_profile_io_t *io = &io_stats[i];
            printf("  操作 %d: %lu 次, %lu B/操作, 平均时间 %lu ns\n",
                   i, io->operation_count, io->avg_bytes, io->avg_time);
        }
        free(io_stats);
    }
    
    /* 分析内存使用 */
    printf("\n3. 内存使用分析:\n");
    kapi_profile_memory_t *memory_stats = NULL;
    int mem_count = 0;
    
    if (kapi_profile_analyze_memory(profiler, session_name, &memory_stats, &mem_count) == 0) {
        kapi_profile_memory_t *mem = memory_stats;
        printf("当前使用: %lu MB, 峰值使用: %lu MB\n",
               mem->current_usage / (1024 * 1024),
               mem->peak_usage / (1024 * 1024));
        printf("分配次数: %lu, 释放次数: %lu, 碎片化: %lu%%\n",
               mem->allocation_count, mem->free_count, mem->fragmentation_ratio);
        free(memory_stats);
    }
    
    /* 分析系统调用 */
    printf("\n4. 系统调用分析:\n");
    kapi_profile_syscall_t *syscall_stats = NULL;
    int syscall_count = 0;
    
    if (kapi_profile_analyze_syscalls(profiler, session_name, &syscall_stats, &syscall_count) == 0) {
        printf("找到 %d 个系统调用:\n", syscall_count);
        for (int i = 0; i < syscall_count && i < 5; i++) {
            kapi_profile_syscall_t *sc = &syscall_stats[i];
            printf("  %s: 调用 %lu 次, 平均时间 %lu ns, 错误 %lu 次\n",
                   sc->name, sc->call_count, sc->avg_time, sc->error_count);
        }
        free(syscall_stats);
    }
    
    /* 查找热点 */
    printf("\n5. 热点分析:\n");
    kapi_profile_hotspot_t *hotspots = NULL;
    int hotspot_count = 5;
    
    if (kapi_profile_find_hotspots(profiler, session_name, &hotspots, &hotspot_count, KAPI_PROFILE_CPU) == 0) {
        printf("找到 %d 个热点:\n", hotspot_count);
        for (int i = 0; i < hotspot_count; i++) {
            kapi_profile_hotspot_t *hs = &hotspots[i];
            printf("  %s: 命中 %lu 次, 占比 %lu%%\n",
                   hs->name, hs->hit_count, hs->percentage);
        }
        free(hotspots);
    }
}

/* 生成报告 */
void generate_reports(kapi_profiler_t *profiler, const char *session_name)
{
    printf("\n生成报告...\n");
    
    /* 生成 JSON 报告 */
    if (kapi_profile_generate_report(profiler, session_name, "/tmp/profile_report.json", "json") == 0) {
        printf("JSON 报告已生成: /tmp/profile_report.json\n");
    }
    
    /* 生成 CSV 报告 */
    if (kapi_profile_generate_report(profiler, session_name, "/tmp/profile_report.csv", "csv") == 0) {
        printf("CSV 报告已生成: /tmp/profile_report.csv\n");
    }
    
    /* 生成 SVG 报告 */
    if (kapi_profile_generate_report(profiler, session_name, "/tmp/profile_report.svg", "svg") == 0) {
        printf("SVG 报告已生成: /tmp/profile_report.svg\n");
    }
}

int main()
{
    kapi_profiler_t *profiler = NULL;
    
    printf("=== Kenux Kernel 性能分析器示例 ===\n");
    
    /* 1. 初始化性能分析器 */
    printf("\n1. 初始化性能分析器...\n");
    if (kapi_profiler_init(&profiler) != 0) {
        printf("错误: 无法初始化性能分析器\n");
        return 1;
    }
    printf("性能分析器初始化成功\n");
    
    /* 2. 创建和启动 CPU 分析会话 */
    printf("\n2. 创建 CPU 分析会话...\n");
    kapi_profile_config_t cpu_config = create_cpu_profile_config();
    if (kapi_profile_start_session(profiler, "cpu_session", &cpu_config) == 0) {
        printf("CPU 会话创建成功\n");
        
        /* 启动采样 */
        kapi_profile_start_sampling(profiler, "cpu_session");
        
        /* 收集 CPU 样本 */
        collect_cpu_samples(profiler, "cpu_session", 50);
        
        /* 停止采样和会话 */
        kapi_profile_stop_sampling(profiler, "cpu_session");
        kapi_profile_stop_session(profiler, "cpu_session");
        
        /* 分析结果 */
        analyze_and_print_results(profiler, "cpu_session");
    }
    
    /* 3. 创建和启动内存分析会话 */
    printf("\n3. 创建内存分析会话...\n");
    kapi_profile_config_t memory_config = create_memory_profile_config();
    if (kapi_profile_start_session(profiler, "memory_session", &memory_config) == 0) {
        printf("内存会话创建成功\n");
        
        /* 收集内存样本 */
        collect_memory_samples(profiler, "memory_session");
        
        /* 停止会话 */
        kapi_profile_stop_session(profiler, "memory_session");
        
        /* 分析结果 */
        analyze_and_print_results(profiler, "memory_session");
    }
    
    /* 4. 创建和启动 I/O 分析会话 */
    printf("\n4. 创建 I/O 分析会话...\n");
    kapi_profile_config_t io_config = create_io_profile_config();
    if (kapi_profile_start_session(profiler, "io_session", &io_config) == 0) {
        printf("I/O 会话创建成功\n");
        
        /* 收集 I/O 样本 */
        collect_io_samples(profiler, "io_session");
        
        /* 停止会话 */
        kapi_profile_stop_session(profiler, "io_session");
        
        /* 分析结果 */
        analyze_and_print_results(profiler, "io_session");
    }
    
    /* 5. 创建和启动系统调用分析会话 */
    printf("\n5. 创建系统调用分析会话...\n");
    kapi_profile_config_t syscall_config = create_syscall_profile_config();
    if (kapi_profile_start_session(profiler, "syscall_session", &syscall_config) == 0) {
        printf("系统调用会话创建成功\n");
        
        /* 收集系统调用样本 */
        collect_syscall_samples(profiler, "syscall_session");
        
        /* 停止会话 */
        kapi_profile_stop_session(profiler, "syscall_session");
        
        /* 分析结果 */
        analyze_and_print_results(profiler, "syscall_session");
    }
    
    /* 6. 生成报告 */
    printf("\n6. 生成报告...\n");
    generate_reports(profiler, "cpu_session");
    generate_reports(profiler, "memory_session");
    generate_reports(profiler, "io_session");
    generate_reports(profiler, "syscall_session");
    
    /* 7. 列出所有会话 */
    printf("\n7. 列出所有会话...\n");
    kapi_profile_session_t *sessions = NULL;
    int session_count = 0;
    
    if (kapi_profile_list_sessions(profiler, &sessions, &session_count) == 0) {
        printf("共有 %d 个会话:\n", session_count);
        for (int i = 0; i < session_count; i++) {
            kapi_profile_session_t *sess = &sessions[i];
            printf("  %s (ID: %d, 类型: %d, 样本数: %lu)\n",
                   sess->name, sess->session_id, sess->config.type, sess->sample_count);
        }
        free(sessions);
    }
    
    /* 8. 删除会话 */
    printf("\n8. 删除会话...\n");
    kapi_profile_delete_session(profiler, "cpu_session");
    kapi_profile_delete_session(profiler, "memory_session");
    kapi_profile_delete_session(profiler, "io_session");
    kapi_profile_delete_session(profiler, "syscall_session");
    printf("所有会话已删除\n");
    
    /* 9. 销毁性能分析器 */
    printf("\n9. 销毁性能分析器...\n");
    kapi_profiler_destroy(profiler);
    printf("性能分析器已销毁\n");
    
    printf("\n=== 示例完成 ===\n");
    return 0;
}