#ifndef _ARCH_FTRACE_H
#define _ARCH_FTRACE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define FTRACE_RING_SIZE       (4 * 1024 * 1024)
#define FTRACE_FUNC_NAME_MAX   256
#define FTRACE_MAX_FILTERS     64
#define FTRACE_MAX_GRAPH_DEPTH 32
#define FTRACE_MAX_TRACERS     8
#define FTRACE_MAX_EVENTS      1024

typedef struct {
    u64 ip;
    u64 parent_ip;
    u64 timestamp;
} ftrace_func_entry_t;

typedef struct {
    ftrace_func_entry_t *entries;
    u64 head;
    u64 tail;
    u64 count;
    u64 dropped;
    spinlock_t lock;
} ftrace_ring_buffer_t;

typedef struct {
    u64 addr;
    char name[FTRACE_FUNC_NAME_MAX];
} ftrace_func_map_t;

typedef struct {
    u64 start;
    u64 end;
    char pattern[FTRACE_FUNC_NAME_MAX];
} ftrace_filter_t;

typedef struct {
    u64 func;
    u64 call_time;
    u64 depth;
    u64 overhead;
} ftrace_graph_entry_t;

typedef struct {
    ftrace_graph_entry_t stack[FTRACE_MAX_GRAPH_DEPTH];
    u64 current_depth;
    spinlock_t lock;
} ftrace_graph_ret_t;

#define FTRACE_TYPE_FUNCTION        0
#define FTRACE_TYPE_FUNCTION_GRAPH  1

typedef struct {
    char name[32];
    int type;
    int (*start)(void);
    int (*stop)(void);
    void (*reset)(void);
    int enabled;
} ftrace_tracer_t;

#define FTRACE_ENABLED     0
#define FTRACE_NOTRACE     0x100000

extern int ftrace_enabled;
extern ftrace_ring_buffer_t ftrace_per_cpu_buffer[1];
extern ftrace_func_map_t *ftrace_func_maps;
extern u64 ftrace_func_map_count;
extern ftrace_filter_t ftrace_filters[FTRACE_MAX_FILTERS];
extern u32 ftrace_filter_count;
extern ftrace_graph_ret_t ftrace_graph;

void ftrace_init(void);
void ftrace_enable(void);
void ftrace_disable(void);

void ftrace_set_filter(const char *func_pattern);
void ftrace_clear_filter(void);
int ftrace_match_filter(u64 ip);

void ftrace_record_entry(u64 ip, u64 parent_ip);
void ftrace_record_exit(u64 ip, u64 parent_ip);

void ftrace_graph_entry(u64 ip, u64 parent_ip);
void ftrace_graph_return(u64 ip);

void ftrace_dump_buffer(void);
void ftrace_clear_buffer(void);
u64 ftrace_now(void);

int ftrace_set_trace(const char *tracer_name);
const char *ftrace_get_current_trace(void);

void __ftrace_entry(u64 ip, u64 parent_ip);
void __ftrace_return(u64 ip);

#endif
