#ifndef FTRACE_H
#define FTRACE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define FTRACE_BUFFER_SIZE      (1 << 18)
#define FTRACE_BUFFER_MASK      (FTRACE_BUFFER_SIZE - 1)
#define FTRACE_MAX_TRACERS      16
#define FTRACE_MAX_TRACEPOINTS  128
#define FTRACE_MAX_GRAPH_DEPTH  32
#define FTRACE_FUNC_SIZE        256
#define FTRACE_EVENT_NAME_LEN   32

#define FTRACE_TYPE_ENTER       0
#define FTRACE_TYPE_EXIT        1
#define FTRACE_TYPE_TRACEPOINT  2
#define FTRACE_TYPE_SPECIAL     3

struct ftrace_event {
    uint64_t timestamp;
    uint64_t ip;
    uint64_t parent_ip;
    uint32_t type;
    uint32_t pid;
    uint32_t depth;
    char func[FTRACE_FUNC_SIZE];
};

struct ftrace_ring_buffer {
    struct ftrace_event* buffer;
    uint64_t head;
    uint64_t tail;
    uint64_t dropped;
    uint64_t entries;
    spinlock_t lock;
};

struct ftrace_func_entry {
    uint64_t ip;
    uint64_t parent_ip;
    uint64_t count;
    uint64_t time;
    uint64_t time_max;
    uint64_t time_min;
    char name[FTRACE_FUNC_SIZE];
    struct ftrace_func_entry* next;
};

struct ftrace_graph_entry {
    uint64_t func;
    uint64_t depth;
    uint64_t start_time;
};

struct ftrace_graph {
    struct ftrace_graph_entry stack[FTRACE_MAX_GRAPH_DEPTH];
    uint64_t curr_depth;
    spinlock_t lock;
};

struct tracepoint {
    char name[FTRACE_EVENT_NAME_LEN];
    void* probe_func;
    void* priv;
    int enabled;
    struct tracepoint* next;
};

struct tracer {
    char name[FTRACE_EVENT_NAME_LEN];
    void (*start)(void);
    void (*stop)(void);
    void (*reset)(void);
    void (*print)(void);
    int enabled;
    struct tracer* next;
};

struct ftrace_ops {
    void (*func)(unsigned long ip, unsigned long parent_ip, struct ftrace_ops* op, struct pt_regs* regs);
    void* private;
    int flags;
};

struct ftrace_filter {
    uint64_t start;
    uint64_t end;
    struct ftrace_filter* next;
};

struct ftrace_state {
    int enabled;
    int graph_enabled;
    int function_enabled;
    struct ftrace_ring_buffer* buffer;
    struct ftrace_func_entry* func_hash[256];
    struct ftrace_graph* graph;
    struct tracer* tracers;
    struct tracepoint* tracepoints;
    spinlock_t lock;
};

void ftrace_init(void);
void ftrace_enable(void);
void ftrace_disable(void);
void ftrace_reset(void);

void ftrace_record_entry(unsigned long ip, unsigned long parent_ip);
void ftrace_record_exit(unsigned long ip, unsigned long parent_ip);
void ftrace_graph_entry_record(unsigned long ip, unsigned long parent_ip);
void ftrace_graph_return_record(unsigned long ip);

void ftrace_tracepoint_register(const char* name, void* probe);
void ftrace_tracepoint_unregister(const char* name);
void ftrace_tracepoint_call(const char* name, void* data);

void ftrace_tracer_register(struct tracer* tracer);
void ftrace_tracer_enable(const char* name);
void ftrace_tracer_disable(const char* name);

uint64_t ftrace_read_buffer(struct ftrace_event* events, uint64_t max_count);
void ftrace_clear_buffer(void);
uint64_t ftrace_now(void);

void mcount(void);
void __ftrace_entry(unsigned long ip, unsigned long parent_ip);
void __ftrace_exit(unsigned long ip, unsigned long parent_ip);

extern struct ftrace_state ftrace_global;

#endif
