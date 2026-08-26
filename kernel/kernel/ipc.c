

#include "ipc.h"
#include "sync.h"
#include "wait.h"
#include <arch/memory.h>
#include <arch/mmap.h>
#include <arch/ipc.h>
#include <string.h>
#include <slab.h>

#define IPC_NOWAIT  04000

extern uint64_t current_process;
extern process_t processes[PROCESS_MAX];

extern void*  pmap_get(void);
extern int    pmap_map_page(void* pml4, uint64_t vaddr, uint64_t paddr,
                              uint64_t flags);
extern void   pmap_unmap_page(void* pml4, uint64_t vaddr);
extern uint64_t pmap_get_physical(void* pml4, uint64_t vaddr);
extern void*  alloc_pages_virt(uint32_t order);
extern void   free_pages_virt(void* virt);

static mmap_context_t shm_mmap_ctx;
static int shm_mmap_inited = 0;

static shm_segment_t shm_table[SHM_MAX];
static uint32_t shm_count = 0;
static spinlock_t shm_lock = SPINLOCK_INIT;

int shm_init(void)
{
    memset(shm_table, 0, sizeof(shm_table));
    shm_count = 0;
    spin_init(&shm_lock);
    mmap_context_init(&shm_mmap_ctx);
    shm_mmap_inited = 1;
    return 0;
}

int shm_get(ipc_key_t key, uint64_t size, int flags, uint32_t* shm_id)
{
    spin_lock(&shm_lock);

    if (key != IPC_PRIVATE) {
        for (uint32_t i = 0; i < shm_count; i++) {
            if (shm_table[i].key == key) {
                *shm_id = i;
                spin_unlock(&shm_lock);
                return 0;
            }
        }
        if (!(flags & IPC_CREAT)) {
            spin_unlock(&shm_lock);
            return -1;
        }
    }

    if (shm_count >= SHM_MAX) {
        spin_unlock(&shm_lock);
        return -1;
    }

    uint32_t id = shm_count++;
    shm_segment_t* seg = &shm_table[id];

    seg->id = id;
    seg->key = key;
    seg->perm.uid = seg->perm.cuid = 0;
    seg->perm.gid = seg->perm.cgid = 0;
    seg->perm.mode = 0666;
    seg->size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    seg->phys_addr = kzalloc(seg->size);
    if (!seg->phys_addr) {
        shm_count--;
        spin_unlock(&shm_lock);
        return -1;
    }
    seg->ref_count = 0;
    seg->creator_pid = current_process;
    seg->flags = 0;
    seg->name[0] = '\0';
    for (int i = 0; i < SHM_MAX_MAPPERS; i++) {
        seg->mapped_vaddr[i] = NULL;
    }

    *shm_id = id;
    spin_unlock(&shm_lock);
    return 0;
}

void* shm_at(uint32_t shm_id, void* addr, int flags)
{
    if (shm_id >= shm_count) return NULL;

    spin_lock(&shm_lock);
    shm_segment_t* seg = &shm_table[shm_id];

    
    if (seg->mapped_vaddr[current_process] != NULL) {
        void* existing = seg->mapped_vaddr[current_process];
        seg->ref_count++;
        spin_unlock(&shm_lock);
        return existing;
    }

    
    uint32_t prot = PROT_READ | PROT_WRITE;
    if (flags & SHM_RDONLY) {
        prot &= ~PROT_WRITE;
    }

    
    uint64_t map_addr = mmap_do_mmap(&shm_mmap_ctx, (uint64_t)addr, seg->size,
                                      prot, MAP_SHARED, -1, 0);
    if (map_addr == (uint64_t)-1 || map_addr == 0) {
        spin_unlock(&shm_lock);
        return NULL;
    }

    
    void* pml4 = pmap_get();
    uint64_t npages = seg->size / PAGE_SIZE;
    uint64_t pte_flags = PAGE_PRESENT | PAGE_USER;
    if (prot & PROT_WRITE) {
        pte_flags |= PAGE_WRITABLE;
    }
    if (!(prot & PROT_EXEC)) {
        pte_flags |= PAGE_NO_EXEC;
    }

    for (uint64_t i = 0; i < npages; i++) {
        uint64_t va = map_addr + i * PAGE_SIZE;
        uint64_t old_pa = pmap_get_physical(pml4, va);
        if (old_pa) {
            pmap_unmap_page(pml4, va);
            free_pages_virt((void*)old_pa);
        }
        uint64_t pa = (uint64_t)seg->phys_addr + i * PAGE_SIZE;
        pmap_map_page(pml4, va, pa, pte_flags);
    }

    seg->mapped_vaddr[current_process] = (void*)map_addr;
    seg->ref_count++;
    spin_unlock(&shm_lock);

    return (void*)map_addr;
}

