#ifndef KAPI_FTRACE_H
#define KAPI_FTRACE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_ftrace_entry kapi_ftrace_entry_t;

#define KAPI_FTRACE_MAX_DEPTH    64
#define KAPI_FTRACE_NAME_LEN     64
#define KAPI_FTRACE_BUFFER_SIZE  65536

typedef void (*kapi_ftrace_callback_t)(uint64_t ip, uint64_t parent_ip,
                                        uint64_t flags, void* data);

#define KAPI_FTRACE_FL_ENABLED   (1ULL << 0)
#define KAPI_FTRACE_FL_REGS      (1ULL << 1)
#define KAPI_FTRACE_FL_REGS_FULL (1ULL << 2)
#define KAPI_FTRACE_FL_TRAMP     (1ULL << 3)
#define KAPI_FTRACE_FL_IPMODIFY  (1ULL << 4)
#define KAPI_FTRACE_FL_DISABLED  (1ULL << 5)

typedef struct {
    uint64_t ip;
    uint64_t parent_ip;
} kapi_ftrace_ips_t;

typedef struct {
    uint64_t duration;
    uint64_t caller_ip;
    uint64_t hit_count;
    char     name[KAPI_FTRACE_NAME_LEN];
    int      depth;
    int      valid;
} kapi_ftrace_graph_entry_t;

int kapi_ftrace_register(const char* name, uint64_t ip, uint64_t flags,
                          kapi_ftrace_callback_t callback, void* data);

int kapi_ftrace_unregister(const char* name, uint64_t ip);

int kapi_ftrace_set_filter(const char* filter, int enable);

int kapi_ftrace_set_notrace(const char* filter, int enable);

int kapi_ftrace_set_ftrace_filter(const char* func);

int kapi_ftrace_set_ftrace_notrace(const char* func);

int kapi_ftrace_function_enabled(void);

int kapi_ftrace_tracing_on(void);

int kapi_ftrace_tracing_off(void);

int kapi_ftrace_trace_printk(const char* fmt, ...);

int kapi_ftrace_dump_trace(void);

kapi_ftrace_entry_t* kapi_ftrace_entry_get(void);

int kapi_ftrace_entry_release(kapi_ftrace_entry_t* entry);

int kapi_ftrace_graph_entry(uint64_t func_ip, uint64_t parent_ip,
                             uint64_t depth);

int kapi_ftrace_graph_return(kapi_ftrace_graph_entry_t* entry);

int kapi_ftrace_graph_dump(void);

int kapi_ftrace_init(void);

#ifdef __cplusplus
}
#endif

#endif