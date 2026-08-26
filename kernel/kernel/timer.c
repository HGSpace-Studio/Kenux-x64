

#include "timer.h"
#include <arch/spinlock.h>
#include <arch/hpet.h>
#include <arch/process.h>
#include <string.h>
#include <slab.h>

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];
extern void process_yield(void);

static timer_wheel_t tw;

/* 高精度定时器链表，按到期时间升序排列 */
static hrtimer_t* hrtimer_list = NULL;
static spinlock_t hrtimer_lock = SPINLOCK_INIT;

void timer_init(void)
{
    timer_wheel_init();
}

void timer_wheel_init(void)
{
    memset(&tw, 0, sizeof(timer_wheel_t));
    spin_init(&tw.lock);
    tw.jiffies = 0;
    tw.next_expire = (uint64_t)-1;
}

uint64_t timer_get_jiffies(void)
{
    return tw.jiffies;
}

uint64_t timer_jiffies_to_ms(uint64_t jiffies)
{
    return jiffies * TIMER_TICK_MS;
}

uint64_t timer_ms_to_jiffies(uint64_t ms)
{
    if (ms == 0) return 1;
    return (ms + TIMER_TICK_MS - 1) / TIMER_TICK_MS;
}

static void __calc_index(uint64_t expires, uint64_t now, int* level, uint64_t* index)
{
    uint64_t delta = expires - now;

    if (delta < (1ULL << (TIMER_WHEEL_BITS * 1))) {
        *level = 0;
    } else if (delta < (1ULL << (TIMER_WHEEL_BITS * 2))) {
        *level = 1;
    } else if (delta < (1ULL << (TIMER_WHEEL_BITS * 3))) {
        *level = 2;
    } else {
        *level = 3;
    }

    uint64_t shift = TIMER_WHEEL_BITS * (*level);
    *index = (expires >> shift) & TIMER_WHEEL_MASK;
}

static void __timer_enqueue(timer_entry_t* timer)
{
    int level;
    uint64_t idx;
    __calc_index(timer->expires, tw.jiffies, &level, &idx);

    timer_entry_t** list = &tw.levels[level].lists[idx];

    timer->next = NULL;
    timer->prev = *list ? (*list)->prev : NULL;

    if (*list) {
        (*list)->prev->next = timer;
        (*list)->prev = timer;
    } else {
        *list = timer;
        timer->prev = timer;
    }

    timer->state = TIMER_ACTIVE;

    if (timer->expires < tw.next_expire) {
        tw.next_expire = timer->expires;
    }
}

static void __timer_dequeue(timer_entry_t* timer)
{
    if (timer->next == timer) {

        int level;
        uint64_t idx;
        __calc_index(timer->expires, tw.jiffies, &level, &idx);
        tw.levels[level].lists[idx] = NULL;
    } else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
    }
    timer->next = timer->prev = NULL;
    timer->state = TIMER_INACTIVE;
}

static void __cascade(int level)
{
    if (level >= TIMER_WHEEL_LEVELS - 1) return;

    uint64_t idx = (tw.jiffies >> (TIMER_WHEEL_BITS * (level + 1))) & TIMER_WHEEL_MASK;
    timer_entry_t* list = tw.levels[level + 1].lists[idx];

    tw.levels[level + 1].lists[idx] = NULL;

    while (list) {
        timer_entry_t* next = list->next;
        list->next = list->prev = NULL;
        __timer_enqueue(list);
        list = next;
    }
}

static void __expire_level0(void)
{
    uint64_t idx = tw.jiffies & TIMER_WHEEL_MASK;
    timer_entry_t* list = tw.levels[0].lists[idx];
    tw.levels[0].lists[idx] = NULL;

    while (list) {
        timer_entry_t* next = list->next;
        list->next = list->prev = NULL;
        list->state = TIMER_EXPIRED;

        if (list->callback) {
            list->callback(list->arg);
        }

        if (!(list->flags & TIMER_FLAG_ONESHOT)) {
            list->expires += timer_ms_to_jiffies(1);
            __timer_enqueue(list);
        }

        list = next;
    }

    if (idx == 0) {
        __cascade(0);
    }
}

static void __hrtimer_expire(void)
{
    spin_lock(&hrtimer_lock);

    uint64_t now = hpet_get_ticks();
    hrtimer_t** pp = &hrtimer_list;

    while (*pp) {
        hrtimer_t* timer = *pp;
        if (timer->expire_ns > now) {
            break;
        }

        /* 从链表中移除 */
        *pp = timer->next;
        timer->next = NULL;
        timer->active = 0;

        /* 执行回调 */
        if (timer->callback) {
            spin_unlock(&hrtimer_lock);
            timer->callback(timer->arg);
            spin_lock(&hrtimer_lock);
        }

        /* 如果不是单次定时器，重新计算并插入 */
        if (!timer->oneshot) {
            uint64_t period = timer->expire_ns - timer->start_ns;
            timer->start_ns = now;
            timer->expire_ns = now + period;
            timer->active = 1;

            hrtimer_t** insert = &hrtimer_list;
            while (*insert && (*insert)->expire_ns < timer->expire_ns) {
                insert = &(*insert)->next;
            }
            timer->next = *insert;
            *insert = timer;
        }
    }

    spin_unlock(&hrtimer_lock);
}