int shm_dt(void* addr)
{
    if (!addr) return -1;

    spin_lock(&shm_lock);

    for (uint32_t i = 0; i < shm_count; i++) {
        shm_segment_t* seg = &shm_table[i];
        if (seg->mapped_vaddr[current_process] == addr) {
            
            mmap_do_munmap(&shm_mmap_ctx, (uint64_t)addr, seg->size);
            seg->mapped_vaddr[current_process] = NULL;
            if (seg->ref_count > 0) {
                seg->ref_count--;
            }
            spin_unlock(&shm_lock);
            return 0;
        }
    }

    spin_unlock(&shm_lock);
    return -1;
}

int shm_ctl(uint32_t shm_id, int cmd, void* buf)
{
    if (shm_id >= shm_count) return -1;
    shm_segment_t* seg = &shm_table[shm_id];

    switch (cmd) {
        case 0:
            spin_lock(&shm_lock);
            if (seg->ref_count == 0) {
                kfree(seg->phys_addr);
                seg->phys_addr = NULL;
                seg->size = 0;
            } else {
                seg->flags |= SHM_DEST;
            }
            spin_unlock(&shm_lock);
            break;
        case 1:
            if (buf) memcpy(buf, seg, sizeof(shm_segment_t));
            break;
        default:
            return -1;
    }
    return 0;
}

static msg_queue_t msg_queues[MSG_MAX];
static msg_entry_t* msg_entries[MSG_MAX];
static uint32_t msg_count = 0;
static spinlock_t msg_lock = SPINLOCK_INIT;

int msg_init(void)
{
    memset(msg_queues, 0, sizeof(msg_queues));
    memset(msg_entries, 0, sizeof(msg_entries));
    msg_count = 0;
    spin_init(&msg_lock);
    return 0;
}

int msg_get(ipc_key_t key, int flags, uint32_t* msg_id)
{
    spin_lock(&msg_lock);

    if (key != IPC_PRIVATE) {
        for (uint32_t i = 0; i < msg_count; i++) {
            if (msg_queues[i].key == key) {
                *msg_id = i;
                spin_unlock(&msg_lock);
                return 0;
            }
        }
        if (!(flags & IPC_CREAT)) {
            spin_unlock(&msg_lock);
            return -1;
        }
    }

    if (msg_count >= MSG_MAX) {
        spin_unlock(&msg_lock);
        return -1;
    }

    uint32_t id = msg_count++;
    msg_queues[id].id = id;
    msg_queues[id].key = key;
    msg_queues[id].perm.mode = 0666;
    msg_queues[id].msg_count = 0;
    msg_queues[id].current_bytes = 0;
    msg_queues[id].max_bytes = 65536;
    msg_queues[id].creator_pid = current_process;
    msg_entries[id] = NULL;

    *msg_id = id;
    spin_unlock(&msg_lock);
    return 0;
}

int msg_send(uint32_t msg_id, const void* msgp, uint64_t size, int flags)
{
    if (msg_id >= msg_count) return -1;
    if (size > MSG_TEXT_MAX) return -1;

    spin_lock(&msg_lock);

    msg_queue_t* mq = &msg_queues[msg_id];

    while (mq->current_bytes + size > mq->max_bytes) {
        if (flags & IPC_NOWAIT) {
            spin_unlock(&msg_lock);
            return -1;
        }

        spin_unlock(&msg_lock);
        extern void process_yield(void);
        process_yield();
        spin_lock(&msg_lock);
    }

    msg_entry_t* entry = (msg_entry_t*)kzalloc(sizeof(msg_entry_t));
    if (!entry) {
        spin_unlock(&msg_lock);
        return -1;
    }

    entry->mtype = *(const long*)msgp;
    entry->msize = size;
    memcpy(entry->mtext, (const uint8_t*)msgp + sizeof(long), size);

    entry->next = NULL;
    msg_entry_t** tail = &msg_entries[msg_id];
    while (*tail) tail = &(*tail)->next;
    *tail = entry;

    mq->msg_count++;
    mq->current_bytes += size;

    spin_unlock(&msg_lock);
    return 0;
}

