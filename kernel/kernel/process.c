#include <arch/process.h>
#include <arch/cfs.h>
#include <arch/memory.h>
#include <memory.h>
#include <string.h>
#include <slab.h>

process_t processes[PROCESS_MAX];
uint64_t process_count = 0;
uint64_t current_process = 0;

static uint64_t sched_jiffies = 0;

static void __process_trampoline(void)
{
    process_t* p = &processes[current_process];
    if (p->entry) {
        void (*fn)(void*) = (void (*)(void*))p->entry;
        fn(p->entry_arg);
    }
    process_exit(0);
}

void process_init(void)
{
    for (int i = 0; i < PROCESS_MAX; i++) {
        processes[i].state = PROCESS_UNUSED;
    }
    process_count = 0;
    current_process = 0;
    sched_jiffies = 0;
    cfs_init();
}

static int priority_to_nice(uint64_t priority)
{
    switch (priority) {
        case PRIORITY_IDLE:     return 19;
        case PRIORITY_LOW:      return 10;
        case PRIORITY_NORMAL:   return 0;
        case PRIORITY_HIGH:     return -10;
        case PRIORITY_REALTIME: return -20;
        default:                return 0;
    }
}

uint64_t process_create(const char* name, void* entry, uint64_t priority)
{
    return process_create_ex(name, entry, NULL, priority, 0, current_process);
}

void process_exit(uint64_t code)
{
    (void)code;
    if (current_process < PROCESS_MAX) {
        process_t* p = &processes[current_process];
        cfs_dequeue_task(cfs_get_rq(), &p->cfs_task);
        p->state = PROCESS_DEAD;
    }
}

void process_yield(void)
{
    if (process_count <= 1) return;

    sched_jiffies++;

    cfs_rq_t* rq = cfs_get_rq();
    process_t* prev = &processes[current_process];

    /* CFS ready tree must contain only READY tasks.  The previous code kept
     * the RUNNING task inside the red-black tree and then changed vruntime in
     * place, breaking tree ordering and causing scheduler "code fights". */
    if (prev->state == PROCESS_RUNNING) {
        cfs_account_exec(rq, &prev->cfs_task, CFS_TICK_NSEC);
        prev->state = PROCESS_READY;
        cfs_enqueue_task(rq, &prev->cfs_task);
    }

    /* 通过 CFS 选取下一个任务 */
    cfs_task_t* next_task = cfs_pick_next_task(rq);
    if (!next_task) {
        if (prev->state == PROCESS_READY) {
            prev->state = PROCESS_RUNNING;
            rq->curr = &prev->cfs_task;
            cfs_dequeue_task(rq, &prev->cfs_task);
        }
        return;
    }

    /* 通过 cfs_task 反查 process_t */
    uint64_t next_pid = current_process;
    for (uint64_t i = 0; i < PROCESS_MAX; i++) {
        if (processes[i].state != PROCESS_UNUSED &&
            &processes[i].cfs_task == next_task) {
            next_pid = i;
            break;
        }
    }

    if (next_pid == current_process) {
        cfs_dequeue_task(rq, next_task);
        prev->state = PROCESS_RUNNING;
        prev->sched_count++;
        rq->curr = next_task;
        return;
    }

    cfs_dequeue_task(rq, next_task);
    rq->curr = next_task;

    uint64_t old_pid = current_process;
    processes[old_pid].context.cr3 = (uint64_t)pmap_get();
    current_process = next_pid;
    processes[next_pid].state = PROCESS_RUNNING;
    processes[next_pid].sched_count++;
    if (processes[next_pid].context.cr3) pmap_switch((void*)processes[next_pid].context.cr3);

    process_switch_fpu(&processes[old_pid], &processes[next_pid]);
    save_fs_gs(&processes[old_pid]);
    process_switch(&processes[old_pid].context,
                   &processes[next_pid].context);
    restore_fs_gs(&processes[next_pid]);

    processes[current_process].state = PROCESS_RUNNING;
}

__attribute__((naked)) void process_switch(process_context_t* old, process_context_t* new)
{
    __asm__ volatile (
        "pushq %rbp\n\t"
        "pushq %rbx\n\t"
        "pushq %r12\n\t"
        "pushq %r13\n\t"
        "pushq %r14\n\t"
        "pushq %r15\n\t"
        "movq %rsp, 144(%rdi)\n\t"
        "movq 144(%rsi), %rsp\n\t"
        "popq %r15\n\t"
        "popq %r14\n\t"
        "popq %r13\n\t"
        "popq %r12\n\t"
        "popq %rbx\n\t"
        "popq %rbp\n\t"
        "ret\n\t"
    );
}

static inline void fpu_save(uint8_t* buf)
{
    __asm__ volatile("fxsave64 (%0)" :: "r"(buf) : "memory");
}