void timer_tick(void)
{
    spin_lock(&tw.lock);
    tw.jiffies++;

    __expire_level0();

    tw.next_expire = (uint64_t)-1;
    for (int l = 0; l < TIMER_WHEEL_LEVELS; l++) {
        uint64_t i = (tw.jiffies >> (TIMER_WHEEL_BITS * l)) & TIMER_WHEEL_MASK;
        if (tw.levels[l].lists[i]) {

            tw.next_expire = tw.jiffies;
            break;
        }
    }

    spin_unlock(&tw.lock);

    /* 每次 tick 检查高精度定时器 */
    __hrtimer_expire();
}

int timer_add(timer_entry_t* timer, uint64_t expires_ms, timer_callback_t cb, void* arg)
{
    if (!timer || !cb) return -1;

    memset(timer, 0, sizeof(timer_entry_t));
    timer->expires = tw.jiffies + timer_ms_to_jiffies(expires_ms);
    timer->callback = cb;
    timer->arg = arg;
    timer->flags = 0;
    timer->next = timer->prev = NULL;

    spin_lock(&tw.lock);
    __timer_enqueue(timer);
    spin_unlock(&tw.lock);
    return 0;
}

int timer_add_oneshot(timer_entry_t* timer, uint64_t expires_ms, timer_callback_t cb, void* arg)
{
    int ret = timer_add(timer, expires_ms, cb, arg);
    if (ret == 0) {
        timer->flags |= TIMER_FLAG_ONESHOT;
    }
    return ret;
}

int timer_mod(timer_entry_t* timer, uint64_t expires_ms)
{
    if (!timer || timer->state != TIMER_ACTIVE) return -1;

    spin_lock(&tw.lock);
    __timer_dequeue(timer);
    timer->expires = tw.jiffies + timer_ms_to_jiffies(expires_ms);
    __timer_enqueue(timer);
    spin_unlock(&tw.lock);
    return 0;
}

int timer_del(timer_entry_t* timer)
{
    if (!timer || timer->state != TIMER_ACTIVE) return -1;

    spin_lock(&tw.lock);
    __timer_dequeue(timer);
    spin_unlock(&tw.lock);
    return 0;
}

int timer_pending(const timer_entry_t* timer)
{
    return timer && timer->state == TIMER_ACTIVE;
}

static void __msleep_wakeup(void* arg)
{
    process_t* p = (process_t*)arg;
    if (p) {
        p->state = PROCESS_READY;
    }
}

void msleep(uint64_t ms)
{
    uint64_t start = timer_get_jiffies();
    uint64_t target = start + timer_ms_to_jiffies(ms);
    while (timer_get_jiffies() < target) {
        __asm__ volatile ("hlt");
    }
}

void usleep(uint64_t us)
{
    msleep((us + 999) / 1000);
}

int hrtimer_start(hrtimer_t* timer, uint64_t ns, void (*cb)(void*), void* arg)
{
    if (!timer || !cb || ns == 0) return -1;

    memset(timer, 0, sizeof(hrtimer_t));
    timer->callback = cb;
    timer->arg = arg;
    timer->start_ns = hpet_get_ticks();
    timer->expire_ns = timer->start_ns + ns;
    timer->active = 1;
    timer->oneshot = 1;

    spin_lock(&hrtimer_lock);

    /* 按到期时间升序插入链表 */
    hrtimer_t** pp = &hrtimer_list;
    while (*pp && (*pp)->expire_ns < timer->expire_ns) {
        pp = &(*pp)->next;
    }
    timer->next = *pp;
    *pp = timer;

    spin_unlock(&hrtimer_lock);
    return 0;
}

int hrtimer_cancel(hrtimer_t* timer)
{
    if (!timer) return -1;

    spin_lock(&hrtimer_lock);

    hrtimer_t** pp = &hrtimer_list;
    while (*pp) {
        if (*pp == timer) {
            *pp = timer->next;
            timer->next = NULL;
            timer->active = 0;
            break;
        }
        pp = &(*pp)->next;
    }

    spin_unlock(&hrtimer_lock);
    return 0;
}

uint64_t hrtimer_get_resolution(void)
{
    /* HPET 典型精度约为 1 微秒（1000 纳秒） */
    return 1000;
}
