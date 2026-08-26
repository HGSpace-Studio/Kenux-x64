#ifndef ARCH_X86_64_IPC_H
#define ARCH_X86_64_IPC_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define IPC_MAX_MESSAGES 64
#define IPC_MAX_SIZE 256
#define IPC_MAX_QUEUES 32
#define IPC_MAX_NAME 32

#define IPC_TYPE_RAW    0
#define IPC_TYPE_SIGNAL 1
#define IPC_TYPE_SHARED 2
#define IPC_TYPE_RPC    3

typedef struct {
    uint64_t source;
    uint64_t destination;
    uint64_t type;
    uint64_t size;
    uint8_t data[IPC_MAX_SIZE];
} ipc_message_t;

typedef struct {
    ipc_message_t messages[IPC_MAX_MESSAGES];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    spinlock_t lock;
} ipc_queue_t;

typedef struct {
    char name[IPC_MAX_NAME];
    uint64_t owner;
    uint64_t creator;
    ipc_queue_t queue;
    uint32_t ref_count;
    uint32_t flags;
    uint64_t max_msg_size;
    uint64_t max_msg_count;
} ipc_channel_t;

typedef struct {
    void* addr;
    uint64_t size;
    uint64_t owner;
    uint32_t ref_count;
    spinlock_t lock;
} ipc_shm_t;

#define IPC_SHM_MAX 16

void ipc_init(void);
int ipc_send(uint64_t destination, uint64_t type, const void* data, uint64_t size);
int ipc_receive(uint64_t source, uint64_t* type, void* data, uint64_t* size);

int ipc_channel_create(const char* name, uint64_t max_msg_size, uint64_t max_msg_count);
int ipc_channel_open(const char* name);
int ipc_channel_send(int channel, uint64_t type, const void* data, uint64_t size);
int ipc_channel_recv(int channel, uint64_t* type, void* data, uint64_t* size);
int ipc_channel_close(int channel);
int ipc_channel_destroy(int channel);

int ipc_shm_create(uint64_t size);
void* ipc_shm_attach(int shmid);
int ipc_shm_detach(int shmid);
int ipc_shm_destroy(int shmid);

#endif