static inline void fpu_restore(const uint8_t* buf)
{
    __asm__ volatile("fxrstor64 (%0)" :: "r"(buf) : "memory");
}

void process_switch_fpu(process_t* old_proc, process_t* new_proc)
{
    if (!old_proc || !new_proc) return;

    if (old_proc->fpu_used) {
        fpu_save(old_proc->fpu_state);
    }

    if (new_proc->fpu_used) {
        fpu_restore(new_proc->fpu_state);
    } else {
        __asm__ volatile("fninit");
        new_proc->fpu_used = 1;
    }
}

static inline void save_fs_gs(process_t* proc)
{
    if (!proc) return;
    __asm__ volatile(
        "movw %%fs, %0\n\t"
        "movw %%gs, %1\n\t"
        : "=m"(proc->fs_selector), "=m"(proc->gs_selector)
    );
    if (proc->fs_selector) {
        uint32_t lo, hi;
        __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"((uint32_t)0xC0000100));
        proc->fs_base = ((uint64_t)hi << 32) | lo;
    }
    if (proc->gs_selector) {
        uint32_t lo, hi;
        __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"((uint32_t)0xC0000101));
        proc->gs_base = ((uint64_t)hi << 32) | lo;
    }
}

static inline void restore_fs_gs(process_t* proc)
{
    if (!proc) return;
    if (proc->fs_selector) {
        __asm__ volatile("movw %0, %%fs" :: "m"(proc->fs_selector));
        uint32_t lo = (uint32_t)(proc->fs_base & 0xFFFFFFFF);
        uint32_t hi = (uint32_t)(proc->fs_base >> 32);
        __asm__ volatile("wrmsr" :: "a"(lo), "d"(hi), "c"((uint32_t)0xC0000100));
    }
    if (proc->gs_selector) {
        __asm__ volatile("movw %0, %%gs" :: "m"(proc->gs_selector));
        uint32_t lo = (uint32_t)(proc->gs_base & 0xFFFFFFFF);
        uint32_t hi = (uint32_t)(proc->gs_base >> 32);
        __asm__ volatile("wrmsr" :: "a"(lo), "d"(hi), "c"((uint32_t)0xC0000101));
    }
}

uint64_t process_get_current_id(void)
{
    return current_process;
}

uint64_t process_current_id(void)
{
    return current_process;
}

process_t* process_get(uint64_t pid)
{
    if (pid < PROCESS_MAX && processes[pid].state != PROCESS_UNUSED) {
        return &processes[pid];
    }
    return NULL;
}

void process_wakeup(uint64_t pid)
{
    if (pid < PROCESS_MAX &&
        (processes[pid].state == PROCESS_WAITING ||
         processes[pid].state == PROCESS_SLEEPING)) {
        processes[pid].state = PROCESS_READY;
        cfs_enqueue_task(cfs_get_rq(), &processes[pid].cfs_task);
    }
}

void enqueue_process(uint64_t pid)
{
    if (pid < PROCESS_MAX &&
        processes[pid].state != PROCESS_UNUSED &&
        processes[pid].state != PROCESS_DEAD &&
        processes[pid].state != PROCESS_RUNNING) {
        processes[pid].state = PROCESS_READY;
        cfs_enqueue_task(cfs_get_rq(), &processes[pid].cfs_task);
    }
}

uint64_t process_create_ex(const char* name, void* entry, void* arg,
                           uint64_t priority, uint32_t flags, uint64_t parent)
{
    uint64_t pid = (uint64_t)-1;
    for (uint64_t i = 0; i < PROCESS_MAX; i++) {
        if (processes[i].state == PROCESS_UNUSED) {
            pid = i;
            break;
        }
    }
    if (pid == (uint64_t)-1) {
        return (uint64_t)-1;
    }

    process_t* proc = &processes[pid];
    memset(proc, 0, sizeof(process_t));
    proc->id = pid;
    proc->priority = priority;
    proc->state = PROCESS_READY;
    proc->entry = entry;
    proc->entry_arg = arg;
    proc->flags = flags;
    proc->parent_id = parent;
    proc->context.cr3 = (uint64_t)pmap_get();
    proc->nice = priority_to_nice(priority);

    if (name) {
        strncpy(proc->name, name, sizeof(proc->name) - 1);
        proc->name[sizeof(proc->name) - 1] = '\0';
    }

    proc->stack = kzalloc(PROCESS_STACK_SIZE);
    if (!proc->stack) {
        proc->state = PROCESS_UNUSED;
        return (uint64_t)-1;
    }

    uint64_t* sp = (uint64_t*)((uint8_t*)proc->stack + PROCESS_STACK_SIZE);
    sp--;
    *sp = (uint64_t)__process_trampoline;
    sp -= 6;
    proc->context.rsp = (uint64_t)sp;

    cfs_task_init(&proc->cfs_task, proc->nice);
    cfs_enqueue_task(cfs_get_rq(), &proc->cfs_task);

    if (pid >= (uint64_t)process_count) {
        process_count = pid + 1;
    }
    return pid;
}

