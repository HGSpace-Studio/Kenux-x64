

#ifndef KAPI_SCHED_H
#define KAPI_SCHED_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_TASK_RUNNING         0
#define KAPI_TASK_INTERRUPTIBLE   1
#define KAPI_TASK_UNINTERRUPTIBLE 2
#define KAPI_TASK_STOPPED         4
#define KAPI_TASK_TRACED          8
#define KAPI_TASK_DEAD            16

void kapi_schedule(void);

void kapi_set_current_state(int state);

void kapi_schedule_timeout(uint64_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
