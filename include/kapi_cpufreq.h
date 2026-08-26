#ifndef KAPI_CPUFREQ_H
#define KAPI_CPUFREQ_H

/*
 * Kenux Advanced OS Skeleton - CPU Frequency Scaling (cpufreq)
 *
 * Dynamic frequency / voltage scaling with pluggable governor policies
 * (ondemand, powersave, performance, schedutil). Integrated with ACPI /
 * thermal for throttling. Skeleton: API only.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_CPUFREQ_MAX_CPUS       256
#define KAPI_CPUFREQ_GOVERNOR_NAME  32

typedef enum {
    KAPI_CPUFREQ_OK             = 0,
    KAPI_CPUFREQ_EINVAL         = -1,
    KAPI_CPUFREQ_ENOMEM         = -2,
    KAPI_CPUFREQ_ENODEV         = -3,
    KAPI_CPUFREQ_EBUSY          = -4,
    KAPI_CPUFREQ_ENOTSUP        = -5
} kapi_cpufreq_err_t;

typedef enum {
    KAPI_CPUFREQ_GOVERNOR_ONDEMAND    = 0,
    KAPI_CPUFREQ_GOVERNOR_POWERSAVE   = 1,
    KAPI_CPUFREQ_GOVERNOR_PERFORMANCE = 2,
    KAPI_CPUFREQ_GOVERNOR_SCHEDUTIL   = 3,
    KAPI_CPUFREQ_GOVERNOR_USERSPACE   = 4
} kapi_cpufreq_governor_t;

typedef struct {
    uint32_t cpu;
    uint32_t min_freq_khz;
    uint32_t max_freq_khz;
    uint32_t cur_freq_khz;
    uint32_t available_freqs[16];
    uint32_t num_freqs;
    uint32_t transition_latency_ns;
    kapi_cpufreq_governor_t governor;
    uint32_t temperature_millideg;
    int      throttled;
} kapi_cpufreq_policy_t;

/* Subsystem lifecycle */
int kapi_cpufreq_init(void);
void kapi_cpufreq_exit(void);

/* Per-CPU policy */
int kapi_cpufreq_get_policy(uint32_t cpu, kapi_cpufreq_policy_t *out);
int kapi_cpufreq_set_policy(uint32_t cpu, const kapi_cpufreq_policy_t *policy);

/* Frequency / governor control */
int kapi_cpufreq_set_freq(uint32_t cpu, uint32_t freq_khz);
int kapi_cpufreq_set_governor(uint32_t cpu, kapi_cpufreq_governor_t gov);
int kapi_cpufreq_get_governor_name(kapi_cpufreq_governor_t gov,
                                   char *out, size_t len);

/* Throttling hook (called from thermal subsystem) */
int kapi_cpufreq_throttle(uint32_t cpu, uint32_t temp_millideg);
int kapi_cpufreq_unthrottle(uint32_t cpu);

/* Periodic governor tick */
int kapi_cpufreq_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_CPUFREQ_H */