int msg_recv(uint32_t msg_id, void* msgp, uint64_t size, long mtype, int flags)
{
    if (msg_id >= msg_count) return -1;

    spin_lock(&msg_lock);

    msg_entry_t** head = &msg_entries[msg_id];

    while (!*head) {
        if (flags & IPC_NOWAIT) {
            spin_unlock(&msg_lock);
            return -1;
        }
        spin_unlock(&msg_lock);
        extern void process_yield(void);
        process_yield();
        spin_lock(&msg_lock);
    }

    msg_entry_t** prev = head;
    while (*prev) {
        if (mtype == 0 || (*prev)->mtype == mtype || (mtype < 0 && (*prev)->mtype <= -mtype)) {
            break;
        }
        prev = &(*prev)->next;
    }

    if (!*prev) {
        spin_unlock(&msg_lock);
        return -1;
    }

    msg_entry_t* entry = *prev;
    *prev = entry->next;

    *(long*)msgp = entry->mtype;
    uint64_t copy_size = entry->msize < size ? entry->msize : size;
    memcpy((uint8_t*)msgp + sizeof(long), entry->mtext, copy_size);

    msg_queues[msg_id].msg_count--;
    msg_queues[msg_id].current_bytes -= entry->msize;

    kfree(entry);
    spin_unlock(&msg_lock);
    return (int)copy_size;
}

int msg_ctl(uint32_t msg_id, int cmd, void* buf)
{
    if (msg_id >= msg_count) return -1;
    switch (cmd) {
        case 0:
            spin_lock(&msg_lock);
            msg_entry_t* entry = msg_entries[msg_id];
            while (entry) {
                msg_entry_t* next = entry->next;
                kfree(entry);
                entry = next;
            }
            msg_entries[msg_id] = NULL;
            memset(&msg_queues[msg_id], 0, sizeof(msg_queue_t));
            spin_unlock(&msg_lock);
            break;
        case 1:
            if (buf) memcpy(buf, &msg_queues[msg_id], sizeof(msg_queue_t));
            break;
        default:
            return -1;
    }
    return 0;
}

static sem_set_t sem_table[SEM_MAX];
static uint32_t sem_count = 0;
static spinlock_t sem_lock = SPINLOCK_INIT;

int sem_init(void)
{
    memset(sem_table, 0, sizeof(sem_table));
    sem_count = 0;
    spin_init(&sem_lock);
    return 0;
}

int sem_get(ipc_key_t key, int nsems, int flags, uint32_t* sem_id)
{
    if (nsems < 1 || nsems > SEM_SET_MAX) return -1;

    spin_lock(&sem_lock);

    if (key != IPC_PRIVATE) {
        for (uint32_t i = 0; i < sem_count; i++) {
            if (sem_table[i].key == key) {
                *sem_id = i;
                spin_unlock(&sem_lock);
                return 0;
            }
        }
        if (!(flags & IPC_CREAT)) {
            spin_unlock(&sem_lock);
            return -1;
        }
    }

    if (sem_count >= SEM_MAX) {
        spin_unlock(&sem_lock);
        return -1;
    }

    uint32_t id = sem_count++;
    sem_set_t* ss = &sem_table[id];
    ss->id = id;
    ss->key = key;
    ss->perm.mode = 0666;
    ss->nsems = nsems;
    memset(ss->values, 0, sizeof(ss->values));
    ss->creator_pid = current_process;

    *sem_id = id;
    spin_unlock(&sem_lock);
    return 0;
}

