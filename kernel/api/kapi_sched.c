

#include "kapi_sched.h"
#include "kapi.h"

#include <arch/process.h>
#include <timer.h>

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];
extern void process_yield(void);

void kapi_schedule(void)
{
    process_yield();
}

void kapi_set_current_state(int state)
{
    if (current_process < PROCESS_MAX) {
        uint64_t s = PROCESS_READY;
        switch (state) {
            case KAPI_TASK_RUNNING:       s = PROCESS_RUNNING; break;
            case KAPI_TASK_INTERRUPTIBLE:
            case KAPI_TASK_UNINTERRUPTIBLE: s = PROCESS_SLEEPING; break;
            case KAPI_TASK_STOPPED:       s = PROCESS_STOPPED; break;
            case KAPI_TASK_DEAD:          s = PROCESS_DEAD; break;
        }
        processes[current_process].state = s;
    }
}

void kapi_schedule_timeout(uint64_t timeout_ms)
{
    msleep(timeout_ms);
}