int thread_create(void* entry, void* arg)
{
    uint64_t tid = process_create_ex("thread", entry, arg,
                                      PRIORITY_NORMAL, 0, current_process);
    if (tid == (uint64_t)-1) {
        return -1;
    }
    return (int)tid;
}

void thread_yield(void)
{
    process_yield();
}

void scheduler_tick(void)
{
    sched_jiffies++;
    cfs_scheduler_tick(cfs_get_rq());
}

void process_sleep(uint64_t ms)
{
    if (current_process < PROCESS_MAX) {
        process_t* p = &processes[current_process];
        p->state = PROCESS_SLEEPING;
        p->wakeup_time = sched_jiffies + ms;
        cfs_dequeue_task(cfs_get_rq(), &p->cfs_task);
    }
    process_yield();
}

int process_kill(uint64_t pid, int signal)
{
    (void)signal;
    if (pid >= PROCESS_MAX) return -1;
    process_t* p = &processes[pid];
    if (p->state == PROCESS_UNUSED) return -1;
    cfs_dequeue_task(cfs_get_rq(), &p->cfs_task);
    p->state = PROCESS_DEAD;
    return 0;
}

int process_wait(uint64_t pid, int* status, uint64_t timeout_ms)
{
    (void)timeout_ms;
    if (pid >= PROCESS_MAX) return -1;
    while (processes[pid].state != PROCESS_DEAD &&
           processes[pid].state != PROCESS_ZOMBIE) {
        process_yield();
    }
    if (status) *status = 0;
    return 0;
}

uint64_t process_find_by_name(const char* name)
{
    if (!name) return (uint64_t)-1;
    for (uint64_t i = 0; i < PROCESS_MAX; i++) {
        if (processes[i].state != PROCESS_UNUSED &&
            strcmp(processes[i].name, name) == 0) {
            return i;
        }
    }
    return (uint64_t)-1;
}

int process_set_priority(uint64_t pid, uint64_t priority)
{
    if (pid >= PROCESS_MAX) return -1;
    process_t* p = &processes[pid];
    if (p->state == PROCESS_UNUSED) return -1;
    p->priority = priority;
    p->nice = priority_to_nice(priority);
    p->cfs_task.nice = p->nice;
    p->cfs_task.load_weight = cfs_nice_to_weight(p->nice);
    return 0;
}

void process_dump(const char* prefix)
{
    (void)prefix;
}

void process_start(void)
{
    /* 启动第一个 READY 进程 */
    for (uint64_t i = 0; i < PROCESS_MAX; i++) {
        if (processes[i].state == PROCESS_READY) {
            current_process = i;
            cfs_dequeue_task(cfs_get_rq(), &processes[i].cfs_task);
            processes[i].state = PROCESS_RUNNING;
            cfs_get_rq()->curr = &processes[i].cfs_task;
            return;
        }
    }
}

static scheduler_stats_t g_scheduler_stats = {0};

scheduler_stats_t* scheduler_get_stats(void)
{
    g_scheduler_stats.total_switches = sched_jiffies;
    return &g_scheduler_stats;
}

static rlimit_t process_rlimits[PROCESS_MAX][RLIMIT_NLIMITS];

static void __init_rlimits(uint64_t pid)
{
    process_rlimits[pid][RLIMIT_CPU].rlim_cur = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_CPU].rlim_max = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_FSIZE].rlim_max = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_DATA].rlim_cur = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_DATA].rlim_max = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_STACK].rlim_cur = 8 * 1024 * 1024;
    process_rlimits[pid][RLIMIT_STACK].rlim_max = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_CORE].rlim_cur = 0;
    process_rlimits[pid][RLIMIT_CORE].rlim_max = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_RSS].rlim_cur = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_RSS].rlim_max = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_NPROC].rlim_cur = 4096;
    process_rlimits[pid][RLIMIT_NPROC].rlim_max = 4096;
    process_rlimits[pid][RLIMIT_NOFILE].rlim_cur = 1024;
    process_rlimits[pid][RLIMIT_NOFILE].rlim_max = 4096;
    process_rlimits[pid][RLIMIT_MEMLOCK].rlim_cur = 64 * 1024;
    process_rlimits[pid][RLIMIT_MEMLOCK].rlim_max = 64 * 1024;
    process_rlimits[pid][RLIMIT_AS].rlim_cur = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_AS].rlim_max = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_LOCKS].rlim_cur = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_LOCKS].rlim_max = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_SIGPENDING].rlim_cur = 4096;
    process_rlimits[pid][RLIMIT_SIGPENDING].rlim_max = 4096;
    process_rlimits[pid][RLIMIT_MSGQUEUE].rlim_cur = 819200;
    process_rlimits[pid][RLIMIT_MSGQUEUE].rlim_max = 819200;
    process_rlimits[pid][RLIMIT_NICE].rlim_cur = 0;
    process_rlimits[pid][RLIMIT_NICE].rlim_max = 0;
    process_rlimits[pid][RLIMIT_RTPRIO].rlim_cur = 0;
    process_rlimits[pid][RLIMIT_RTPRIO].rlim_max = 0;
    process_rlimits[pid][RLIMIT_RTTIME].rlim_cur = RLIM_INFINITY;
    process_rlimits[pid][RLIMIT_RTTIME].rlim_max = RLIM_INFINITY;
}