int sem_op(uint32_t sem_id, struct sembuf* ops, uint32_t nops)
{
    if (sem_id >= sem_count) return -1;
    sem_set_t* ss = &sem_table[sem_id];

    spin_lock(&sem_lock);

    for (uint32_t i = 0; i < nops; i++) {
        uint16_t num = ops[i].sem_num;
        if (num >= ss->nsems) {
            spin_unlock(&sem_lock);
            return -1;
        }

        int16_t op = ops[i].sem_op;
        int val = ss->values[num] + op;

        if (val < 0) {

            if (ops[i].sem_flg & IPC_NOWAIT) {
                spin_unlock(&sem_lock);
                return -1;
            }

            spin_unlock(&sem_lock);
            extern void process_yield(void);
            process_yield();
            spin_lock(&sem_lock);

            i--;
            continue;
        }

        ss->values[num] = val;
    }

    spin_unlock(&sem_lock);
    return 0;
}

int sem_ctl(uint32_t sem_id, int semnum, int cmd, void* arg)
{
    if (sem_id >= sem_count) return -1;
    sem_set_t* ss = &sem_table[sem_id];

    switch (cmd) {
        case 0:
            spin_lock(&sem_lock);
            memset(ss, 0, sizeof(sem_set_t));
            spin_unlock(&sem_lock);
            break;
        case 1:
            if (arg && (uint32_t)semnum < ss->nsems) {
                ss->values[semnum] = *(int*)arg;
            }
            break;
        case 2:
            if (arg && (uint32_t)semnum < ss->nsems) {
                *(int*)arg = ss->values[semnum];
            }
            break;
        case 3:
            if (arg) memcpy(arg, ss->values, ss->nsems * sizeof(int16_t));
            break;
        default:
            return -1;
    }
    return 0;
}

typedef struct futex_waiter {
    struct futex_waiter* next;
    uint64_t pid;
    void*    uaddr;
} futex_waiter_t;

static futex_waiter_t* futex_queues[FUTEX_MAX_QUEUES];
static spinlock_t futex_lock = SPINLOCK_INIT;

static uint32_t __futex_hash(void* uaddr)
{
    return (uint32_t)((uint64_t)uaddr >> 4) % FUTEX_MAX_QUEUES;
}

int futex_wait(void* uaddr, uint32_t val, uint64_t timeout_ms)
{

    uint32_t current = *(volatile uint32_t*)uaddr;
    if (current != val) return 0;

    spin_lock(&futex_lock);

    futex_waiter_t* waiter = (futex_waiter_t*)kzalloc(sizeof(futex_waiter_t));
    if (!waiter) {
        spin_unlock(&futex_lock);
        return -1;
    }

    waiter->pid = current_process;
    waiter->uaddr = uaddr;

    uint32_t h = __futex_hash(uaddr);
    waiter->next = futex_queues[h];
    futex_queues[h] = waiter;

    processes[current_process].state = PROCESS_SLEEPING;
    spin_unlock(&futex_lock);

    extern void process_yield(void);
    process_yield();

    spin_lock(&futex_lock);
    futex_waiter_t** pp = &futex_queues[h];
    while (*pp) {
        if (*pp == waiter) {
            *pp = waiter->next;
            break;
        }
        pp = &(*pp)->next;
    }
    kfree(waiter);
    spin_unlock(&futex_lock);

    return 0;
}

int futex_wake(void* uaddr, uint32_t count)
{
    if (count == 0) return 0;

    spin_lock(&futex_lock);

    uint32_t h = __futex_hash(uaddr);
    futex_waiter_t** pp = &futex_queues[h];
    uint32_t woken = 0;

    while (*pp && woken < count) {
        futex_waiter_t* waiter = *pp;

        if (waiter->uaddr == uaddr) {
            *pp = waiter->next;

            if (waiter->pid < PROCESS_MAX &&
                processes[waiter->pid].state == PROCESS_SLEEPING) {
                processes[waiter->pid].state = PROCESS_READY;
            }
            kfree(waiter);
            woken++;
        } else {
            pp = &(*pp)->next;
        }
    }

    spin_unlock(&futex_lock);
    return woken;
}

