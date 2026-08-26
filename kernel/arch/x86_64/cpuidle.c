#include <arch/cpuidle.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

cpuidle_driver_t *cpuidle_curr_driver = NULL;
cpuidle_device_t cpuidle_devices[CPUIDLE_MAX_CPUS];
static u32 cpuidle_num_devices = 0;
static spinlock_t cpuidle_lock = SPINLOCK_INIT;

static inline u64 read_tsc_ns(void)
{
    u32 lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static int poll_idle_enter(cpuidle_device_t *dev, cpuidle_state_t *state)
{
    (void)state;
    (void)dev;
    __asm__ volatile ("pause");
    return 0;
}

static int mwait_c1_enter(cpuidle_device_t *dev, cpuidle_state_t *state)
{
    (void)state;
    (void)dev;
    __asm__ volatile ("monitor" :: "a"(0), "c"(0), "d"(0));
    __asm__ volatile ("mwait" :: "a"(0x10), "c"(0x00));
    return 0;
}

static int mwait_c1e_enter(cpuidle_device_t *dev, cpuidle_state_t *state)
{
    (void)state;
    (void)dev;
    __asm__ volatile ("monitor" :: "a"(0), "c"(0), "d"(0));
    __asm__ volatile ("mwait" :: "a"(0x20), "c"(0x00));
    return 0;
}

static int mwait_c6_enter(cpuidle_device_t *dev, cpuidle_state_t *state)
{
    (void)state;
    (void)dev;
    __asm__ volatile ("monitor" :: "a"(0), "c"(0), "d"(0));
    __asm__ volatile ("mwait" :: "a"(0x40), "c"(0x20));
    return 0;
}

static int mwait_c7_enter(cpuidle_device_t *dev, cpuidle_state_t *state)
{
    (void)state;
    (void)dev;
    __asm__ volatile ("monitor" :: "a"(0), "c"(0), "d"(0));
    __asm__ volatile ("mwait" :: "a"(0x60), "c"(0x30));
    return 0;
}

static int mwait_c8_enter(cpuidle_device_t *dev, cpuidle_state_t *state)
{
    (void)state;
    (void)dev;
    __asm__ volatile ("monitor" :: "a"(0), "c"(0), "d"(0));
    __asm__ volatile ("mwait" :: "a"(0x80), "c"(0x40));
    return 0;
}

static cpuidle_driver_t intel_idle_driver;

static int intel_idle_driver_init(cpuidle_device_t *dev)
{
    if (!dev || !cpuidle_curr_driver) return -1;

    memset(dev->states, 0, sizeof(dev->states));
    dev->state_count = 0;
    dev->enabled = 1;
    dev->last_state_idx = 0;
    dev->last_residency_ns = 0;
    memset(dev->total_time_ns, 0, sizeof(dev->total_time_ns));
    memset(dev->usage_count, 0, sizeof(dev->usage_count));

    for (u32 i = 0; i < cpuidle_curr_driver->state_count && i < CPUIDLE_STATE_MAX; i++) {
        memcpy(&dev->states[i], &cpuidle_curr_driver->states[i], sizeof(cpuidle_state_t));
        dev->states[i].index = (int)i;
        dev->states[i].flags |= CPUIDLE_FLAG_ENABLED;
        dev->state_count++;
    }

    spin_init(&dev->lock);
    return 0;
}

static int intel_idle_driver_reflect(cpuidle_device_t *dev, int state_idx)
{
    if (!dev || state_idx < 0 || state_idx >= (int)dev->state_count) return -1;
    dev->last_state_idx = (u32)state_idx;
    return 0;
}

static cpuidle_state_t intel_idle_states[] = {
    {
        .name = "POLL",
        .desc = "CPUIDLE CORE POLL",
        .exit_latency = 0,
        .target_residency = 0,
        .power_usage = 0,
        .flags = CPUIDLE_FLAG_POLLING,
        .enter = poll_idle_enter,
        .index = 0
    },
    {
        .name = "C1",
        .desc = "MWAIT 0x00",
        .exit_latency = 1,
        .target_residency = 2,
        .power_usage = 1,
        .flags = CPUIDLE_FLAG_NONE,
        .enter = mwait_c1_enter,
        .index = 1
    },
    {
        .name = "C1E",
        .desc = "MWAIT 0x01",
        .exit_latency = 10,
        .target_residency = 20,
        .power_usage = 1,
        .flags = CPUIDLE_FLAG_NONE,
        .enter = mwait_c1e_enter,
        .index = 2
    },
    {
        .name = "C6",
        .desc = "MWAIT 0x20",
        .exit_latency = 80,
        .target_residency = 200,
        .power_usage = 1,
        .flags = CPUIDLE_FLAG_TIMER_STOP,
        .enter = mwait_c6_enter,
        .index = 3
    },
    {
        .name = "C7",
        .desc = "MWAIT 0x30",
        .exit_latency = 120,
        .target_residency = 400,
        .power_usage = 1,
        .flags = CPUIDLE_FLAG_TIMER_STOP,
        .enter = mwait_c7_enter,
        .index = 4
    },
    {
        .name = "C8",
        .desc = "MWAIT 0x40",
        .exit_latency = 200,
        .target_residency = 600,
        .power_usage = 1,
        .flags = CPUIDLE_FLAG_TIMER_STOP,
        .enter = mwait_c8_enter,
        .index = 5
    }
};

static cpuidle_driver_t intel_idle_driver = {
    .name = "intel_idle",
    .state_count = 6,
    .states = {
        [0] = {
            .name = "POLL",
            .desc = "CPUIDLE CORE POLL",
            .exit_latency = 0,
            .target_residency = 0,
            .power_usage = 0,
            .flags = CPUIDLE_FLAG_POLLING,
            .enter = poll_idle_enter,
            .index = 0
        },
        [1] = {
            .name = "C1",
            .desc = "MWAIT 0x00",
            .exit_latency = 1,
            .target_residency = 2,
            .power_usage = 1,
            .flags = CPUIDLE_FLAG_NONE,
            .enter = mwait_c1_enter,
            .index = 1
        },
        [2] = {
            .name = "C1E",
            .desc = "MWAIT 0x01",
            .exit_latency = 10,
            .target_residency = 20,
            .power_usage = 1,
            .flags = CPUIDLE_FLAG_NONE,
            .enter = mwait_c1e_enter,
            .index = 2
        },
        [3] = {
            .name = "C6",
            .desc = "MWAIT 0x20",
            .exit_latency = 80,
            .target_residency = 200,
            .power_usage = 1,
            .flags = CPUIDLE_FLAG_TIMER_STOP,
            .enter = mwait_c6_enter,
            .index = 3
        },
        [4] = {
            .name = "C7",
            .desc = "MWAIT 0x30",
            .exit_latency = 120,
            .target_residency = 400,
            .power_usage = 1,
            .flags = CPUIDLE_FLAG_TIMER_STOP,
            .enter = mwait_c7_enter,
            .index = 4
        },
        [5] = {
            .name = "C8",
            .desc = "MWAIT 0x40",
            .exit_latency = 200,
            .target_residency = 600,
            .power_usage = 1,
            .flags = CPUIDLE_FLAG_TIMER_STOP,
            .enter = mwait_c8_enter,
            .index = 5
        }
    },
    .init = intel_idle_driver_init,
    .exit = NULL,
    .reflect = intel_idle_driver_reflect,
    .registered = 0
};

void cpuidle_init(void)
{
    memset(cpuidle_devices, 0, sizeof(cpuidle_devices));
    cpuidle_curr_driver = NULL;
    cpuidle_num_devices = 0;
    spin_init(&cpuidle_lock);

    cpuidle_register_driver(&intel_idle_driver);

    cpuidle_devices[0].cpu = 0;
    cpuidle_register_device(&cpuidle_devices[0]);
}

int cpuidle_register_driver(cpuidle_driver_t *driver)
{
    if (!driver) return -1;
    spin_lock(&cpuidle_lock);
    if (cpuidle_curr_driver) {
        spin_unlock(&cpuidle_lock);
        return -1;
    }
    cpuidle_curr_driver = driver;
    driver->registered = 1;

    memcpy(driver->states, intel_idle_states, sizeof(intel_idle_states));
    driver->state_count = 6;

    spin_unlock(&cpuidle_lock);
    return 0;
}

int cpuidle_register_device(cpuidle_device_t *dev)
{
    if (!dev) return -1;
    if (!cpuidle_curr_driver || !cpuidle_curr_driver->init) return -1;

    spin_lock(&cpuidle_lock);
    if (cpuidle_num_devices >= CPUIDLE_MAX_CPUS) {
        spin_unlock(&cpuidle_lock);
        return -1;
    }

    cpuidle_curr_driver->init(dev);
    dev->enabled = 1;
    cpuidle_num_devices++;
    spin_unlock(&cpuidle_lock);
    return 0;
}

void cpuidle_unregister_device(cpuidle_device_t *dev)
{
    if (!dev) return;
    spin_lock(&dev->lock);
    dev->enabled = 0;
    dev->state_count = 0;
    spin_unlock(&dev->lock);
}

int cpuidle_enter_state(cpuidle_device_t *dev, int state_idx)
{
    if (!dev || state_idx < 0 || state_idx >= (int)dev->state_count) return -1;

    cpuidle_state_t *state = &dev->states[state_idx];
    if (!(state->flags & CPUIDLE_FLAG_ENABLED)) return -1;

    u64 t0 = read_tsc_ns();
    int ret = 0;

    if (state->enter) {
        ret = state->enter(dev, state);
    }

    u64 t1 = read_tsc_ns();
    u64 residency = t1 - t0;

    spin_lock(&dev->lock);
    dev->last_residency_ns = residency;
    dev->total_time_ns[state_idx] += residency;
    dev->usage_count[state_idx]++;
    spin_unlock(&dev->lock);

    if (cpuidle_curr_driver && cpuidle_curr_driver->reflect) {
        cpuidle_curr_driver->reflect(dev, state_idx);
    }

    return ret;
}

cpuidle_state_t *cpuidle_get_best_state(cpuidle_device_t *dev, u64 predicted_us)
{
    if (!dev || dev->state_count == 0) return NULL;

    cpuidle_state_t *best = NULL;
    for (u32 i = 0; i < dev->state_count; i++) {
        cpuidle_state_t *state = &dev->states[i];
        if (!(state->flags & CPUIDLE_FLAG_ENABLED)) continue;
        if (state->target_residency <= predicted_us) {
            if (!best || state->exit_latency > best->exit_latency) {
                best = state;
            }
        }
    }

    if (!best && dev->state_count > 0) {
        best = &dev->states[0];
    }

    return best;
}

int cpuidle_find_deepest_state(u64 latency_limit_us)
{
    if (!cpuidle_curr_driver) return 0;

    int deepest = 0;
    for (u32 i = 0; i < cpuidle_curr_driver->state_count; i++) {
        cpuidle_state_t *state = &cpuidle_curr_driver->states[i];
        if (!(state->flags & CPUIDLE_FLAG_ENABLED)) continue;
        if (state->exit_latency <= latency_limit_us) {
            deepest = (int)i;
        }
    }
    return deepest;
}

int cpuidle_enter_free(cpuidle_device_t *dev)
{
    if (!dev || dev->state_count == 0) return -1;

    cpuidle_state_t *best = cpuidle_get_best_state(dev, 100);
    if (!best) return -1;

    return cpuidle_enter_state(dev, best->index);
}

void cpuidle_reflect(cpuidle_device_t *dev, int state_idx)
{
    if (!dev || state_idx < 0) return;
    if (cpuidle_curr_driver && cpuidle_curr_driver->reflect) {
        cpuidle_curr_driver->reflect(dev, state_idx);
    }
}
