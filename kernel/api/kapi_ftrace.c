#include "kapi_ftrace.h"
#include "kapi.h"

#include <arch/memory.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

struct kapi_ftrace_entry {
    uint64_t ip;
    uint64_t parent_ip;
    uint64_t timestamp;
    uint64_t flags;
    int      pid;
    int      cpu;
    int      valid;
};

#define KAPI_FTRACE_MAX_ENTRIES  256
#define KAPI_FTRACE_MAX_FUNCTIONS 128
#define KAPI_FTRACE_MAX_GRAPH     256

typedef struct {
    char                  name[KAPI_FTRACE_NAME_LEN];
    uint64_t              ip;
    uint64_t              flags;
    kapi_ftrace_callback_t callback;
    void*                 data;
    int                   valid;
    int                   enabled;
} kapi_ftrace_func_t;

static kapi_ftrace_entry_t kapi_ftrace_entries[KAPI_FTRACE_MAX_ENTRIES];
static kapi_ftrace_func_t  kapi_ftrace_funcs[KAPI_FTRACE_MAX_FUNCTIONS];
static kapi_ftrace_graph_entry_t kapi_ftrace_graph[KAPI_FTRACE_MAX_GRAPH];
static int kapi_ftrace_entry_idx = 0;
static int kapi_ftrace_graph_idx = 0;
static int kapi_ftrace_tracing = 1;
static int kapi_ftrace_initialized = 0;

static uint64_t kapi_ftrace_get_timestamp(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

int kapi_ftrace_init(void)
{
    if (kapi_ftrace_initialized) {
        return KAPI_OK;
    }

    memset(kapi_ftrace_entries, 0, sizeof(kapi_ftrace_entries));
    memset(kapi_ftrace_funcs, 0, sizeof(kapi_ftrace_funcs));
    memset(kapi_ftrace_graph, 0, sizeof(kapi_ftrace_graph));
    kapi_ftrace_entry_idx = 0;
    kapi_ftrace_graph_idx = 0;
    kapi_ftrace_tracing = 1;
    kapi_ftrace_initialized = 1;
    return KAPI_OK;
}

int kapi_ftrace_register(const char* name, uint64_t ip, uint64_t flags,
                          kapi_ftrace_callback_t callback, void* data)
{
    if (!name) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_FTRACE_MAX_FUNCTIONS; i++) {
        if (!kapi_ftrace_funcs[i].valid) {
            strncpy(kapi_ftrace_funcs[i].name, name,
                    KAPI_FTRACE_NAME_LEN - 1);
            kapi_ftrace_funcs[i].ip = ip;
            kapi_ftrace_funcs[i].flags = flags;
            kapi_ftrace_funcs[i].callback = callback;
            kapi_ftrace_funcs[i].data = data;
            kapi_ftrace_funcs[i].valid = 1;
            kapi_ftrace_funcs[i].enabled = 1;
            return KAPI_OK;
        }
    }
    return KAPI_ERROR;
}

int kapi_ftrace_unregister(const char* name, uint64_t ip)
{
    for (int i = 0; i < KAPI_FTRACE_MAX_FUNCTIONS; i++) {
        if (kapi_ftrace_funcs[i].valid &&
            kapi_ftrace_funcs[i].ip == ip &&
            strcmp(kapi_ftrace_funcs[i].name, name) == 0) {
            kapi_ftrace_funcs[i].valid = 0;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_ftrace_set_filter(const char* filter, int enable)
{
    if (!filter) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_FTRACE_MAX_FUNCTIONS; i++) {
        if (kapi_ftrace_funcs[i].valid &&
            strstr(kapi_ftrace_funcs[i].name, filter)) {
            kapi_ftrace_funcs[i].enabled = enable;
        }
    }
    return KAPI_OK;
}

int kapi_ftrace_set_notrace(const char* filter, int enable)
{
    if (!filter) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_FTRACE_MAX_FUNCTIONS; i++) {
        if (kapi_ftrace_funcs[i].valid &&
            strstr(kapi_ftrace_funcs[i].name, filter)) {
            kapi_ftrace_funcs[i].enabled = enable ? 0 : 1;
        }
    }
    return KAPI_OK;
}

int kapi_ftrace_set_ftrace_filter(const char* func)
{
    return kapi_ftrace_set_filter(func, 1);
}

int kapi_ftrace_set_ftrace_notrace(const char* func)
{
    return kapi_ftrace_set_notrace(func, 1);
}

int kapi_ftrace_function_enabled(void)
{
    return kapi_ftrace_tracing;
}

int kapi_ftrace_tracing_on(void)
{
    kapi_ftrace_tracing = 1;
    return KAPI_OK;
}

int kapi_ftrace_tracing_off(void)
{
    kapi_ftrace_tracing = 0;
    return KAPI_OK;
}

int kapi_ftrace_trace_printk(const char* fmt, ...)
{
    if (!kapi_ftrace_tracing || !fmt) {
        return 0;
    }

    int idx = kapi_ftrace_entry_idx;
    kapi_ftrace_entry_t* entry = &kapi_ftrace_entries[idx];

    memset(entry, 0, sizeof(*entry));
    entry->timestamp = kapi_ftrace_get_timestamp();
    entry->valid = 1;

    kapi_ftrace_entry_idx = (kapi_ftrace_entry_idx + 1) % KAPI_FTRACE_MAX_ENTRIES;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    return (int)strlen(buf);
}

int kapi_ftrace_dump_trace(void)
{
    int count = 0;
    for (int i = 0; i < KAPI_FTRACE_MAX_ENTRIES; i++) {
        if (kapi_ftrace_entries[i].valid) {
            count++;
        }
    }
    return count;
}

kapi_ftrace_entry_t* kapi_ftrace_entry_get(void)
{
    int idx = (kapi_ftrace_entry_idx - 1 + KAPI_FTRACE_MAX_ENTRIES) %
              KAPI_FTRACE_MAX_ENTRIES;
    if (kapi_ftrace_entries[idx].valid) {
        return &kapi_ftrace_entries[idx];
    }
    return NULL;
}

int kapi_ftrace_entry_release(kapi_ftrace_entry_t* entry)
{
    if (!entry) {
        return KAPI_EINVAL;
    }
    entry->valid = 0;
    return KAPI_OK;
}

int kapi_ftrace_graph_entry(uint64_t func_ip, uint64_t parent_ip,
                             uint64_t depth)
{
    if (!kapi_ftrace_tracing) {
        return KAPI_OK;
    }

    int idx = kapi_ftrace_graph_idx;
    kapi_ftrace_graph_entry_t* entry = &kapi_ftrace_graph[idx];

    memset(entry, 0, sizeof(*entry));
    entry->caller_ip = parent_ip;
    entry->depth = (int)depth;
    entry->valid = 1;

    kapi_ftrace_graph_idx = (kapi_ftrace_graph_idx + 1) % KAPI_FTRACE_MAX_GRAPH;
    (void)func_ip;
    return KAPI_OK;
}

int kapi_ftrace_graph_return(kapi_ftrace_graph_entry_t* entry)
{
    if (!entry) {
        return KAPI_EINVAL;
    }

    entry->duration = kapi_ftrace_get_timestamp() - entry->duration;
    entry->hit_count++;
    return KAPI_OK;
}

int kapi_ftrace_graph_dump(void)
{
    int count = 0;
    for (int i = 0; i < KAPI_FTRACE_MAX_GRAPH; i++) {
        if (kapi_ftrace_graph[i].valid) {
            count++;
        }
    }
    return count;
}