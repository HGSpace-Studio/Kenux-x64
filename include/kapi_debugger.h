#ifndef KAPI_DEBUGGER_H
#define KAPI_DEBUGGER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 调试器会话管理 */
typedef struct kapi_debugger kapi_debugger_t;
typedef struct kapi_debug_session kapi_debug_session_t;

/* 进程标识符 */
typedef int kapi_debug_pid_t;

/* 断点类型 */
typedef enum {
    KAPI_BREAKPOINT_SOFT = 0,    /* 软断点 (INT3) */
    KAPI_BREAKPOINT_HARD,       /* 硬断点 (调试寄存器) */
    KAPI_BREAKPOINT_WATCH,      /* 硬件断点 (观察点) */
    KAPI_BREAKPOINT_CONDITIONAL  /* 条件断点 */
} kapi_breakpoint_type_t;

/* 断点大小 */
typedef enum {
    KAPI_BREAKPOINT_SIZE_1 = 0,  /* 1字节 */
    KAPI_BREAKPOINT_SIZE_2,    /* 2字节 */
    KAPI_BREAKPOINT_SIZE_4,    /* 4字节 */
    KAPI_BREAKPOINT_SIZE_8     /* 8字节 */
} kapi_breakpoint_size_t;

/* 断点标志 */
#define KAPI_BREAKPOINT_ENABLED    (1U << 0)
#define KAPI_BREAKPOINT_TEMPORARY  (1U << 1)
#define KAPI_BREAKPOINT_SINGLESHOT  (1U << 2)
#define KAPI_BREAKPOINT_CONDITION  (1U << 3)

/* 断点结构 */
typedef struct kapi_breakpoint {
    uint64_t id;                     /* 唯一标识符 */
    kapi_breakpoint_type_t type;    /* 断点类型 */
    kapi_breakpoint_size_t size;    /* 断点大小 */
    uint64_t address;               /* 断点地址 */
    uint64_t flags;                 /* 断点标志 */
    char condition[256];            /* 条件表达式 */
    kapi_debug_pid_t pid;          /* 所属进程ID */
    int hit_count;                  /* 命中次数 */
    uint64_t last_hit_time;        /* 最后命中时间 */
} kapi_breakpoint_t;

/* 寄存器集 */
typedef struct kapi_debug_registers {
    /* 通用寄存器 */
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    
    /* 指令指针 */
    uint64_t rip;
    
    /* 标志寄存器 */
    uint64_t rflags;
    
    /* 段寄存器 */
    uint16_t cs, ds, es, fs, gs, ss;
    
    /* 调试寄存器 */
    uint64_t dr0, dr1, dr2, dr3, dr6, dr7;
    
    /* MMX寄存器 (可选) */
    uint64_t mmx[8];
    
    /* XMM寄存器 (可选) */
    uint64_t xmm[16];
    
    /* YMM寄存器 (可选) */
    uint64_t ymm[16];
} kapi_debug_registers_t;

/* 调试动作 */
typedef enum {
    KAPI_DEBUG_CONTINUE = 0,      /* 继续执行 */
    KAPI_DEBUG_STEP_INTO,         /* 单步进入 */
    KAPI_DEBUG_STEP_OVER,         /* 单步跳过 */
    KAPI_DEBUG_STEP_OUT,          /* 单步跳出 */
    KAPI_DEBUG_BREAK,            /* 强制中断 */
    KAPI_DEBUG_ATTACH,            /* 附加进程 */
    KAPI_DEBUG_DETACH             /* 分离进程 */
} kapi_debug_action_t;

/* 调试事件 */
typedef enum {
    KAPI_DEBUG_EVENT_BREAKPOINT = 0,   /* 断点命中 */
    KAPI_DEBUG_EVENT_EXCEPTION,        /* 异常发生 */
    KAPI_DEBUG_EVENT_SIGNAL,          /* 信号收到 */
    KAPI_DEBUG_EVENT_ATTACH,           /* 附加成功 */
    KAPI_DEBUG_EVENT_DETACH,           /* 分离成功 */
    KAPI_DEBUG_EVENT_EXIT              /* 进程退出 */
} kapi_debug_event_type_t;

typedef struct kapi_debug_event {
    kapi_debug_event_type_t type;      /* 事件类型 */
    kapi_debug_pid_t pid;             /* 进程ID */
    uint64_t timestamp;               /* 时间戳 */
    uint64_t rip;                    /* 指令指针 */
    int signal_number;               /* 信号号（如果适用） */
    kapi_breakpoint_t *breakpoint;   /* 断点信息（如果适用） */
    void *user_data;                 /* 用户数据 */
} kapi_debug_event_t;

/* 调试会话回调函数类型 */
typedef void (*kapi_debug_event_callback_t)(const kapi_debug_event_t *event, void *user_data);

/* 调试器初始化和销毁 */
int kapi_debugger_init(kapi_debugger_t **debugger);
int kapi_debugger_destroy(kapi_debugger_t *debugger);

