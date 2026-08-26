#include <kapi.h>
#include "memory_optimized.h"
#include "scheduler_enhanced.h"
#include "interrupt_enhanced.h"
#include "display_enhanced.h"
#include "filesystem_enhanced.h"
#include "network_enhanced.h"

#define PERF_MAX_COUNTERS 256
#define PERF_MAX_EVENTS    64
#define SAMPLE_INTERVAL_MS  1000

typedef enum {
    PERF_EVENT_CPU_CYCLES,
    PERF_EVENT_INSTRUCTIONS,
    PERF_EVENT_CACHE_REFERENCES,
    PERF_EVENT_CACHE_MISSES,
    PERF_EVENT_BRANCH_INSTRUCTIONS,
    PERF_BRANCH_MISSES,
    PERF_EVENT_BUS_CYCLES,
    PERF_EVENT_STALLED_CYCLES_FRONTEND,
    PERF_EVENT_STALLED_CYCLES_BACKEND,
    PERF_EVENT_REF_CPU_CYCLES,
    PERF_EVENT_CPU_CLOCK,
    PERF_EVENT_TASK_CLOCK,
    PERF_EVENT_PAGE_FAULTS_MIN,
    PERF_EVENT_PAGE_FAULTS_MAJ,
    PERF_EVENT_CONTEXT_SWITCHES,
    PERF_EVENT_CPU_MIGRATIONS,
    PERF_EVENT_PAGE_FAULTS,
    PERF_EVENT_MAJOR_FAULTS,
    PERF_EVENT_MINOR_FAULTS,
    PERF_EVENT_ALIGNMENT_FAULTS,
    PERF_EVENT_EMULATION_FAULTS
} perf_event_type_t;

typedef struct {
    uint64_t event_id;
    perf_event_type_t type;
    uint64_t count;
    uint64_t time_enabled;
    uint64_t time_running;
    bool enabled;
    char name[64];
    int cpu_id;
    pid_t pid;
} perf_counter_t;

typedef struct {
    uint64_t timestamp;
    float cpu_usage_percent;
    float memory_usage_percent;
    float disk_io_percent;
    float network_rx_bytes;
    float network_tx_bytes;
    uint32_t context_switches;
    uint32_t interrupts;
    uint32_t processes_running;
    uint32_t processes_blocked;
    float load_average[3];
} performance_sample_t;

typedef struct {
    perf_counter_t counters[PERF_MAX_COUNTERS];
    int counter_count;
    performance_sample_t samples[3600];
    int sample_count;
    int current_sample;
    spinlock_t lock;
    timer_t* sampling_timer;
    bool initialized;
} perf_monitor_t;

static perf_monitor_t perf_mon;

void perf_init(void)
{
    if (perf_mon.initialized) return;

    spin_init(&perf_mon.lock);
    memset(&perf_mon, sizeof(perf_monitor_t), 0);

    perf_mon.counter_count = 0;
    perf_mon.sample_count = 0;
    perf_mon.current_sample = 0;

    for (int i = 0; i < PERF_MAX_COUNTERS; i++) {
        perf_mon.counters[i].event_id = i;
        perf_mon.counters[i].enabled = false;
        perf_mon.counters[i].count = 0;
    }

    perf_mon.sampling_timer = system_create_timer(SAMPLE_INTERVAL_MS * 1000000ULL,
                                                  perf_sampling_callback, NULL,
                                                  TIMER_FLAG_PERIODIC);
    if (perf_mon.sampling_timer) {
        system_start_timer(perf_mon.sampling_timer);
    }

    perf_enable_hardware_counters();

    perf_mon.initialized = true;
}

int perf_alloc_counter(perf_event_type_t type, const char* name, int cpu_id, pid_t pid)
{
    if (!perf_mon.initialized) return -EINVAL;

    spin_lock(&perf_mon.lock);

    if (perf_mon.counter_count >= PERF_MAX_COUNTERS) {
        spin_unlock(&perf_mon.lock);
        return -ENOSPC;
    }

    int idx = perf_mon.counter_count++;
    perf_counter_t* counter = &perf_mon.counters[idx];

    counter->type = type;
    counter->enabled = true;
    counter->count = 0;
    counter->time_enabled = 0;
    counter->time_running = 0;
    counter->cpu_id = cpu_id;
    counter->pid = pid;

    if (name) {
        strncpy(counter->name, name, 63);
        counter->name[63] = '\0';
    } else {
        snprintf(counter->name, 64, "counter_%d", idx);
    }

    perf_program_hw_counter(idx, type);

    spin_unlock(&perf_mon.lock);
    return idx;
}

