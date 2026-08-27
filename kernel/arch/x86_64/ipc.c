#include <arch/ipc.h>
#include <arch/memory.h>
#include <arch/process.h>
#include <string.h>

static ipc_channel_t ipc_channels[IPC_MAX_QUEUES];
static ipc_shm_t ipc_shms[IPC_SHM_MAX];
static spinlock_t ipc_global_lock = SPINLOCK_INIT;
static int ipc_initialized = 0;

void ipc_init(void)
{
    memset(ipc_channels, 0, sizeof(ipc_channels));
    memset(ipc_shms, 0, sizeof(ipc_shms));
    for (int i = 0; i < IPC_MAX_QUEUES; i++) {
        ipc_channels[i].ref_count = 0;
        ipc_channels[i].queue.head = 0;
        ipc_channels[i].queue.tail = 0;
        ipc_channels[i].queue.count = 0;
        ipc_channels[i].queue.lock = SPINLOCK_INIT;
    }
    ipc_initialized = 1;
}

static int ipc_find_channel(const char* name)
{
    for (int i = 0; i < IPC_MAX_QUEUES; i++) {
        if (ipc_channels[i].ref_count > 0 &&
            strncmp(ipc_channels[i].name, name, IPC_MAX_NAME) == 0) {
            return i;
        }
    }
    return -1;
}

static int ipc_alloc_channel(void)
{
    for (int i = 0; i < IPC_MAX_QUEUES; i++) {
        if (ipc_channels[i].ref_count == 0) return i;
    }
    return -1;
}

int ipc_send(uint64_t destination, uint64_t type, const void* data, uint64_t size)
{
    if (!ipc_initialized || !data || size > IPC_MAX_SIZE) return -1;

    spinlock_acquire(&ipc_global_lock);

    for (int i = 0; i < IPC_MAX_QUEUES; i++) {
        ipc_channel_t* ch = &ipc_channels[i];
        if (ch->ref_count > 0 && ch->owner == destination) {
            ipc_queue_t* q = &ch->queue;
            spinlock_acquire(&q->lock);

            if (q->count >= IPC_MAX_MESSAGES) {
                spinlock_release(&q->lock);
                spinlock_release(&ipc_global_lock);
                return -2;
            }

            ipc_message_t* msg = &q->messages[q->tail];
            msg->source = process_get_current_id();
            msg->destination = destination;
            msg->type = type;
            msg->size = size;
            memcpy(msg->data, data, size);

            q->tail = (q->tail + 1) % IPC_MAX_MESSAGES;
            q->count++;

            spinlock_release(&q->lock);
            spinlock_release(&ipc_global_lock);
            return 0;
        }
    }

    spinlock_release(&ipc_global_lock);
    return -1;
}

int ipc_receive(uint64_t source, uint64_t* type, void* data, uint64_t* size)
{
    if (!ipc_initialized) return -1;

    uint64_t my_id = process_get_current_id();

    spinlock_acquire(&ipc_global_lock);

    for (int i = 0; i < IPC_MAX_QUEUES; i++) {
        ipc_channel_t* ch = &ipc_channels[i];
        if (ch->ref_count > 0 && ch->owner == my_id) {
            ipc_queue_t* q = &ch->queue;
            spinlock_acquire(&q->lock);

            if (q->count == 0) {
                spinlock_release(&q->lock);
                spinlock_release(&ipc_global_lock);
                return -2;
            }

            ipc_message_t* msg = &q->messages[q->head];
            if (source != 0 && msg->source != source) {
                spinlock_release(&q->lock);
                spinlock_release(&ipc_global_lock);
                return -3;
            }

            if (type) *type = msg->type;
            if (size) *size = msg->size;
            if (data) memcpy(data, msg->data, msg->size);

            q->head = (q->head + 1) % IPC_MAX_MESSAGES;
            q->count--;

            spinlock_release(&q->lock);
            spinlock_release(&ipc_global_lock);
            return 0;
        }
    }

    spinlock_release(&ipc_global_lock);
    return -1;
}

int ipc_channel_create(const char* name, uint64_t max_msg_size, uint64_t max_msg_count)
{
    if (!ipc_initialized || !name) return -1;

    spinlock_acquire(&ipc_global_lock);

    if (ipc_find_channel(name) >= 0) {
        spinlock_release(&ipc_global_lock);
        return -2;
    }

    int idx = ipc_alloc_channel();
    if (idx < 0) {
        spinlock_release(&ipc_global_lock);
        return -3;
    }

    ipc_channel_t* ch = &ipc_channels[idx];
    strncpy(ch->name, name, IPC_MAX_NAME - 1);
    ch->name[IPC_MAX_NAME - 1] = '\0';
    ch->owner = process_get_current_id();
    ch->creator = ch->owner;
    ch->ref_count = 1;
    ch->flags = 0;
    ch->max_msg_size = max_msg_size ? max_msg_size : IPC_MAX_SIZE;
    ch->max_msg_count = max_msg_count ? max_msg_count : IPC_MAX_MESSAGES;
    ch->queue.head = 0;
    ch->queue.tail = 0;
    ch->queue.count = 0;

    spinlock_release(&ipc_global_lock);
    return idx;
}

int ipc_channel_open(const char* name)
{
    if (!ipc_initialized || !name) return -1;

    spinlock_acquire(&ipc_global_lock);
    int idx = ipc_find_channel(name);
    if (idx < 0) {
        spinlock_release(&ipc_global_lock);
        return -2;
    }
    ipc_channels[idx].ref_count++;
    spinlock_release(&ipc_global_lock);
    return idx;
}