int futex_requeue(void* uaddr, void* uaddr2, uint32_t count)
{
    spin_lock(&futex_lock);

    uint32_t h1 = __futex_hash(uaddr);
    uint32_t h2 = __futex_hash(uaddr2);
    futex_waiter_t** pp = &futex_queues[h1];
    uint32_t moved = 0;

    while (*pp && moved < count) {
        futex_waiter_t* waiter = *pp;
        if (waiter->uaddr == uaddr) {
            *pp = waiter->next;

            waiter->uaddr = uaddr2;
            waiter->next = futex_queues[h2];
            futex_queues[h2] = waiter;
            moved++;
        } else {
            pp = &(*pp)->next;
        }
    }

    spin_unlock(&futex_lock);
    return moved;
}

#define SHMGET  23
#define SHMAT   21
#define SHMDT   22
#define SHMCTL  24
#define MSGGET  19
#define MSGSND  20
#define MSGRCV  25
#define MSGCTL  26
#define SEMGET  64
#define SEMOP   65
#define SEMCTL  66

long ipc_syscall(long call, long first, long second, long third, long fourth, long fifth)
{
    switch (call) {
        case SHMGET:
            {
                uint32_t id;
                int ret = shm_get((ipc_key_t)first, (uint64_t)second, (int)third, &id);
                return ret == 0 ? (long)id : -1;
            }
        case SHMAT:
            {
                void* ret = shm_at((uint32_t)first, (void*)second, (int)third);
                return ret ? (long)ret : -1;
            }
        case SHMDT:
            return shm_dt((void*)first);
        case SHMCTL:
            return shm_ctl((uint32_t)first, (int)second, (void*)third);

        case MSGGET:
            {
                uint32_t id;
                int ret = msg_get((ipc_key_t)first, (int)second, &id);
                return ret == 0 ? (long)id : -1;
            }
        case MSGSND:
            return msg_send((uint32_t)first, (void*)second, (uint64_t)third, (int)fourth);
        case MSGRCV:
            return msg_recv((uint32_t)first, (void*)second, (uint64_t)third, (long)fourth, (int)fifth);
        case MSGCTL:
            return msg_ctl((uint32_t)first, (int)second, (void*)third);

        case SEMGET:
            {
                uint32_t id;
                int ret = sem_get((ipc_key_t)first, (int)second, (int)third, &id);
                return ret == 0 ? (long)id : -1;
            }
        case SEMOP:
            return sem_op((uint32_t)first, (struct sembuf*)second, (uint32_t)third);
        case SEMCTL:
            return sem_ctl((uint32_t)first, (int)second, (int)third, (void*)fifth);

        default:
            return -1;
    }
}

static ipc_message_t ipc_buffer[IPC_MAX_MESSAGES];
static uint32_t ipc_head = 0;
static uint32_t ipc_tail = 0;
static uint32_t ipc_count = 0;
static spinlock_t ipc_buf_lock = SPINLOCK_INIT;

void ipc_init(void)
{
    shm_init();
    msg_init();
    sem_init();
    memset(ipc_buffer, 0, sizeof(ipc_buffer));
    ipc_head = 0;
    ipc_tail = 0;
    ipc_count = 0;
    spin_init(&ipc_buf_lock);
}

int ipc_send(uint64_t destination, uint64_t type, const void* data, uint64_t size)
{
    if (size > IPC_MAX_SIZE) return -1;

    spin_lock(&ipc_buf_lock);
    if (ipc_count >= IPC_MAX_MESSAGES) {
        spin_unlock(&ipc_buf_lock);
        return -1;
    }

    ipc_message_t* msg = &ipc_buffer[ipc_tail];
    msg->source = current_process;
    msg->destination = destination;
    msg->type = type;
    msg->size = size;
    if (data && size > 0) {
        memcpy(msg->data, data, size);
    }

    ipc_tail = (ipc_tail + 1) % IPC_MAX_MESSAGES;
    ipc_count++;
    spin_unlock(&ipc_buf_lock);

    if (destination < PROCESS_MAX) {
        process_wakeup(destination);
    }
    return 0;
}