int perf_free_counter(int counter_id)
{
    if (!perf_mon.initialized || counter_id < 0 || counter_id >= PERF_MAX_COUNTERS) {
        return -EINVAL;
    }

    spin_lock(&perf_mon.lock);

    perf_counter_t* counter = &perf_mon.counters[counter_id];
    if (counter->enabled) {
        perf_disable_hw_counter(counter_id);
        counter->enabled = false;
        counter->count = 0;
    }

    spin_unlock(&perf_mon.lock);
    return 0;
}

int perf_reset_counter(int counter_id)
{
    if (!perf_mon.initialized || counter_id < 0 || counter_id >= PERF_MAX_COUNTERS) {
        return -EINVAL;
    }

    spin_lock(&perf_mon.lock);

    perf_counter_t* counter = &perf_mon.counters[counter_id];
    counter->count = 0;
    counter->time_enabled = 0;
    counter->time_running = 0;

    perf_read_and_clear_hw_counter(counter_id);

    spin_unlock(&perf_mon.lock);
    return 0;
}

int perf_read_counter(int counter_id, uint64_t* value)
{
    if (!perf_mon.initialized || !value || counter_id < 0 ||
        counter_id >= PERF_MAX_COUNTERS) {
        return -EINVAL;
    }

    spin_lock(&perf_mon.lock);

    perf_counter_t* counter = &perf_mon.counters[counter_id];
    if (!counter->enabled) {
        spin_unlock(&perf_mon.lock);
        return -ENOENT;
    }

    uint64 hw_value;
    perf_read_hw_counter(counter_id, &hw_value);
    counter->count += hw_value;
    *value = counter->count;

    spin_unlock(&perf_mon.lock);
    return 0;
}

int perf_start_counter(int counter_id)
{
    if (!perf_mon.initialized || counter_id < 0 || counter_id >= PERF_MAX_COUNTERS) {
        return -EINVAL;
    }

    spin_lock(&perf_mon.lock);

    perf_counter_t* counter = &perf_mon.counters[counter_id];
    if (counter->enabled) {
        perf_enable_hw_counter(counter_id);
    }

    spin_unlock(&perf_mon.lock);
    return 0;
}

int perf_stop_counter(int counter_id)
{
    if (!perf_mon.initialized || counter_id < 0 || counter_id >= PERF_MAX_COUNTERS) {
        return -EINVAL;
    }

    spin_lock(&perf_mon.lock);

    perf_counter_t* counter = &perf_mon.counters[counter_id];
    if (counter->enabled) {
        uint64 hw_value;
        perf_read_hw_counter(counter_id, &hw_value);
        counter->count += hw_value;
        perf_disable_hw_counter(counter_id);
    }

    spin_unlock(&perf_mon.lock);
    return 0;
}

