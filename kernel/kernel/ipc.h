

#ifndef KERNEL_IPC_H
#define KERNEL_IPC_H

#include <arch/types.h>
#include <arch/spinlock.h>

typedef uint64_t ipc_key_t;

#define IPC_PRIVATE  0

typedef struct ipc_perm {
    uint32_t uid;
    uint32_t gid;
    uint32_t cuid;
    uint32_t cgid;
    uint16_t mode;
} ipc_perm_t;

#define SHM_MAX        64
#define SHM_NAME_MAX   64
#define SHM_MIN_SIZE   4096
#define SHM_MAX_MAPPERS 64

typedef struct shm_segment {
    uint32_t     id;
    ipc_key_t    key;
    ipc_perm_t   perm;
    void*        phys_addr;
    uint64_t     size;
    uint32_t     ref_count;
    uint32_t     creator_pid;
    uint32_t     flags;
    char         name[SHM_NAME_MAX];
    void*        mapped_vaddr[SHM_MAX_MAPPERS];
} shm_segment_t;

#define IPC_CREAT  01000
#define IPC_EXCL   02000
#define SHM_RDONLY 0100
#define SHM_RND     0200
#define SHM_DEST    01000

int shm_init(void);
int shm_get(ipc_key_t key, uint64_t size, int flags, uint32_t* shm_id);
void* shm_at(uint32_t shm_id, void* addr, int flags);
int shm_dt(void* addr);
int shm_ctl(uint32_t shm_id, int cmd, void* buf);

#define MSG_MAX        64
#define MSG_TYPES_MAX  64
#define MSG_TEXT_MAX    4096

typedef struct msg_queue {
    uint32_t     id;
    ipc_key_t    key;
    ipc_perm_t   perm;
    uint32_t     msg_count;
    uint64_t     current_bytes;
    uint64_t     max_bytes;
    uint32_t     creator_pid;
} msg_queue_t;

typedef struct msg_entry {
    struct msg_entry* next;
    long     mtype;
    uint64_t msize;
    uint8_t  mtext[MSG_TEXT_MAX];
} msg_entry_t;

int msg_init(void);
int msg_get(ipc_key_t key, int flags, uint32_t* msg_id);
int msg_send(uint32_t msg_id, const void* msgp, uint64_t size, int flags);
int msg_recv(uint32_t msg_id, void* msgp, uint64_t size, long mtype, int flags);
int msg_ctl(uint32_t msg_id, int cmd, void* buf);

#define SEM_MAX        64
#define SEM_SET_MAX    16

typedef struct sem_set {
    uint32_t     id;
    ipc_key_t    key;
    ipc_perm_t   perm;
    uint16_t     nsems;
    int16_t      values[SEM_SET_MAX];
    uint32_t     creator_pid;
} sem_set_t;

struct sembuf {
    uint16_t sem_num;
    int16_t  sem_op;
    int16_t  sem_flg;
};

int sem_init(void);
int sem_get(ipc_key_t key, int nsems, int flags, uint32_t* sem_id);
int sem_op(uint32_t sem_id, struct sembuf* ops, uint32_t nops);
int sem_ctl(uint32_t sem_id, int semnum, int cmd, void* arg);

#define SEM_UNDO  0x1000

#define FUTEX_WAIT    0
#define FUTEX_WAKE    1
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4

typedef struct futex_key {
    void*    uaddr;
    uint32_t futex_word;
} futex_key_t;

#define FUTEX_MAX_QUEUES  128

int futex_wait(void* uaddr, uint32_t val, uint64_t timeout_ms);
int futex_wake(void* uaddr, uint32_t count);
int futex_requeue(void* uaddr, void* uaddr2, uint32_t count);

long ipc_syscall(long call, long first, long second, long third, long fourth, long fifth);

#endif
