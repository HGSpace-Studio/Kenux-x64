#include <arch/cpufreq.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

cpufreq_policy_t cpufreq_policies[CPUFREQ_MAX_CPUS];
cpufreq_driver_t *cpufreq_driver = NULL;
u32 cpufreq_num_policies = 0;

static cpufreq_governor_t *governors[CPUFREQ_MAX_GOVERNORS];
static u32 governor_count = 0;
static spinlock_t cpufreq_global_lock = SPINLOCK_INIT;

static u64 rdmsr(u32 msr)
{
    u32 lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static void wrmsr(u32 msr, u64 val)
{
    u32 lo = (u32)(val & 0xFFFFFFFF);
    u32 hi = (u32)(val >> 32);
    __asm__ volatile ("wrmsr" :: "a"(lo), "d"(hi), "c"(msr));
}

static int performance_start(cpufreq_policy_t *policy)
{
    if (!policy || !cpufreq_driver) return -1;
    u32 max_freq = policy->max_freq;
    if (cpufreq_driver->target_index) {
        for (u32 i = 0; i < policy->table_size; i++) {
            if (policy->freq_table[i].frequency <= max_freq) {
                if (i + 1 < policy->table_size &&
                    policy->freq_table[i + 1].frequency <= max_freq) continue;
                cpufreq_driver->target_index(policy, i);
                policy->cur_freq = policy->freq_table[i].frequency;
                break;
            }
        }
    }
    return 0;
}

static void performance_stop(cpufreq_policy_t *policy)
{
    (void)policy;
}

static int performance_limits(cpufreq_policy_t *policy)
{
    return performance_start(policy);
}

static int powersave_start(cpufreq_policy_t *policy)
{
    if (!policy || !cpufreq_driver) return -1;
    u32 min_freq = policy->min_freq;
    if (cpufreq_driver->target_index) {
        for (s32 i = (s32)policy->table_size - 1; i >= 0; i--) {
            if (policy->freq_table[i].frequency >= min_freq) {
                cpufreq_driver->target_index(policy, (u32)i);
                policy->cur_freq = policy->freq_table[i].frequency;
                break;
            }
        }
    }
    return 0;
}

static void powersave_stop(cpufreq_policy_t *policy)
{
    (void)policy;
}

static int powersave_limits(cpufreq_policy_t *policy)
{
    return powersave_start(policy);
}

#define ONDEMAND_UP_THRESHOLD     80
#define ONDEMAND_SAMPLING_RATE_MS 200
#define ONDEMAND_SAMPLING_DOWN_FACTOR 5

static u32 ondemand_up_threshold = ONDEMAND_UP_THRESHOLD;
static u32 ondemand_sampling_rate = ONDEMAND_SAMPLING_RATE_MS;
static u32 ondemand_sampling_down_factor = ONDEMAND_SAMPLING_DOWN_FACTOR;

static int ondemand_start(cpufreq_policy_t *policy)
{
    (void)policy;
    return 0;
}

static void ondemand_stop(cpufreq_policy_t *policy)
{
    (void)policy;
}

static int ondemand_limits(cpufreq_policy_t *policy)
{
    (void)policy;
    return 0;
}

static int conservative_up_threshold = 80;
static int conservative_down_threshold = 20;
static int conservative_step = 5;
static u32 conservative_sampling_rate = 200;

static int conservative_start(cpufreq_policy_t *policy)
{
    (void)policy;
    return 0;
}

static void conservative_stop(cpufreq_policy_t *policy)
{
    (void)policy;
}

static int conservative_limits(cpufreq_policy_t *policy)
{
    (void)policy;
    return 0;
}

static int schedutil_start(cpufreq_policy_t *policy)
{
    (void)policy;
    return 0;
}

static void schedutil_stop(cpufreq_policy_t *policy)
{
    (void)policy;
}

static int schedutil_limits(cpufreq_policy_t *policy)
{
    (void)policy;
    return 0;
}

static cpufreq_governor_t gov_performance = {
    .name = CPUFREQ_GOVERNOR_PERFORMANCE,
    .start = performance_start,
    .stop = performance_stop,
    .limits = performance_limits,
    .init = NULL,
    .exit = NULL,
    .active = 0
};

static cpufreq_governor_t gov_powersave = {
    .name = CPUFREQ_GOVERNOR_POWERSAVE,
    .start = powersave_start,
    .stop = powersave_stop,
    .limits = powersave_limits,
    .init = NULL,
    .exit = NULL,
    .active = 0
};

static cpufreq_governor_t gov_ondemand = {
    .name = CPUFREQ_GOVERNOR_ONDEMAND,
    .start = ondemand_start,
    .stop = ondemand_stop,
    .limits = ondemand_limits,
    .init = NULL,
    .exit = NULL,
    .active = 0
};

static cpufreq_governor_t gov_conservative = {
    .name = CPUFREQ_GOVERNOR_CONSERVATIVE,
    .start = conservative_start,
    .stop = conservative_stop,
    .limits = conservative_limits,
    .init = NULL,
    .exit = NULL,
    .active = 0
};

static cpufreq_governor_t gov_schedutil = {
    .name = CPUFREQ_GOVERNOR_SCHEDUTIL,
    .start = schedutil_start,
    .stop = schedutil_stop,
    .limits = schedutil_limits,
    .init = NULL,
    .exit = NULL,
    .active = 0
};

static int intel_pstate_verify(cpufreq_policy_t *policy)
{
    if (!policy) return -1;
    if (policy->min_freq > policy->max_freq) return -1;
    return 0;
}

static int intel_pstate_target_index(cpufreq_policy_t *policy, u32 index)
{
    if (!policy || index >= policy->table_size) return -1;
    u32 freq = policy->freq_table[index].frequency;

    u64 msr_val = rdmsr(MSR_IA32_PERF_CTL);
    u64 status = rdmsr(MSR_IA32_PERF_STATUS);
    u64 pstate = status & INTEL_PERF_CTL_P_STATE_MSK;

    (void)freq;
    (void)msr_val;
    (void)pstate;

    policy->cur_freq = freq;
    return 0;
}

static u32 intel_pstate_get(cpufreq_policy_t *policy)
{
    if (!policy) return 0;
    u64 status = rdmsr(MSR_IA32_PERF_STATUS);
    u64 pstate = status & INTEL_PERF_CTL_P_STATE_MSK;
    (void)pstate;
    return policy->cur_freq;
}

static int intel_pstate_init(cpufreq_policy_t *policy)
{
    if (!policy) return -1;
    u64 status = rdmsr(MSR_IA32_PERF_STATUS);
    u64 base_freq = 1000;
    u64 max_freq = 4000;
    u64 turbo_max = max_freq;
    u64 min_freq = 800;

    (void)status;
    (void)base_freq;

    policy->cpuinfo_min_freq = (u32)min_freq;
    policy->cpuinfo_max_freq = (u32)turbo_max;
    policy->min_freq = (u32)min_freq;
    policy->max_freq = (u32)max_freq;
    policy->cur_freq = (u32)max_freq;
    policy->cpuinfo_transition_latency = CPUFREQ_TRANSITION_LATENCY;

    u32 freq = (u32)max_freq;
    policy->table_size = 0;
    while (freq >= (u32)min_freq && policy->table_size < CPUFREQ_TABLE_MAX) {
        policy->freq_table[policy->table_size].frequency = freq;
        policy->freq_table[policy->table_size].driver_data = policy->table_size;
        policy->table_size++;
        if (freq <= 100) break;
        freq -= 100;
        if (freq < (u32)min_freq) freq = (u32)min_freq;
    }

    return 0;
}

static cpufreq_driver_t intel_pstate_driver = {
    .name = "intel_pstate",
    .verify = intel_pstate_verify,
    .target_index = intel_pstate_target_index,
    .fast_switch = NULL,
    .get = intel_pstate_get,
    .init = intel_pstate_init,
    .exit = NULL,
    .flags = 0
};

void cpufreq_init(void)
{
    memset(cpufreq_policies, 0, sizeof(cpufreq_policies));
    cpufreq_driver = NULL;
    cpufreq_num_policies = 0;
    governor_count = 0;
    spin_init(&cpufreq_global_lock);

    memset(governors, 0, sizeof(governors));
    governors[0] = &gov_performance;
    governors[1] = &gov_powersave;
    governors[2] = &gov_ondemand;
    governors[3] = &gov_conservative;
    governors[4] = &gov_schedutil;
    governor_count = 5;
    gov_performance.active = 0;
    gov_powersave.active = 0;
    gov_ondemand.active = 0;
    gov_conservative.active = 0;
    gov_schedutil.active = 0;

    cpufreq_register_driver(&intel_pstate_driver);

    cpufreq_policies[0].cpu = 0;
    cpufreq_policies[0].active = 0;
    spin_init(&cpufreq_policies[0].lock);

    if (cpufreq_driver && cpufreq_driver->init) {
        cpufreq_driver->init(&cpufreq_policies[0]);
    }

    strncpy(cpufreq_policies[0].governor_name, CPUFREQ_GOVERNOR_PERFORMANCE, CPUFREQ_NAME_MAX - 1);
    cpufreq_policies[0].governor_name[CPUFREQ_NAME_MAX - 1] = '\0';
    cpufreq_policies[0].active = 1;
    cpufreq_num_policies = 1;

    cpufreq_governor_t *gov = cpufreq_get_governor(cpufreq_policies[0].governor_name);
    if (gov && gov->start) {
        gov->start(&cpufreq_policies[0]);
        gov->active = 1;
    }
}

int cpufreq_register_driver(cpufreq_driver_t *driver)
{
    if (!driver) return -1;
    spin_lock(&cpufreq_global_lock);
    if (cpufreq_driver) {
        spin_unlock(&cpufreq_global_lock);
        return -1;
    }
    cpufreq_driver = driver;
    spin_unlock(&cpufreq_global_lock);
    return 0;
}

int cpufreq_unregister_driver(void)
{
    spin_lock(&cpufreq_global_lock);
    cpufreq_driver = NULL;
    spin_unlock(&cpufreq_global_lock);
    return 0;
}

cpufreq_driver_t *cpufreq_get_driver(void)
{
    return cpufreq_driver;
}

int cpufreq_register_governor(cpufreq_governor_t *governor)
{
    if (!governor) return -1;
    spin_lock(&cpufreq_global_lock);
    if (governor_count >= CPUFREQ_MAX_GOVERNORS) {
        spin_unlock(&cpufreq_global_lock);
        return -1;
    }
    for (u32 i = 0; i < governor_count; i++) {
        if (strncmp(governors[i]->name, governor->name, CPUFREQ_NAME_MAX) == 0) {
            spin_unlock(&cpufreq_global_lock);
            return -1;
        }
    }
    governors[governor_count++] = governor;
    spin_unlock(&cpufreq_global_lock);
    return 0;
}

int cpufreq_unregister_governor(const char *name)
{
    if (!name) return -1;
    spin_lock(&cpufreq_global_lock);
    for (u32 i = 0; i < governor_count; i++) {
        if (strncmp(governors[i]->name, name, CPUFREQ_NAME_MAX) == 0) {
            governors[i] = governors[governor_count - 1];
            governors[governor_count - 1] = NULL;
            governor_count--;
            spin_unlock(&cpufreq_global_lock);
            return 0;
        }
    }
    spin_unlock(&cpufreq_global_lock);
    return -1;
}

cpufreq_governor_t *cpufreq_get_governor(const char *name)
{
    if (!name) return NULL;
    for (u32 i = 0; i < governor_count; i++) {
        if (strncmp(governors[i]->name, name, CPUFREQ_NAME_MAX) == 0) {
            return governors[i];
        }
    }
    return NULL;
}

int cpufreq_update_policy(u32 cpu)
{
    if (cpu >= CPUFREQ_MAX_CPUS) return -1;
    cpufreq_policy_t *policy = &cpufreq_policies[cpu];
    if (!policy->active) return -1;

    spin_lock(&policy->lock);
    cpufreq_governor_t *gov = cpufreq_get_governor(policy->governor_name);
    if (gov && gov->limits) {
        gov->limits(policy);
    }
    spin_unlock(&policy->lock);
    return 0;
}

int cpufreq_set_policy(u32 cpu, const char *governor_name)
{
    if (cpu >= CPUFREQ_MAX_CPUS || !governor_name) return -1;
    cpufreq_policy_t *policy = &cpufreq_policies[cpu];
    if (!policy->active) return -1;

    spin_lock(&policy->lock);
    cpufreq_governor_t *old_gov = cpufreq_get_governor(policy->governor_name);
    if (old_gov && old_gov->stop) {
        old_gov->stop(policy);
        old_gov->active = 0;
    }

    cpufreq_governor_t *new_gov = cpufreq_get_governor(governor_name);
    if (!new_gov) {
        spin_unlock(&policy->lock);
        return -1;
    }

    strncpy(policy->governor_name, governor_name, CPUFREQ_NAME_MAX - 1);
    policy->governor_name[CPUFREQ_NAME_MAX - 1] = '\0';

    if (new_gov->start) {
        new_gov->start(policy);
        new_gov->active = 1;
    }
    spin_unlock(&policy->lock);
    return 0;
}

int cpufreq_set_frequency(u32 cpu, u32 freq)
{
    if (cpu >= CPUFREQ_MAX_CPUS) return -1;
    cpufreq_policy_t *policy = &cpufreq_policies[cpu];
    if (!policy->active || !cpufreq_driver) return -1;

    if (freq < policy->min_freq || freq > policy->max_freq) return -1;

    spin_lock(&policy->lock);
    for (u32 i = 0; i < policy->table_size; i++) {
        if (policy->freq_table[i].frequency == freq) {
            if (cpufreq_driver->target_index) {
                cpufreq_driver->target_index(policy, i);
                policy->cur_freq = freq;
                spin_unlock(&policy->lock);
                return 0;
            }
            break;
        }
    }
    spin_unlock(&policy->lock);
    return -1;
}

u32 cpufreq_get(u32 cpu)
{
    if (cpu >= CPUFREQ_MAX_CPUS) return 0;
    cpufreq_policy_t *policy = &cpufreq_policies[cpu];
    if (!policy->active) return 0;
    if (cpufreq_driver && cpufreq_driver->get) {
        return cpufreq_driver->get(policy);
    }
    return policy->cur_freq;
}

u32 cpufreq_quick_get(u32 cpu)
{
    if (cpu >= CPUFREQ_MAX_CPUS) return 0;
    return cpufreq_policies[cpu].cur_freq;
}

void cpufreq_notify_transition(cpufreq_policy_t *policy, u32 new_freq)
{
    if (!policy) return;
    policy->cur_freq = new_freq;
}
