/**
 * Kenux Kernel 调试器使用示例
 * 
 * 演示如何使用 kapi_debugger API 进行进程调试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/kapi_debugger.h"

/* 调试事件回调函数 */
static void debug_event_callback(const kapi_debug_event_t *event, void *user_data)
{
    printf("[DEBUG EVENT] Type: %d, PID: %d, Timestamp: %lu\n", 
           event->type, event->pid, event->timestamp);
    
    switch (event->type) {
        case KAPI_DEBUG_EVENT_BREAKPOINT:
            printf("  Breakpoint hit at 0x%lx\n", event->rip);
            if (event->breakpoint) {
                printf("  Breakpoint ID: %lu, Hit count: %d\n", 
                       event->breakpoint->id, event->breakpoint->hit_count);
            }
            break;
            
        case KAPI_DEBUG_EVENT_EXCEPTION:
            printf("  Exception received, signal: %d\n", event->signal_number);
            break;
            
        case KAPI_DEBUG_EVENT_ATTACH:
            printf("  Process attached successfully\n");
            break;
            
        case KAPI_DEBUG_EVENT_DETACH:
            printf("  Process detached\n");
            break;
            
        case KAPI_DEBUG_EVENT_EXIT:
            printf("  Process exited\n");
            break;
            
        default:
            printf("  Unknown event type\n");
            break;
    }
}

