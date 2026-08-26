#ifndef _ARCH_CPUFREQ_H
#define _ARCH_CPUFREQ_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define CPUFREQ_NAME_MAX           32
#define CPUFREQ_MAX_CPUS           64
#define CPUFREQ_MAX_GOVERNORS       8
#define CPUFREQ_TABLE_MAX          32
#define CPUFREQ_TRANSITION_LATENCY 10000

#define MSR_IA32_PERF_STATUS       0x198
#define MSR_IA32_PERF_CTL          0x199
#define INTEL_PERF_CTL_P_STATE_MSK  0xFFFF

#define CPUFREQ_GOVERNOR_PERFORMANCE "performance"
#define CPUFREQ_GOVERNOR_POWERSAVE   "powersave"
#define CPUFREQ_GOVERNOR_ONDEMAND    "ondemand"
#define CPUFREQ_GOVERNOR_CONSERVATIVE "conservative"
#define CPUFREQ_GOVERNOR_SCHEDUTIL   "schedutil"

typedef struct cpufreq_freq_table {
    u32 frequency;
    u32 driver_data;
} cpufreq_freq_table_t;

typedef struct cpufreq_policy {
    u32 cpu;
    u32 min_freq;
    u32 max_freq;
    u32 cur_freq;
    u32 cpuinfo_min_freq;
    u32 cpuinfo_max_freq;
    u32 cpuinfo_transition_latency;
    u64 related_cpus;
    char governor_name[CPUFREQ_NAME_MAX];
    void *governor_data;
    cpufreq_freq_table_t freq_table[CPUFREQ_TABLE_MAX];
    u32 table_size;
    spinlock_t lock;
    int active;
} cpufreq_policy_t;

typedef struct cpufreq_governor {
    char name[CPUFREQ_NAME_MAX];
    int (*start)(cpufreq_policy_t *policy);
    void (*stop)(cpufreq_policy_t *policy);
    int (*limits)(cpufreq_policy_t *policy);
    int (*init)(cpufreq_policy_t *policy);
    void (*exit)(cpufreq_policy_t *policy);
    int active;
} cpufreq_governor_t;

typedef struct cpufreq_driver {
    char name[CPUFREQ_NAME_MAX];
    int (*verify)(cpufreq_policy_t *policy);
    int (*target_index)(cpufreq_policy_t *policy, u32 index);
    int (*fast_switch)(cpufreq_policy_t *policy, u32 target_freq);
    u32 (*get)(cpufreq_policy_t *policy);
    int (*init)(cpufreq_policy_t *policy);
    void (*exit)(cpufreq_policy_t *policy);
    u32 flags;
} cpufreq_driver_t;

void cpufreq_init(void);
int cpufreq_register_driver(cpufreq_driver_t *driver);
int cpufreq_unregister_driver(void);
cpufreq_driver_t *cpufreq_get_driver(void);

int cpufreq_register_governor(cpufreq_governor_t *governor);
int cpufreq_unregister_governor(const char *name);
cpufreq_governor_t *cpufreq_get_governor(const char *name);

int cpufreq_update_policy(u32 cpu);
int cpufreq_set_policy(u32 cpu, const char *governor_name);
int cpufreq_set_frequency(u32 cpu, u32 freq);

u32 cpufreq_get(u32 cpu);
u32 cpufreq_quick_get(u32 cpu);
void cpufreq_notify_transition(cpufreq_policy_t *policy, u32 new_freq);

extern cpufreq_policy_t cpufreq_policies[CPUFREQ_MAX_CPUS];
extern cpufreq_driver_t *cpufreq_driver;
extern u32 cpufreq_num_policies;

#endif