int process_signal_send(uint64_t pid, int sig)
{
    if (pid >= PROCESS_MAX || sig < 1 || sig > 64) return -1;
    process_t* p = &processes[pid];
    if (p->state == PROCESS_UNUSED || p->state == PROCESS_DEAD) return -1;

    if (sig == SIGKILL) {
        cfs_dequeue_task(cfs_get_rq(), &p->cfs_task);
        p->state = PROCESS_DEAD;
        return 0;
    }

    if (sig == SIGSTOP) {
        p->state = PROCESS_STOPPED;
        cfs_dequeue_task(cfs_get_rq(), &p->cfs_task);
        return 0;
    }

    if (sig == SIGCONT) {
        p->state = PROCESS_READY;
        cfs_enqueue_task(cfs_get_rq(), &p->cfs_task);
        return 0;
    }

    p->pending_signals |= (1ULL << sig);

    if (p->state == PROCESS_SLEEPING || p->state == PROCESS_WAITING) {
        p->state = PROCESS_READY;
        cfs_enqueue_task(cfs_get_rq(), &p->cfs_task);
    }

    return 0;
}

int process_signal_mask(uint64_t how, uint64_t set, uint64_t* oldset)
{
    process_t* p = &processes[current_process];
    if (oldset) *oldset = p->signal_mask;

    switch (how) {
        case 0:
            p->signal_mask = set;
            break;
        case 1:
            p->signal_mask |= set;
            break;
        case 2:
            p->signal_mask &= ~set;
            break;
        default:
            return -1;
    }
    return 0;
}

int process_setrlimit(int resource, const rlimit_t* rlim)
{
    if (resource < 0 || resource >= RLIMIT_NLIMITS || !rlim) return -1;
    if (rlim->rlim_cur > rlim->rlim_max) return -1;
    process_rlimits[current_process][resource] = *rlim;
    return 0;
}

int process_getrlimit(int resource, rlimit_t* rlim)
{
    if (resource < 0 || resource >= RLIMIT_NLIMITS || !rlim) return -1;
    *rlim = process_rlimits[current_process][resource];
    return 0;
}

uint64_t process_get_uid(uint64_t pid)
{
    if (pid >= PROCESS_MAX) return (uint64_t)-1;
    return 0;
}

uint64_t process_get_gid(uint64_t pid)
{
    if (pid >= PROCESS_MAX) return (uint64_t)-1;
    return 0;
}

int process_set_uid(uint64_t pid, uint64_t uid)
{
    if (pid >= PROCESS_MAX) return -1;
    (void)uid;
    return 0;
}

int process_set_gid(uint64_t pid, uint64_t gid)
{
    if (pid >= PROCESS_MAX) return -1;
    (void)gid;
    return 0;
}

int process_fork(void)
{
    process_t* parent = &processes[current_process];
    uint64_t child_pid = process_create_ex(
        parent->name,
        parent->entry,
        parent->entry_arg,
        parent->priority,
        parent->flags,
        current_process
    );
    if (child_pid == (uint64_t)-1) return -1;

    process_t* child = &processes[child_pid];
    child->pending_signals = 0;
    child->signal_mask = parent->signal_mask;
    __init_rlimits(child_pid);

    for (int i = 0; i < 16 && i < parent->fd_count; i++) {
        child->fd_table[i] = parent->fd_table[i];
    }
    child->fd_count = parent->fd_count;

    return (int)child_pid;
}

int process_execve(const char* path, char* const argv[], char* const envp[])
{
    if (!path) return -1;

    process_t* p = &processes[current_process];

    for (int i = 0; i < 16; i++) {
        p->fd_table[i] = -1;
    }
    p->fd_count = 0;
    p->pending_signals = 0;
    p->signal_mask = 0;

    (void)argv; (void)envp;
    return 0;
}