int ipc_channel_send(int channel, uint64_t type, const void* data, uint64_t size)
{
    if (!ipc_initialized || channel < 0 || channel >= IPC_MAX_QUEUES) return -1;
    if (!data || size > IPC_MAX_SIZE) return -2;

    ipc_channel_t* ch = &ipc_channels[channel];
    if (ch->ref_count == 0) return -3;

    ipc_queue_t* q = &ch->queue;
    spinlock_acquire(&q->lock);

    if (q->count >= ch->max_msg_count) {
        spinlock_release(&q->lock);
        return -4;
    }

    ipc_message_t* msg = &q->messages[q->tail];
    msg->source = process_get_current_id();
    msg->destination = ch->owner;
    msg->type = type;
    msg->size = size > ch->max_msg_size ? ch->max_msg_size : size;
    memcpy(msg->data, data, msg->size);

    q->tail = (q->tail + 1) % IPC_MAX_MESSAGES;
    q->count++;

    spinlock_release(&q->lock);
    return 0;
}

int ipc_channel_recv(int channel, uint64_t* type, void* data, uint64_t* size)
{
    if (!ipc_initialized || channel < 0 || channel >= IPC_MAX_QUEUES) return -1;

    ipc_channel_t* ch = &ipc_channels[channel];
    if (ch->ref_count == 0) return -2;

    ipc_queue_t* q = &ch->queue;
    spinlock_acquire(&q->lock);

    if (q->count == 0) {
        spinlock_release(&q->lock);
        return -3;
    }

    ipc_message_t* msg = &q->messages[q->head];
    if (type) *type = msg->type;
    if (size) *size = msg->size;
    if (data) memcpy(data, msg->data, msg->size);

    q->head = (q->head + 1) % IPC_MAX_MESSAGES;
    q->count--;

    spinlock_release(&q->lock);
    return 0;
}

int ipc_channel_close(int channel)
{
    if (!ipc_initialized || channel < 0 || channel >= IPC_MAX_QUEUES) return -1;

    spinlock_acquire(&ipc_global_lock);
    ipc_channel_t* ch = &ipc_channels[channel];
    if (ch->ref_count == 0) {
        spinlock_release(&ipc_global_lock);
        return -2;
    }
    ch->ref_count--;
    spinlock_release(&ipc_global_lock);
    return 0;
}

int ipc_channel_destroy(int channel)
{
    if (!ipc_initialized || channel < 0 || channel >= IPC_MAX_QUEUES) return -1;

    spinlock_acquire(&ipc_global_lock);
    ipc_channel_t* ch = &ipc_channels[channel];

    if (ch->creator != process_get_current_id()) {
        spinlock_release(&ipc_global_lock);
        return -2;
    }

    ch->ref_count = 0;
    ch->name[0] = '\0';
    ch->queue.head = 0;
    ch->queue.tail = 0;
    ch->queue.count = 0;

    spinlock_release(&ipc_global_lock);
    return 0;
}

int ipc_shm_create(uint64_t size)
{
    if (!ipc_initialized || size == 0) return -1;

    spinlock_acquire(&ipc_global_lock);

    int idx = -1;
    for (int i = 0; i < IPC_SHM_MAX; i++) {
        if (ipc_shms[i].ref_count == 0) { idx = i; break; }
    }
    if (idx < 0) {
        spinlock_release(&ipc_global_lock);
        return -2;
    }

    ipc_shm_t* shm = &ipc_shms[idx];
    shm->addr = memory_alloc(size);
    if (!shm->addr) {
        spinlock_release(&ipc_global_lock);
        return -3;
    }
    shm->size = size;
    shm->owner = process_get_current_id();
    shm->ref_count = 1;
    shm->lock = SPINLOCK_INIT;

    spinlock_release(&ipc_global_lock);
    return idx;
}

void* ipc_shm_attach(int shmid)
{
    if (!ipc_initialized || shmid < 0 || shmid >= IPC_SHM_MAX) return NULL;

    spinlock_acquire(&ipc_global_lock);
    ipc_shm_t* shm = &ipc_shms[shmid];
    if (shm->ref_count == 0) {
        spinlock_release(&ipc_global_lock);
        return NULL;
    }
    shm->ref_count++;
    void* addr = shm->addr;
    spinlock_release(&ipc_global_lock);
    return addr;
}

int ipc_shm_detach(int shmid)
{
    if (!ipc_initialized || shmid < 0 || shmid >= IPC_SHM_MAX) return -1;

    spinlock_acquire(&ipc_global_lock);
    ipc_shm_t* shm = &ipc_shms[shmid];
    if (shm->ref_count == 0) {
        spinlock_release(&ipc_global_lock);
        return -2;
    }
    shm->ref_count--;
    spinlock_release(&ipc_global_lock);
    return 0;
}

int ipc_shm_destroy(int shmid)
{
    if (!ipc_initialized || shmid < 0 || shmid >= IPC_SHM_MAX) return -1;

    spinlock_acquire(&ipc_global_lock);
    ipc_shm_t* shm = &ipc_shms[shmid];

    if (shm->owner != process_get_current_id()) {
        spinlock_release(&ipc_global_lock);
        return -2;
    }

    if (shm->addr) {
        memory_free(shm->addr);
    }
    memset(shm, 0, sizeof(ipc_shm_t));

    spinlock_release(&ipc_global_lock);
    return 0;
}