int main()
{
    kapi_debugger_t *debugger = NULL;
    kapi_debug_pid_t target_pid = 1234; /* 目标进程ID */
    
    printf("=== Kenux Kernel 调试器示例 ===\n");
    
    /* 1. 初始化调试器 */
    printf("\n1. 初始化调试器...\n");
    if (kapi_debugger_init(&debugger) != 0) {
        printf("错误: 无法初始化调试器\n");
        return 1;
    }
    printf("调试器初始化成功，ID: %lu\n", debugger->debugger_id);
    
    /* 2. 设置事件回调 */
    printf("\n2. 设置事件回调...\n");
    kapi_debugger_set_event_callback(debugger, debug_event_callback, NULL);
    
    /* 3. 附加到目标进程 */
    printf("\n3. 附加到进程 %d...\n", target_pid);
    if (kapi_debugger_attach(debugger, target_pid) != 0) {
        printf("警告: 无法附加到进程 %d (可能是进程不存在)\n", target_pid);
        /* 创建一个模拟进程进行测试 */
        target_pid = kapi_debugger_create_process(debugger, "/tmp/test_process", NULL);
        printf("创建模拟进程: %d\n", target_pid);
    }
    
    /* 4. 添加断点 */
    printf("\n4. 添加断点...\n");
    uint64_t breakpoint_address = 0x1000; /* 断点地址 */
    
    /* 添加软断点 */
    if (kapi_debugger_breakpoint_add(debugger, target_pid, breakpoint_address,
                                   KAPI_BREAKPOINT_SOFT, KAPI_BREAKPOINT_SIZE_1,
                                   KAPI_BREAKPOINT_ENABLED, NULL) == 0) {
        printf("成功添加软断点在地址 0x%lx\n", breakpoint_address);
    }
    
    /* 添加条件断点 */
    const char *condition = "eax == 0";
    if (kapi_debugger_breakpoint_add(debugger, target_pid, 0x2000,
                                   KAPI_BREAKPOINT_CONDITIONAL, KAPI_BREAKPOINT_SIZE_4,
                                   KAPI_BREAKPOINT_ENABLED, condition) == 0) {
        printf("成功添加条件断点在地址 0x%lx\n", 0x2000);
    }
    
    /* 5. 列出所有断点 */
    printf("\n5. 列出所有断点...\n");
    kapi_breakpoint_t *breakpoints = NULL;
    int breakpoint_count = 0;
    
    if (kapi_debugger_breakpoint_list(debugger, target_pid, &breakpoints, &breakpoint_count) == 0) {
        printf("找到 %d 个断点:\n", breakpoint_count);
        for (int i = 0; i < breakpoint_count; i++) {
            kapi_breakpoint_t *bp = &breakpoints[i];
            printf("  断点 %d: 地址=0x%lx, 类型=%d, 标志=0x%lx, 命中次数=%d\n",
                   i, bp->address, bp->type, bp->flags, bp->hit_count);
            if (bp->condition[0] != '\0') {
                printf("    条件: %s\n", bp->condition);
            }
        }
        free(breakpoints);
    }
    
    /* 6. 获取进程信息 */
    printf("\n6. 获取进程信息...\n");
    char *process_name = NULL;
    int process_status = 0;
    uint64_t base_address = 0;
    
    if (kapi_debugger_get_process_info(debugger, target_pid, &process_name, &process_status, &base_address) == 0) {
        printf("进程名称: %s\n", process_name);
        printf("进程状态: %s\n", process_status ? "运行中" : "已停止");
        printf("基地址: 0x%lx\n", base_address);
        free(process_name);
    }
    
    /* 7. 获取寄存器 */
    printf("\n7. 获取寄存器...\n");
    kapi_debug_registers_t registers;
    
    if (kapi_debugger_get_registers(debugger, target_pid, &registers) == 0) {
        printf("寄存器状态:\n");
        printf("  RIP: 0x%lx\n", registers.rip);
        printf("  RSP: 0x%lx\n", registers.rsp);
        printf("  RBP: 0x%lx\n", registers.rbp);
        printf("  RAX: 0x%lx\n", registers.rax);
        printf("  RBX: 0x%lx\n", registers.rbx);
    }
    
    /* 8. 内存读取 */
    printf("\n8. 内存读取...\n");
    uint8_t memory_buffer[64];
    ssize_t bytes_read = kapi_debugger_read_memory(debugger, target_pid, 
                                                  memory_buffer, sizeof(memory_buffer), 
                                                  0x1000);
    
    if (bytes_read > 0) {
        printf("从地址 0x1000 读取了 %ld 字节:\n", bytes_read);
        for (int i = 0; i < bytes_read && i < 16; i++) {
            printf("  0x%02x ", memory_buffer[i]);
        }
        printf("\n");
    }
    
    /* 9. 栈回溯 */
    printf("\n9. 获取栈回溯...\n");
    kapi_debug_frame_t *frames = NULL;
    int frame_depth = 0;
    
    if (kapi_debugger_get_backtrace(debugger, target_pid, &frames, &frame_depth) == 0) {
        printf("找到 %d 个栈帧:\n", frame_depth);
        
        char **function_names = NULL;
        char **source_files = NULL;
        
        if (kapi_debugger_resolve_backtrace(debugger, target_pid, frames, frame_depth,
                                         &function_names, &source_files) == 0) {
            for (int i = 0; i < frame_depth; i++) {
                printf("  帧 %d:\n", i);
                printf("    函数: %s\n", function_names[i]);
                printf("    源文件: %s\n", source_files[i]);
                printf("    指令指针: 0x%lx\n", frames[i].instruction_pointer);
                printf("    返回地址: 0x%lx\n", frames[i].return_address);
                free(function_names[i]);
                free(source_files[i]);
            }
            free(function_names);
            free(source_files);
        }
        
        free(frames);
    }
    
    /* 10. 获取调试符号 */
    printf("\n10. 获取调试符号...\n");
    kapi_debug_symbol_t *symbols = NULL;
    int symbol_count = 0;
    
    if (kapi_debugger_list_symbols(debugger, target_pid, &symbols, &symbol_count) == 0) {
        printf("找到 %d 个符号:\n", symbol_count);
        for (int i = 0; i < symbol_count; i++) {
            printf("  符号 %d: %s @ 0x%lx (大小: %lu)\n", 
                   i, symbols[i].name, symbols[i].address, symbols[i].size);
        }
        free(symbols);
    }
    
    /* 11. 模拟调试操作 */
    printf("\n11. 模拟调试操作...\n");
    
    /* 继续执行 */
    printf("  继续执行...\n");
    kapi_debugger_continue(debugger, target_pid);
    
    /* 单步执行 */
    printf("  单步进入...\n");
    kapi_debugger_step(debugger, target_pid, KAPI_DEBUG_STEP_INTO);
    
    /* 强制中断 */
    printf("  强制中断...\n");
    kapi_debugger_break(debugger, target_pid);
    
    /* 线程管理 */
    printf("  列出线程...\n");
    kapi_debug_thread_t *threads = NULL;
    int thread_count = 0;
    
    if (kapi_debugger_list_threads(debugger, target_pid, &threads, &thread_count) == 0) {
        printf("  找到 %d 个线程:\n", thread_count);
        for (int i = 0; i < thread_count; i++) {
            printf("    线程 %d: 状态=%d, RIP=0x%lx\n", 
                   threads[i].pid, threads[i].state, threads[i].instruction_pointer);
        }
        free(threads);
    }
    
    /* 12. 清理断点 */
    printf("\n12. 清理断点...\n");
    kapi_debugger_breakpoint_clear(debugger, target_pid);
    printf("已清除所有断点\n");
    
    /* 13. 分离进程 */
    printf("\n13. 分离进程...\n");
    kapi_debugger_detach(debugger, target_pid);
    printf("已分离进程 %d\n", target_pid);
    
    /* 14. 销毁调试器 */
    printf("\n14. 销毁调试器...\n");
    kapi_debugger_destroy(debugger);
    printf("调试器已销毁\n");
    
    printf("\n=== 示例完成 ===\n");
    return 0;
}