int ipc_receive(uint64_t source, uint64_t* type, void* data, uint64_t* size)
{
    spin_lock(&ipc_buf_lock);

    for (uint32_t i = 0; i < ipc_count; i++) {
        uint32_t idx = (ipc_head + i) % IPC_MAX_MESSAGES;
        ipc_message_t* msg = &ipc_buffer[idx];
        if (msg->destination == current_process) {
            if (source == (uint64_t)-1 || msg->source == source) {
                if (type) *type = msg->type;
                if (data && size && *size > 0) {
                    uint64_t copy_size = msg->size < *size ? msg->size : *size;
                    memcpy(data, msg->data, copy_size);
                    *size = copy_size;
                } else if (size) {
                    *size = msg->size;
                }

                uint32_t tail_idx = (ipc_head + ipc_count - 1) % IPC_MAX_MESSAGES;
                if (idx != tail_idx) {
                    memcpy(&ipc_buffer[idx], &ipc_buffer[tail_idx], sizeof(ipc_message_t));
                }
                ipc_count--;
                spin_unlock(&ipc_buf_lock);
                return 0;
            }
        }
    }

    spin_unlock(&ipc_buf_lock);
    return -1;
}

int __srcu_read_idx = 0;

static ipc_channel_t ipc_channels[IPC_MAX_QUEUES];
static uint32_t ipc_channel_count = 0;
static spinlock_t ipc_channel_lock = SPINLOCK_INIT;

static ipc_shm_t ipc_shm_table[IPC_SHM_MAX];
static uint32_t ipc_shm_count = 0;
static spinlock_t ipc_shm_lock = SPINLOCK_INIT;

int ipc_channel_create(const char* name, uint64_t max_msg_size, uint64_t max_msg_count)
{
    if (!name) return -1;

    spin_lock(&ipc_channel_lock);

    for (uint32_t i = 0; i < ipc_channel_count; i++) {
        if (strcmp(ipc_channels[i].name, name) == 0) {
            spin_unlock(&ipc_channel_lock);
            return -1;
        }
    }

    if (ipc_channel_count >= IPC_MAX_QUEUES) {
        spin_unlock(&ipc_channel_lock);
        return -1;
    }

    int id = (int)ipc_channel_count++;
    ipc_channel_t* ch = &ipc_channels[id];

    strncpy(ch->name, name, IPC_MAX_NAME - 1);
    ch->name[IPC_MAX_NAME - 1] = '\0';
    ch->owner = current_process;
    ch->creator = current_process;
    ch->ref_count = 1;
    ch->flags = 0;
    ch->max_msg_size = max_msg_size ? max_msg_size : IPC_MAX_SIZE;
    ch->max_msg_count = max_msg_count ? max_msg_count : IPC_MAX_MESSAGES;
    ch->queue.head = 0;
    ch->queue.tail = 0;
    ch->queue.count = 0;
    spin_init(&ch->queue.lock);

    spin_unlock(&ipc_channel_lock);
    return id;
}

int ipc_channel_open(const char* name)
{
    if (!name) return -1;

    spin_lock(&ipc_channel_lock);

    for (uint32_t i = 0; i < ipc_channel_count; i++) {
        if (strcmp(ipc_channels[i].name, name) == 0) {
            ipc_channels[i].ref_count++;
            spin_unlock(&ipc_channel_lock);
            return (int)i;
        }
    }

    spin_unlock(&ipc_channel_lock);
    return -1;
}

int ipc_channel_send(int channel, uint64_t type, const void* data, uint64_t size)
{
    if (channel < 0 || (uint32_t)channel >= ipc_channel_count) return -1;
    if (size > IPC_MAX_SIZE) return -1;

    ipc_channel_t* ch = &ipc_channels[channel];
    spin_lock(&ch->queue.lock);

    if (ch->queue.count >= IPC_MAX_MESSAGES) {
        spin_unlock(&ch->queue.lock);
        return -1;
    }

    ipc_message_t* msg = &ch->queue.messages[ch->queue.tail];
    msg->source = current_process;
    msg->destination = ch->owner;
    msg->type = type;
    msg->size = size;
    if (data && size > 0) {
        memcpy(msg->data, data, size);
    }

    ch->queue.tail = (ch->queue.tail + 1) % IPC_MAX_MESSAGES;
    ch->queue.count++;
    spin_unlock(&ch->queue.lock);

    process_wakeup(ch->owner);
    return 0;
}

