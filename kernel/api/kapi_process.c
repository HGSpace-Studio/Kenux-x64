

#include "kapi_process.h"
#include "kapi.h"

#include <arch/process.h>
#include <arch/ipc.h>
#include <arch/memory.h>
#include <timer.h>
#include <string.h>

/* kernel/process.h 中的函数前置声明，避免与 arch/process.h 中的 process_exit 冲突 */
extern int thread_create(void* entry, void* arg);
extern void thread_yield(void);

extern process_t processes[PROCESS_MAX];
extern uint64_t process_count;

struct kapi_process {
    int pid;
};

struct kapi_thread {
    int tid;
};

static struct kapi_process kapi_proc_table[PROCESS_MAX];
static struct kapi_thread kapi_thread_table[PROCESS_MAX];
static int kapi_proc_initialized = 0;

int kapi_process_init(void)
{
    if (kapi_proc_initialized) {
        return KAPI_OK;
    }

    memset(kapi_proc_table, 0, sizeof(kapi_proc_table));
    memset(kapi_thread_table, 0, sizeof(kapi_thread_table));
    kapi_proc_initialized = 1;
    return KAPI_OK;
}

kapi_proc_t kapi_proc_create(const char* name, void (*entry)(void*), void* arg, int prio)
{
    if (!name || !entry) {
        return NULL;
    }

    uint64_t kernel_prio = 2;
    switch (prio) {
        case KAPI_PRIO_IDLE:     kernel_prio = 0; break;
        case KAPI_PRIO_LOW:      kernel_prio = 1; break;
        case KAPI_PRIO_NORMAL:   kernel_prio = 2; break;
        case KAPI_PRIO_HIGH:     kernel_prio = 3; break;
        case KAPI_PRIO_REALTIME: kernel_prio = 4; break;
    }

    process_create(name, (void*)entry, kernel_prio);

    for (uint64_t i = 0; i < process_count; i++) {
        if (processes[i].state != PROCESS_TERMINATED &&
            strcmp(processes[i].name, name) == 0) {
            kapi_proc_table[i].pid = (int)processes[i].id;
            return &kapi_proc_table[i];
        }
    }

    return NULL;
}

void kapi_proc_exit(int status)
{
    (void)status;
    process_exit(0);
}

int kapi_proc_kill(kapi_proc_t proc, int force)
{
    if (!proc) {
        return KAPI_EINVAL;
    }

    for (uint64_t i = 0; i < process_count; i++) {
        if ((int)processes[i].id == proc->pid) {
            processes[i].state = PROCESS_TERMINATED;
            return KAPI_OK;
        }
    }
    return KAPI_ERROR;
}

int kapi_proc_kill_by_pid(int pid, int force)
{
    for (uint64_t i = 0; i < process_count; i++) {
        if ((int)processes[i].id == pid) {
            processes[i].state = PROCESS_TERMINATED;
            return KAPI_OK;
        }
    }
    return KAPI_ERROR;
}

kapi_proc_t kapi_proc_current(void)
{
    static struct kapi_process current;

    for (uint64_t i = 0; i < process_count; i++) {
        if (processes[i].state == PROCESS_RUNNING) {
            current.pid = (int)processes[i].id;
            return &current;
        }
    }
    current.pid = 0;
    return &current;
}

int kapi_proc_get_pid(kapi_proc_t proc)
{
    if (!proc) return -1;
    return proc->pid;
}

int kapi_proc_get_info(kapi_proc_t proc, kapi_proc_info_t* info)
{
    if (!info) {
        return KAPI_EINVAL;
    }

    memset(info, 0, sizeof(*info));

    uint64_t idx = 0;

    if (proc) {
        for (uint64_t i = 0; i < process_count; i++) {
            if ((int)processes[i].id == proc->pid) {
                idx = i;
                break;
            }
        }
    }

    info->pid = (int)processes[idx].id;
    info->state = (int)processes[idx].state;
    info->priority = (int)processes[idx].priority;
    strncpy(info->name, processes[idx].name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    return KAPI_OK;
}

void kapi_proc_sleep(uint64_t ms)
{
    msleep(ms);
}

void kapi_proc_yield(void)
{
    process_yield();
}

int kapi_proc_wait(int pid, int* status)
{
    if (pid < -1 || pid == 0) {
        return KAPI_EINVAL;
    }

    if (pid > 0) {
        /* 等待特定子进程 */
        for (;;) {
            int found = 0;
            int terminated = 0;
            for (uint64_t i = 0; i < process_count; i++) {
                if ((int)processes[i].id == pid) {
                    found = 1;
                    if (processes[i].state == PROCESS_TERMINATED) {
                        terminated = 1;
                        if (status) {
                            *status = 0;  /* 当前进程结构未存储退出码，暂置 0 */
                        }
                    }
                    break;
                }
            }
            if (!found) {
                return KAPI_ERROR;
            }
            if (terminated) {
                return pid;
            }
            process_yield();
        }
    } else {
        /* pid == -1，等待任意子进程 */
        for (;;) {
            for (uint64_t i = 0; i < process_count; i++) {
                if (processes[i].state == PROCESS_TERMINATED) {
                    if (status) {
                        *status = 0;
                    }
                    return (int)processes[i].id;
                }
            }
            process_yield();
        }
    }
}

kapi_thread_t kapi_thread_create(void (*entry)(void*), void* arg)
{
    if (!entry) {
        return NULL;
    }

    int tid = thread_create((void*)entry, arg);
    if (tid < 0) {
        return NULL;
    }

    int slot = -1;
    for (int i = 0; i < PROCESS_MAX; i++) {
        if (kapi_thread_table[i].tid == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    kapi_thread_table[slot].tid = tid;
    return &kapi_thread_table[slot];
}

void kapi_thread_exit(void)
{
    thread_yield();
}

int kapi_ipc_send(int pid, const void* msg, size_t size)
{
    if (!msg || size == 0 || size > IPC_MAX_SIZE) {
        return KAPI_EINVAL;
    }

    return ipc_send((uint64_t)pid, 0, msg, size) == 0 ? KAPI_OK : KAPI_ERROR;
}

int kapi_ipc_recv(void* msg, size_t size, uint64_t timeout_ms)
{
    if (!msg || size == 0) {
        return KAPI_EINVAL;
    }

    uint64_t type, rsize;
    if (ipc_receive(0, &type, msg, &rsize) != 0) {
        return KAPI_ERROR;
    }

    size_t copy_size = size > rsize ? rsize : size;
    return (int)copy_size;
}

int kapi_proc_count(void)
{
    int count = 0;
    for (uint64_t i = 0; i < process_count; i++) {
        if (processes[i].state != PROCESS_TERMINATED) {
            count++;
        }
    }
    return count;
}

int kapi_proc_list(kapi_proc_info_t* infos, int count)
{
    if (!infos || count <= 0) {
        return 0;
    }

    int filled = 0;
    for (uint64_t i = 0; i < process_count && filled < count; i++) {
        if (processes[i].state != PROCESS_TERMINATED) {
            infos[filled].pid = (int)processes[i].id;
            infos[filled].state = (int)processes[i].state;
            infos[filled].priority = (int)processes[i].priority;
            strncpy(infos[filled].name, processes[i].name,
                    sizeof(infos[filled].name) - 1);
            infos[filled].name[sizeof(infos[filled].name) - 1] = '\0';
            infos[filled].cpu_time = 0;
            infos[filled].mem_usage = 0;
            filled++;
        }
    }

    return filled;
}
