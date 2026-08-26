#include <arch/ftrace.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

int ftrace_enabled = 0;

ftrace_ring_buffer_t ftrace_per_cpu_buffer[1];
ftrace_func_map_t *ftrace_func_maps = NULL;
u64 ftrace_func_map_count = 0;
ftrace_filter_t ftrace_filters[FTRACE_MAX_FILTERS];
u32 ftrace_filter_count = 0;
ftrace_graph_ret_t ftrace_graph;

static ftrace_tracer_t ftrace_tracers[FTRACE_MAX_TRACERS];
static u32 ftrace_current_tracer = 0;
static u32 ftrace_tracer_count = 0;

static u64 ftrace_ring_entries_count(void)
{
    return FTRACE_RING_SIZE / sizeof(ftrace_func_entry_t);
}

static u64 ftrace_read_tsc(void)
{
    u32 lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

u64 ftrace_now(void)
{
    return ftrace_read_tsc();
}

void ftrace_init(void)
{
    ftrace_enabled = 0;
    ftrace_func_maps = NULL;
    ftrace_func_map_count = 0;
    ftrace_filter_count = 0;
    ftrace_tracer_count = 0;
    ftrace_current_tracer = 0;

    memset(ftrace_filters, 0, sizeof(ftrace_filters));
    memset(&ftrace_graph, 0, sizeof(ftrace_graph));
    spin_init(&ftrace_graph.lock);

    u64 entry_count = ftrace_ring_entries_count();
    ftrace_per_cpu_buffer[0].entries = (ftrace_func_entry_t *)kzalloc(FTRACE_RING_SIZE);
    ftrace_per_cpu_buffer[0].head = 0;
    ftrace_per_cpu_buffer[0].tail = 0;
    ftrace_per_cpu_buffer[0].count = 0;
    ftrace_per_cpu_buffer[0].dropped = 0;
    spin_init(&ftrace_per_cpu_buffer[0].lock);
    (void)entry_count;
}

void ftrace_enable(void)
{
    __sync_lock_test_and_set(&ftrace_enabled, 1);
}

void ftrace_disable(void)
{
    __sync_lock_test_and_set(&ftrace_enabled, 0);
}

void ftrace_set_filter(const char *func_pattern)
{
    if (!func_pattern || ftrace_filter_count >= FTRACE_MAX_FILTERS) return;
    spin_lock(&ftrace_graph.lock);
    ftrace_filter_t *f = &ftrace_filters[ftrace_filter_count++];
    strncpy(f->pattern, func_pattern, FTRACE_FUNC_NAME_MAX - 1);
    f->pattern[FTRACE_FUNC_NAME_MAX - 1] = '\0';
    f->start = 0;
    f->end = 0;
    for (u64 i = 0; i < ftrace_func_map_count; i++) {
        if (strncmp(ftrace_func_maps[i].name, func_pattern, FTRACE_FUNC_NAME_MAX) == 0) {
            f->start = ftrace_func_maps[i].addr;
            f->end = f->start + 1;
            break;
        }
    }
    spin_unlock(&ftrace_graph.lock);
}

void ftrace_clear_filter(void)
{
    spin_lock(&ftrace_graph.lock);
    ftrace_filter_count = 0;
    memset(ftrace_filters, 0, sizeof(ftrace_filters));
    spin_unlock(&ftrace_graph.lock);
}

int ftrace_match_filter(u64 ip)
{
    if (ftrace_filter_count == 0) return 1;
    for (u32 i = 0; i < ftrace_filter_count; i++) {
        if (ip >= ftrace_filters[i].start && ip < ftrace_filters[i].end) {
            return 1;
        }
    }
    return 0;
}

static void ftrace_ring_write(ftrace_func_entry_t *entry)
{
    ftrace_ring_buffer_t *buf = &ftrace_per_cpu_buffer[0];
    spin_lock(&buf->lock);
    u64 next_head = buf->head + 1;
    if (next_head >= ftrace_ring_entries_count()) {
        next_head = 0;
    }
    if (next_head == buf->tail) {
        buf->tail++;
        if (buf->tail >= ftrace_ring_entries_count()) {
            buf->tail = 0;
        }
        buf->dropped++;
    }
    buf->entries[buf->head] = *entry;
    buf->head = next_head;
    buf->count++;
    spin_unlock(&buf->lock);
}

static const char *ftrace_lookup_name(u64 ip)
{
    for (u64 i = 0; i < ftrace_func_map_count; i++) {
        if (ftrace_func_maps[i].addr == ip) {
            return ftrace_func_maps[i].name;
        }
    }
    return "<unknown>";
}

void ftrace_record_entry(u64 ip, u64 parent_ip)
{
    if (!ftrace_enabled) return;
    if (!ftrace_match_filter(ip)) return;

    ftrace_func_entry_t entry;
    entry.ip = ip;
    entry.parent_ip = parent_ip;
    entry.timestamp = ftrace_now();
    ftrace_ring_write(&entry);
}

void ftrace_record_exit(u64 ip, u64 parent_ip)
{
    if (!ftrace_enabled) return;
    if (!ftrace_match_filter(ip)) return;

    ftrace_func_entry_t entry;
    entry.ip = ip;
    entry.parent_ip = parent_ip;
    entry.timestamp = ftrace_now();
    ftrace_ring_write(&entry);
}

void ftrace_graph_entry(u64 ip, u64 parent_ip)
{
    if (!ftrace_enabled) return;
    if (!ftrace_match_filter(ip)) return;

    spin_lock(&ftrace_graph.lock);
    if (ftrace_graph.current_depth < FTRACE_MAX_GRAPH_DEPTH) {
        ftrace_graph_entry_t *ge = &ftrace_graph.stack[ftrace_graph.current_depth];
        ge->func = ip;
        ge->call_time = ftrace_now();
        ge->depth = ftrace_graph.current_depth;
        ge->overhead = 0;
        ftrace_graph.current_depth++;
    }
    spin_unlock(&ftrace_graph.lock);

    ftrace_record_entry(ip, parent_ip);
}

void ftrace_graph_return(u64 ip)
{
    if (!ftrace_enabled) return;

    spin_lock(&ftrace_graph.lock);
    if (ftrace_graph.current_depth > 0) {
        ftrace_graph.current_depth--;
        ftrace_graph_entry_t *ge = &ftrace_graph.stack[ftrace_graph.current_depth];
        if (ge->func == ip) {
            u64 delta = ftrace_now() - ge->call_time;
        }
    }
    spin_unlock(&ftrace_graph.lock);
}

void ftrace_dump_buffer(void)
{
    ftrace_ring_buffer_t *buf = &ftrace_per_cpu_buffer[0];
    u64 idx = buf->tail;
    u64 count = 0;

    spin_lock(&buf->lock);
    while (idx != buf->head && count < 1024) {
        ftrace_func_entry_t *e = &buf->entries[idx];
        const char *name = ftrace_lookup_name(e->ip);
        const char *pname = ftrace_lookup_name(e->parent_ip);
        (void)name;
        (void)pname;
        idx++;
        if (idx >= ftrace_ring_entries_count()) {
            idx = 0;
        }
        count++;
    }
    spin_unlock(&buf->lock);
}

void ftrace_clear_buffer(void)
{
    ftrace_ring_buffer_t *buf = &ftrace_per_cpu_buffer[0];
    spin_lock(&buf->lock);
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    buf->dropped = 0;
    spin_unlock(&buf->lock);
}

static int ftrace_function_start(void)
{
    ftrace_enable();
    return 0;
}

static int ftrace_function_stop(void)
{
    ftrace_disable();
    return 0;
}

static void ftrace_function_reset(void)
{
    ftrace_disable();
    ftrace_clear_buffer();
    ftrace_clear_filter();
}

static int ftrace_graph_start(void)
{
    ftrace_enable();
    memset(&ftrace_graph, 0, sizeof(ftrace_graph));
    spin_init(&ftrace_graph.lock);
    return 0;
}

static int ftrace_graph_stop(void)
{
    ftrace_disable();
    return 0;
}

static void ftrace_graph_reset(void)
{
    ftrace_disable();
    memset(&ftrace_graph, 0, sizeof(ftrace_graph));
    spin_init(&ftrace_graph.lock);
    ftrace_clear_buffer();
}

int ftrace_set_trace(const char *tracer_name)
{
    if (!tracer_name) return -1;

    for (u32 i = 0; i < ftrace_tracer_count; i++) {
        if (strncmp(ftrace_tracers[i].name, tracer_name, 32) == 0) {
            if (ftrace_current_tracer < ftrace_tracer_count) {
                ftrace_tracers[ftrace_current_tracer].stop();
                ftrace_tracers[ftrace_current_tracer].reset();
                ftrace_tracers[ftrace_current_tracer].enabled = 0;
            }
            ftrace_current_tracer = i;
            ftrace_tracers[i].reset();
            ftrace_tracers[i].start();
            ftrace_tracers[i].enabled = 1;
            return 0;
        }
    }
    return -1;
}

const char *ftrace_get_current_trace(void)
{
    if (ftrace_current_tracer < ftrace_tracer_count) {
        return ftrace_tracers[ftrace_current_tracer].name;
    }
    return "none";
}

void __ftrace_entry(u64 ip, u64 parent_ip)
{
    if (!ftrace_enabled) return;
    if (ftrace_current_tracer < ftrace_tracer_count) {
        if (ftrace_tracers[ftrace_current_tracer].type == FTRACE_TYPE_FUNCTION) {
            ftrace_record_entry(ip, parent_ip);
        } else if (ftrace_tracers[ftrace_current_tracer].type == FTRACE_TYPE_FUNCTION_GRAPH) {
            ftrace_graph_entry(ip, parent_ip);
        }
    }
}

void __ftrace_return(u64 ip)
{
    if (!ftrace_enabled) return;
    if (ftrace_current_tracer < ftrace_tracer_count) {
        if (ftrace_tracers[ftrace_current_tracer].type == FTRACE_TYPE_FUNCTION_GRAPH) {
            ftrace_graph_return(ip);
        }
    }
}

__attribute__((constructor))
static void ftrace_register_builtin_tracers(void)
{
    memset(ftrace_tracers, 0, sizeof(ftrace_tracers));
    ftrace_tracer_count = 0;

    strncpy(ftrace_tracers[0].name, "function", 32);
    ftrace_tracers[0].type = FTRACE_TYPE_FUNCTION;
    ftrace_tracers[0].start = ftrace_function_start;
    ftrace_tracers[0].stop = ftrace_function_stop;
    ftrace_tracers[0].reset = ftrace_function_reset;
    ftrace_tracers[0].enabled = 0;

    strncpy(ftrace_tracers[1].name, "function_graph", 32);
    ftrace_tracers[1].type = FTRACE_TYPE_FUNCTION_GRAPH;
    ftrace_tracers[1].start = ftrace_graph_start;
    ftrace_tracers[1].stop = ftrace_graph_stop;
    ftrace_tracers[1].reset = ftrace_graph_reset;
    ftrace_tracers[1].enabled = 0;

    ftrace_tracer_count = 2;
}