int ipc_channel_recv(int channel, uint64_t* type, void* data, uint64_t* size)
{
    if (channel < 0 || (uint32_t)channel >= ipc_channel_count) return -1;

    ipc_channel_t* ch = &ipc_channels[channel];
    spin_lock(&ch->queue.lock);

    if (ch->queue.count == 0) {
        spin_unlock(&ch->queue.lock);
        return -1;
    }

    ipc_message_t* msg = &ch->queue.messages[ch->queue.head];
    if (type) *type = msg->type;
    if (data && size && *size > 0) {
        uint64_t copy_size = msg->size < *size ? msg->size : *size;
        memcpy(data, msg->data, copy_size);
        *size = copy_size;
    } else if (size) {
        *size = msg->size;
    }

    ch->queue.head = (ch->queue.head + 1) % IPC_MAX_MESSAGES;
    ch->queue.count--;
    spin_unlock(&ch->queue.lock);
    return 0;
}

int ipc_channel_close(int channel)
{
    if (channel < 0 || (uint32_t)channel >= ipc_channel_count) return -1;

    spin_lock(&ipc_channel_lock);
    ipc_channel_t* ch = &ipc_channels[channel];

    if (ch->ref_count > 0) {
        ch->ref_count--;
    }
    spin_unlock(&ipc_channel_lock);
    return 0;
}

int ipc_channel_destroy(int channel)
{
    if (channel < 0 || (uint32_t)channel >= ipc_channel_count) return -1;

    spin_lock(&ipc_channel_lock);
    ipc_channel_t* ch = &ipc_channels[channel];

    if (ch->owner != current_process) {
        spin_unlock(&ipc_channel_lock);
        return -1;
    }

    ch->name[0] = '\0';
    ch->owner = 0;
    ch->ref_count = 0;
    ch->queue.head = 0;
    ch->queue.tail = 0;
    ch->queue.count = 0;
    spin_unlock(&ipc_channel_lock);
    return 0;
}

int ipc_shm_create(uint64_t size)
{
    if (size == 0) return -1;

    spin_lock(&ipc_shm_lock);

    if (ipc_shm_count >= IPC_SHM_MAX) {
        spin_unlock(&ipc_shm_lock);
        return -1;
    }

    int id = (int)ipc_shm_count++;
    ipc_shm_t* shm = &ipc_shm_table[id];

    uint64_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    shm->addr = kzalloc(aligned_size);
    if (!shm->addr) {
        ipc_shm_count--;
        spin_unlock(&ipc_shm_lock);
        return -1;
    }

    shm->size = aligned_size;
    shm->owner = current_process;
    shm->ref_count = 1;
    spin_init(&shm->lock);

    spin_unlock(&ipc_shm_lock);
    return id;
}

void* ipc_shm_attach(int shmid)
{
    if (shmid < 0 || (uint32_t)shmid >= ipc_shm_count) return NULL;

    spin_lock(&ipc_shm_lock);
    ipc_shm_t* shm = &ipc_shm_table[shmid];

    shm->ref_count++;
    void* addr = shm->addr;
    spin_unlock(&ipc_shm_lock);

    return addr;
}

int ipc_shm_detach(int shmid)
{
    if (shmid < 0 || (uint32_t)shmid >= ipc_shm_count) return -1;

    spin_lock(&ipc_shm_lock);
    ipc_shm_t* shm = &ipc_shm_table[shmid];

    if (shm->ref_count > 0) {
        shm->ref_count--;
    }
    spin_unlock(&ipc_shm_lock);
    return 0;
}

int ipc_shm_destroy(int shmid)
{
    if (shmid < 0 || (uint32_t)shmid >= ipc_shm_count) return -1;

    spin_lock(&ipc_shm_lock);
    ipc_shm_t* shm = &ipc_shm_table[shmid];

    if (shm->owner != current_process) {
        spin_unlock(&ipc_shm_lock);
        return -1;
    }

    if (shm->addr) {
        kfree(shm->addr);
        shm->addr = NULL;
    }
    shm->size = 0;
    shm->ref_count = 0;
    spin_unlock(&ipc_shm_lock);
    return 0;
}