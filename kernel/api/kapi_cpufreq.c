/*
 * Kenux Advanced OS Skeleton - CPU Frequency Scaling implementation
 *
 * Skeleton: per-CPU policy table + governor dispatch. Actual MSR writes
 * for P-states and ACPI _PSS/_PPC handling are TODO pending arch hooks.
 */

#include "kapi_cpufreq.h"
#include "kapi.h"

#include <string.h>

static kapi_cpufreq_policy_t kapi_cpufreq_policies[KAPI_CPUFREQ_MAX_CPUS];
static int kapi_cpufreq_initialized = 0;

int kapi_cpufreq_init(void)
{
    if (kapi_cpufreq_initialized) {
        return KAPI_CPUFREQ_OK;
    }
    memset(kapi_cpufreq_policies, 0, sizeof(kapi_cpufreq_policies));
    /* TODO: enumerate CPUs from CPU mask and probe available freqs */
    kapi_cpufreq_initialized = 1;
    return KAPI_CPUFREQ_OK;
}

void kapi_cpufreq_exit(void)
{
    memset(kapi_cpufreq_policies, 0, sizeof(kapi_cpufreq_policies));
    kapi_cpufreq_initialized = 0;
}

int kapi_cpufreq_get_policy(uint32_t cpu, kapi_cpufreq_policy_t *out)
{
    if (cpu >= KAPI_CPUFREQ_MAX_CPUS || !out) {
        return KAPI_CPUFREQ_EINVAL;
    }
    *out = kapi_cpufreq_policies[cpu];
    return KAPI_CPUFREQ_OK;
}

int kapi_cpufreq_set_policy(uint32_t cpu, const kapi_cpufreq_policy_t *policy)
{
    if (cpu >= KAPI_CPUFREQ_MAX_CPUS || !policy) {
        return KAPI_CPUFREQ_EINVAL;
    }
    kapi_cpufreq_policies[cpu] = *policy;
    /* TODO: program MSRs / ACPI _PSC */
    return KAPI_CPUFREQ_OK;
}

int kapi_cpufreq_set_freq(uint32_t cpu, uint32_t freq_khz)
{
    if (cpu >= KAPI_CPUFREQ_MAX_CPUS) {
        return KAPI_CPUFREQ_EINVAL;
    }
    kapi_cpufreq_policy_t *p = &kapi_cpufreq_policies[cpu];
    if (freq_khz < p->min_freq_khz || freq_khz > p->max_freq_khz) {
        return KAPI_CPUFREQ_EINVAL;
    }
    p->cur_freq_khz = freq_khz;
    /* TODO: transition latency + MSR write */
    return KAPI_CPUFREQ_OK;
}

int kapi_cpufreq_set_governor(uint32_t cpu, kapi_cpufreq_governor_t gov)
{
    if (cpu >= KAPI_CPUFREQ_MAX_CPUS) {
        return KAPI_CPUFREQ_EINVAL;
    }
    kapi_cpufreq_policies[cpu].governor = gov;
    return KAPI_CPUFREQ_OK;
}

int kapi_cpufreq_get_governor_name(kapi_cpufreq_governor_t gov,
                                   char *out, size_t len)
{
    const char *name = NULL;
    switch (gov) {
        case KAPI_CPUFREQ_GOVERNOR_ONDEMAND:    name = "ondemand";    break;
        case KAPI_CPUFREQ_GOVERNOR_POWERSAVE:    name = "powersave";   break;
        case KAPI_CPUFREQ_GOVERNOR_PERFORMANCE:  name = "performance"; break;
        case KAPI_CPUFREQ_GOVERNOR_SCHEDUTIL:    name = "schedutil";   break;
        case KAPI_CPUFREQ_GOVERNOR_USERSPACE:    name = "userspace";    break;
        default:
            return KAPI_CPUFREQ_EINVAL;
    }
    if (!out || len == 0) {
        return KAPI_CPUFREQ_EINVAL;
    }
    strncpy(out, name, len - 1);
    out[len - 1] = '\0';
    return KAPI_CPUFREQ_OK;
}

int kapi_cpufreq_throttle(uint32_t cpu, uint32_t temp_millideg)
{
    if (cpu >= KAPI_CPUFREQ_MAX_CPUS) {
        return KAPI_CPUFREQ_EINVAL;
    }
    kapi_cpufreq_policy_t *p = &kapi_cpufreq_policies[cpu];
    p->temperature_millideg = temp_millideg;
    /* TODO: cap frequency to a lower bin when crossing thermal trip */
    p->throttled = 1;
    return KAPI_CPUFREQ_OK;
}

int kapi_cpufreq_unthrottle(uint32_t cpu)
{
    if (cpu >= KAPI_CPUFREQ_MAX_CPUS) {
        return KAPI_CPUFREQ_EINVAL;
    }
    kapi_cpufreq_policies[cpu].throttled = 0;
    return KAPI_CPUFREQ_OK;
}

int kapi_cpufreq_tick(void)
{
    /* TODO: per-CPU governor evaluation based on load */
    for (int i = 0; i < KAPI_CPUFREQ_MAX_CPUS; i++) {
        if (kapi_cpufreq_policies[i].cur_freq_khz != 0 &&
            kapi_cpufreq_policies[i].governor ==
                KAPI_CPUFREQ_GOVERNOR_ONDEMAND) {
            /* TODO: read CPU load, adjust frequency */
        }
    }
    return KAPI_CPUFREQ_OK;
}