/* 进程调试操作 */
int kapi_debugger_attach(kapi_debugger_t *debugger, kapi_debug_pid_t pid);
int kapi_debugger_detach(kapi_debugger_t *debugger, kapi_debug_pid_t pid);
int kapi_debugger_create_process(kapi_debugger_t *debugger, const char *path, char **argv);
int kapi_debugger_terminate_process(kapi_debugger_t *debugger, kapi_debug_pid_t pid);

/* 调试控制 */
int kapi_debugger_continue(kapi_debugger_t *debugger, kapi_debug_pid_t pid);
int kapi_debugger_step(kapi_debugger_t *debugger, kapi_debug_pid_t pid, kapi_debug_action_t action);
int kapi_debugger_break(kapi_debugger_t *debugger, kapi_debug_pid_t pid);

/* 断点管理 */
int kapi_debugger_breakpoint_add(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                 uint64_t address, kapi_breakpoint_type_t type,
                                 kapi_breakpoint_size_t size, uint64_t flags,
                                 const char *condition);
int kapi_debugger_breakpoint_remove(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                    uint64_t address);
int kapi_debugger_breakpoint_enable(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                    uint64_t address, int enable);
int kapi_debugger_breakpoint_list(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                  kapi_breakpoint_t **breakpoints, int *count);
int kapi_debugger_breakpoint_clear(kapi_debugger_t *debugger, kapi_debug_pid_t pid);

/* 寄存器访问 */
int kapi_debugger_get_registers(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                kapi_debug_registers_t *regs);
int kapi_debugger_set_registers(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                const kapi_debug_registers_t *regs);
int kapi_debugger_get_register(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                              const char *reg_name, uint64_t *value);
int kapi_debugger_set_register(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                              const char *reg_name, uint64_t value);

/* 内存访问 */
ssize_t kapi_debugger_read_memory(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                 void *buf, size_t count, uint64_t address);
ssize_t kapi_debugger_write_memory(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                  const void *buf, size_t count, uint64_t address);
int kapi_debugger_modify_memory(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                               uint64_t address, const void *old_data, size_t old_size,
                               const void *new_data, size_t new_size);

/* 调试符号 */
typedef struct kapi_debug_symbol {
    char name[256];                    /* 符号名称 */
    char module[256];                 /* 所属模块 */
    uint64_t address;                 /* 符号地址 */
    uint64_t size;                    /* 符号大小 */
    int type;                         /* 符号类型 (函数/变量等) */
    int section;                      /* 所属段 */
    int line_number;                  /* 行号 (如果有调试信息) */
    char file_name[256];              /* 源文件名 (如果有调试信息) */
} kapi_debug_symbol_t;

int kapi_debugger_load_symbols(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                               const char *symbol_file);
int kapi_debugger_lookup_symbol(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                               const char *name, kapi_debug_symbol_t *symbol);
int kapi_debugger_lookup_address(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                uint64_t address, kapi_debug_symbol_t *symbol);
int kapi_debugger_list_symbols(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                              kapi_debug_symbol_t **symbols, int *count);

/* 栈回溯 */
typedef struct kapi_debug_frame {
    uint64_t instruction_pointer;     /* 指令指针 */
    uint64_t stack_pointer;          /* 栈指针 */
    uint64_t base_pointer;           /* 基址指针 */
    uint64_t return_address;         /* 返回地址 */
    int frame_number;                /* 帧号 */
} kapi_debug_frame_t;

int kapi_debugger_get_backtrace(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                               kapi_debug_frame_t **frames, int *depth);
int kapi_debugger_resolve_backtrace(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                  const kapi_debug_frame_t *frames, int depth,
                                  char **function_names, char **source_files);

/* 线程管理 */
typedef struct kapi_debug_thread {
    kapi_debug_pid_t pid;             /* 线程ID */
    uint64_t rip;                    /* 指令指针 */
    int state;                       /* 线程状态 (运行/停止/退出) */
    uint64_t wait_reason;           /* 等待原因 */
    kapi_debug_registers_t regs;      /* 寄存器状态 */
} kapi_debug_thread_t;

int kapi_debugger_list_threads(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                              kapi_debug_thread_t **threads, int *count);
int kapi_debugger_thread_suspend(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                kapi_debug_pid_t tid);
int kapi_debugger_thread_resume(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                               kapi_debug_pid_t tid);

/* 事件回调管理 */
int kapi_debugger_set_event_callback(kapi_debugger_t *debugger,
                                    kapi_debug_event_callback_t callback,
                                    void *user_data);
int kapi_debugger_remove_event_callback(kapi_debugger_t *debugger);

/* 调试信息 */
int kapi_debugger_get_process_info(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                   char **name, int *status, uint64_t *base_address);
int kapi_debugger_get_debug_info(kapi_debugger_t *debugger, kapi_debug_pid_t pid,
                                char **debug_path, int *debug_available);

/* 错误处理 */
const char *kapi_debugger_get_error_string(int error_code);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_DEBUGGER_H */