void perf_sampling_callback(timer_t* timer, void* data)
{
    if (!perf_mon.initialized) return;

    performance_sample_t sample;
    memset(&sample, 0, sizeof(sample));

    sample.timestamp = get_current_time_ns();

    scheduler_stats_t* sched_stats = get_scheduler_statistics();
    if (sched_stats) {
        sample.cpu_usage_percent = sched_stats->cpu_usage_percent;
        sample.context_switches = sched_stats->context_switches;
        sample.processes_running = sched_stats->running_tasks;
        sample.processes_blocked = sched_stats->blocked_tasks;
        sample.load_average[0] = sched_stats->load_avg_1min;
        sample.load_average[1] = sched_stats->load_avg_5min;
        sample.load_average[2] = sched_stats->load_avg_15min;
    }

    memory_stats_t* mem_stats = get_memory_statistics();
    if (mem_stats) {
        uint64 total_mem = mem_stats->total_memory;
        uint64 used_mem = mem_stats->used_memory;
        if (total_mem > 0) {
            sample.memory_usage_percent = (float)(used_mem * 100.0 / total_mem);
        }
    }

    filesystem_stats_t* fs_stats = get_filesystem_statistics();
    if (fs_stats) {
        sample.disk_io_percent = fs_stats->io_utilization;
    }

    network_stats_t* net_stats = get_network_statistics();
    if (net_stats) {
        sample.network_rx_bytes = net_stats->bytes_received;
        sample.network_tx_bytes = net_stats->bytes_sent;
    }

    interrupt_stats_t* irq_stats = get_interrupt_statistics(0);
    if (irq_stats) {
        sample.interrupts = irq_stats->total_interrupts;
    }

    spin_lock(&perf_mon.lock);

    int idx = perf_mon.current_sample;
    perf_mon.samples[idx] = sample;
    perf_mon.current_sample = (idx + 1) % 3600;
    if (perf_mon.sample_count < 3600) {
        perf_mon.sample_count++;
    }

    spin_unlock(&perf_mon.lock);
}

performance_sample_t* perf_get_latest_sample(void)
{
    if (!perf_mon.initialized || perf_mon.sample_count == 0) return NULL;

    spin_lock(&perf_mon.lock);

    int idx = (perf_mon.current_sample - 1 + 3600) % 3600;
    performance_sample_t* sample = &perf_mon.samples[idx];

    spin_unlock(&perf_mon.lock);
    return sample;
}

performance_sample_t* perf_get_sample_history(int* count)
{
    if (!perf_mon.initialized) {
        *count = 0;
        return NULL;
    }

    spin_lock(&perf_mon.lock);
    *count = perf_mon.sample_count;
    spin_unlock(&perf_mon.lock);

    return perf_mon.samples;
}

float perf_get_cpu_usage_avg(int seconds)
{
    if (!perf_mon.initialized || perf_mon.sample_count == 0) return 0.0f;

    int samples_needed = seconds;
    if (samples_needed > perf_mon.sample_count) {
        samples_needed = perf_mon.sample_count;
    }

    float sum = 0.0f;
    int count = 0;

    spin_lock(&perf_mon.lock);

    for (int i = 0; i < samples_needed; i++) {
        int idx = (perf_mon.current_sample - 1 - i + 3600) % 3600;
        sum += perf_mon.samples[idx].cpu_usage_percent;
        count++;
    }

    spin_unlock(&perf_mon.lock);

    return (count > 0) ? (sum / count) : 0.0f;
}

float perf_get_memory_usage_avg(int seconds)
{
    if (!perf_mon.initialized || perf_mon.sample_count == 0) return 0.0f;

    int samples_needed = seconds;
    if (samples_needed > perf_mon.sample_count) {
        samples_needed = perf_mon.sample_count;
    }

    float sum = 0.0f;
    int count = 0;

    spin_lock(&perf_mon.lock);

    for (int i = 0; i < samples_needed; i++) {
        int idx = (perf_mon.current_sample - 1 - i + 3600) % 3600;
        sum += perf_mon.samples[idx].memory_usage_percent;
        count++;
    }

    spin_unlock(&perf_mon.lock);

    return (count > 0) ? (sum / count) : 0.0f;
}

