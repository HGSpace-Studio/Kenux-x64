#ifndef _ARCH_CPUIDLE_H
#define _ARCH_CPUIDLE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define CPUIDLE_NAME_MAX        32
#define CPUIDLE_STATE_MAX       8
#define CPUIDLE_MAX_CPUS        64

/* Forward declarations */
struct cpuidle_device;

typedef struct cpuidle_state cpuidle_state_t;
typedef struct cpuidle_device cpuidle_device_t;

struct cpuidle_state {
    char name[CPUIDLE_NAME_MAX];
    char desc[64];
    u32 exit_latency;
    u32 target_residency;
    u32 power_usage;
    u32 flags;
    int (*enter)(cpuidle_device_t *dev, cpuidle_state_t *state);
    int index;
};

#define CPUIDLE_FLAG_NONE       0x0000
#define CPUIDLE_FLAG_COUPLED     0x0001
#define CPUIDLE_FLAG_TIMER_STOP 0x0002
#define CPUIDLE_FLAG_POLLING    0x0004
#define CPUIDLE_FLAG_ENABLED     0x8000

struct cpuidle_device {
    u32 cpu;
    u32 state_count;
    u32 enabled;
    u32 last_state_idx;
    u64 last_residency_ns;
    u64 total_time_ns[CPUIDLE_STATE_MAX];
    u64 usage_count[CPUIDLE_STATE_MAX];
    cpuidle_state_t states[CPUIDLE_STATE_MAX];
    spinlock_t lock;
};

typedef struct cpuidle_driver {
    char name[CPUIDLE_NAME_MAX];
    u32 state_count;
    cpuidle_state_t states[CPUIDLE_STATE_MAX];
    int (*init)(cpuidle_device_t *dev);
    void (*exit)(cpuidle_device_t *dev);
    int (*reflect)(cpuidle_device_t *dev, int state_idx);
    int registered;
} cpuidle_driver_t;

void cpuidle_init(void);
int cpuidle_register_driver(cpuidle_driver_t *driver);
int cpuidle_register_device(cpuidle_device_t *dev);
void cpuidle_unregister_device(cpuidle_device_t *dev);

int cpuidle_enter_state(cpuidle_device_t *dev, int state_idx);
int cpuidle_enter_free(cpuidle_device_t *dev);
void cpuidle_reflect(cpuidle_device_t *dev, int state_idx);

cpuidle_state_t *cpuidle_get_best_state(cpuidle_device_t *dev, u64 predicted_us);
int cpuidle_find_deepest_state(u64 latency_limit_us);

extern cpuidle_driver_t *cpuidle_curr_driver;
extern cpuidle_device_t cpuidle_devices[CPUIDLE_MAX_CPUS];

#endif