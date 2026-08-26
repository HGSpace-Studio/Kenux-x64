

#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include <arch/types.h>
#include <arch/spinlock.h>

typedef void (*timer_callback_t)(void* arg);

#define TIMER_INACTIVE    0
#define TIMER_ACTIVE      1
#define TIMER_EXPIRED     2

typedef struct timer_entry {
    struct timer_entry* next;
    struct timer_entry* prev;

    uint64_t      expires;
    timer_callback_t callback;
    void*         arg;

    uint32_t      state;
    uint32_t      flags;
} timer_entry_t;

#define TIMER_FLAG_ONESHOT     0x01

#define TIMER_WHEEL_BITS      8
#define TIMER_WHEEL_SIZE      (1 << TIMER_WHEEL_BITS)
#define TIMER_WHEEL_MASK      (TIMER_WHEEL_SIZE - 1)
#define TIMER_WHEEL_LEVELS    4
#define TIMER_WHEEL_SHIFT     TIMER_WHEEL_BITS

#define TIMER_TICK_MS         1

typedef struct {
    timer_entry_t* lists[TIMER_WHEEL_SIZE];
} timer_level_t;

typedef struct {
    timer_level_t levels[TIMER_WHEEL_LEVELS];
    uint64_t      jiffies;
    uint64_t      next_expire;
    spinlock_t    lock;
} timer_wheel_t;

typedef struct hrtimer {
    struct hrtimer* next;
    void (*callback)(void* arg);
    void*  arg;
    uint64_t start_ns;
    uint64_t expire_ns;
    int     active;
    int     oneshot;
} hrtimer_t;

void timer_init(void);
void timer_wheel_init(void);

void timer_tick(void);
uint64_t timer_get_jiffies(void);
uint64_t timer_jiffies_to_ms(uint64_t jiffies);
uint64_t timer_ms_to_jiffies(uint64_t ms);

int timer_add(timer_entry_t* timer, uint64_t expires_ms, timer_callback_t cb, void* arg);
int timer_add_oneshot(timer_entry_t* timer, uint64_t expires_ms, timer_callback_t cb, void* arg);
int timer_mod(timer_entry_t* timer, uint64_t expires_ms);
int timer_del(timer_entry_t* timer);
int timer_pending(const timer_entry_t* timer);

void msleep(uint64_t ms);
void usleep(uint64_t us);

int hrtimer_start(hrtimer_t* timer, uint64_t ns, void (*cb)(void*), void* arg);
int hrtimer_cancel(hrtimer_t* timer);
uint64_t hrtimer_get_resolution(void);

#endif