int perf_generate_report(char* buffer, size_t size)
{
    if (!buffer || size == 0 || !perf_mon.initialized) return -EINVAL;

    performance_sample_t* latest = perf_get_latest_sample();
    if (!latest) return -ENOENT;

    memory_stats_t* mem_stats = get_memory_statistics();
    scheduler_stats_t* sched_stats = get_scheduler_statistics();

    int written = snprintf(buffer, size,
        "=== Kenux Performance Report ===\n"
        "Timestamp: %lu ns\n\n"
        "CPU Usage: %.2f%%\n"
        "Memory Usage: %.2f%%\n"
        "Disk I/O: %.2f%%\n"
        "Network RX: %.2f bytes\n"
        "Network TX: %.2f bytes\n\n"
        "Context Switches: %u\n"
        "Interrupts: %u\n"
        "Running Processes: %u\n"
        "Blocked Processes: %u\n\n"
        "Load Average (1/5/15 min): %.2f / %.2f / %.2f\n\n",
        latest->timestamp,
        latest->cpu_usage_percent,
        latest->memory_usage_percent,
        latest->disk_io_percent,
        latest->network_rx_bytes,
        latest->network_tx_bytes,
        latest->context_switches,
        latest->interrupts,
        latest->processes_running,
        latest->processes_blocked,
        latest->load_average[0],
        latest->load_average[1],
        latest->load_average[2]);

    if (mem_stats && written > 0 && (size_t)written < size) {
        written += snprintf(buffer + written, size - written,
            "\n=== Memory Details ===\n"
            "Total Memory: %lu bytes (%.2f MB)\n"
            "Used Memory: %lu bytes (%.2f MB)\n"
            "Free Memory: %lu bytes (%.2f MB)\n"
            "Cached Memory: %lu bytes (%.2f MB)\n"
            "Slab Allocations: %lu\n"
            "Slab Hits: %lu\n"
            "Slab Misses: %lu\n"
            "Fragmentation: %.2f%%\n\n",
            mem_stats->total_memory, mem_stats->total_memory / (1024.0 * 1024.0),
            mem_stats->used_memory, mem_stats->used_memory / (1024.0 * 1024.0),
            mem_stats->free_memory, mem_stats->free_memory / (1024.0 * 1024.0),
            mem_stats->cached_memory, mem_stats->cached_memory / (1024.0 * 1024.0),
            mem_stats->slab_allocs,
            mem_stats->slab_hits,
            mem_stats->slab_misses,
            mem_stats->fragmentation_percent);
    }

    if (sched_stats && written > 0 && (size_t)written < size) {
        written += snprintf(buffer + written, size - written,
            "\n=== Scheduler Details ===\n"
            "Total Schedule Calls: %lu\n"
            "Idle Time: %.2f%%\n"
            "Average Latency: %lu ns\n"
            "Max Latency: %lu ns\n"
            "Migrations: %lu\n"
            "Starvations: %lu\n\n",
            sched_stats->schedule_calls,
            sched_stats->idle_percent,
            sched_stats->avg_latency_ns,
            sched_stats->max_latency_ns,
            sched_stats->migrations,
            sched_stats->starvations);
    }

    spin_lock(&perf_mon.lock);

    if (written > 0 && (size_t)written < size) {
        written += snprintf(buffer + written, size - written,
            "\n=== Performance Counters ===\n");
        for (int i = 0; i < perf_mon.counter_count; i++) {
            perf_counter_t* counter = &perf_mon.counters[i];
            if (counter->enabled) {
                written += snprintf(buffer + written, size - written,
                    "%-32s: %lu\n", counter->name, counter->count);
                if ((size_t)written >= size) break;
            }
        }
    }

    spin_unlock(&perf_mon.lock);

    return (written > 0) ? written : -ENOMEM;
}

void perf_enable_hardware_counters(void)
{
    uint64 msr_value;

    rdmsr(IA32_PERF_GLOBAL_CTRL, &msr_value);
    msr_value |= (1ULL << 32) | (1ULL << 33);
    wrmsr(IA32_PERF_GLOBAL_CTRL, msr_value);

    wrmsr(IA32_PERF_FIXED_CTR_CTRL, 0x0B);
}

void perf_disable_hardware_counters(void)
{
    wrmsr(IA32_PERF_GLOBAL_CTRL, 0);
}

void perf_program_hw_counter(int idx, perf_event_type_t type)
{
    if (idx < 0 || idx >= 8) return;

    uint64 event_select = 0;
    uint64 umask = 0;

    switch (type) {
        case PERF_EVENT_CPU_CYCLES:
            event_select = 0x3C;
            umask = 0x00;
            break;
        case PERF_EVENT_INSTRUCTIONS:
            event_select = 0xC0;
            umask = 0x00;
            break;
        case PERF_EVENT_CACHE_REFERENCES:
            event_select = 0x24;
            ummask = 0xFF;
            break;
        case PERF_EVENT_CACHE_MISSES:
            event_select = 0x23;
            umask = 0x01;
            break;
        case PERF_EVENT_BRANCH_INSTRUCTIONS:
            event_select = 0xC4;
            umask = 0x00;
            break;
        case PERF_EVENT_BRANCH_MISSES:
            event_select = 0xC5;
            umask = 0x00;
            break;
        default:
            return;
    }

    uint64 config = (umask << 8) | event_select;
    config |= (1ULL << 22) | (1ULL << 16);

    wrmsr(IA32_PERFEVTSEL0 + idx, config);
    wrmsr(IA32_PMC0 + idx, 0);
}

void perf_enable_hw_counter(int idx)
{
    if (idx < 0 || idx >= 8) return;

    uint64 ctrl;
    rdmsr(IA32_PERF_GLOBAL_CTRL, &ctrl);
    ctrl |= (1ULL << idx);
    wrmsr(IA32_PERF_GLOBAL_CTRL, ctrl);
}

void perf_disable_hw_counter(int idx)
{
    if (idx < 0 || idx >= 8) return;

    uint64 ctrl;
    rdmsr(IA32_PERF_GLOBAL_CTRL, &ctrl);
    ctrl &= ~(1ULL << idx);
    wrmsr(IA32_PERF_GLOBAL_CTRL, ctrl);
}

void perf_read_hw_counter(int idx, uint64_t* value)
{
    if (idx < 0 || idx >= 8 || !value) return;
    rdmsr(IA32_PMC0 + idx, value);
}

void perf_read_and_clear_hw_counter(int idx)
{
    if (idx < 0 || idx >= 8) return;
    wrmsr(IA32_PMC0 + idx, 0);
}

void perf_cleanup(void)
{
    if (!perf_mon.initialized) return;

    if (perf_mon.sampling_timer) {
        system_stop_timer(perf_mon.sampling_timer);
        system_destroy_timer(perf_mon.sampling_timer);
        perf_mon.sampling_timer = NULL;
    }

    for (int i = 0; i < PERF_MAX_COUNTERS; i++) {
        if (perf_mon.counters[i].enabled) {
            perf_free_counter(i);
        }
    }

    perf_disable_hardware_counters();

    perf_mon.initialized = false;
}

void perf_auto_tune(void)
{
    if (!perf_mon.initialized || perf_mon.sample_count < 10) return;

    performance_sample_t* latest = perf_get_latest_sample();
    if (!latest) return;

    if (latest->memory_usage_percent > 85.0f) {
        memory_compact();
        memory_defrag();
    }

    if (latest->memory_usage_percent > 92.0f) {
        buddy_zone_t* zone = buddy_get_main_zone();
        if (zone) {
            buddy_defragment(zone);
        }
    }

    if (latest->context_switches > 10000) {
        extern uint64_t sysctl_sched_min_granularity;
        if (sysctl_sched_min_granularity < 3000000ULL) {
            sysctl_sched_min_granularity += 250000ULL;
        }
    }

    if (latest->cpu_usage_percent < 30.0f) {
        extern uint64_t sysctl_sched_min_granularity;
        if (sysctl_sched_min_granularity > 750000ULL) {
            sysctl_sched_min_granularity -= 250000ULL;
        }
    }

    if (latest->processes_blocked > latest->processes_running * 2) {
        for (int i = 0; i < PROCESS_MAX; i++) {
            if (processes[i].state == PROCESS_WAITING) {
                process_wakeup(i);
                break;
            }
        }
    }
}

typedef struct {
    uint64_t last_check_ns;
    uint64_t check_interval_ns;
    int tune_count;
    uint64_t total_tune_time_ns;
} auto_tune_state_t;

static auto_tune_state_t tune_state = {0, 5000000000ULL, 0, 0};

void perf_auto_tune_tick(void)
{
    uint64_t now = get_current_time_ns();
    if (now - tune_state.last_check_ns < tune_state.check_interval_ns) return;

    uint64_t start = now;
    perf_auto_tune();
    uint64_t elapsed = get_current_time_ns() - start;

    tune_state.last_check_ns = now;
    tune_state.tune_count++;
    tune_state.total_tune_time_ns += elapsed;
}