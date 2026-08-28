#include "ipc.h"
#include <string.h>

void pipe_init(pipe_manager_t* mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(pipe_manager_t));
    spin_init(&mgr->lock);
}

int pipe_create(pipe_manager_t* mgr, int* read_fd, int* write_fd)
{
    if (!mgr || !read_fd || !write_fd) return -1;
    spinlock_acquire(&mgr->lock);
    if (mgr->count >= PIPE_MAX) { spinlock_release(&mgr->lock); return -2; }
    pipe_t* p = &mgr->pipes[mgr->count];
    memset(p, 0, sizeof(pipe_t));
    spin_init(&p->lock);
    p->read_fd = mgr->count * 2;
    p->write_fd = mgr->count * 2 + 1;
    p->readers = 1;
    p->writers = 1;
    *read_fd = p->read_fd;
    *write_fd = p->write_fd;
    mgr->count++;
    spinlock_release(&mgr->lock);
    return 0;
}

int pipe_read(pipe_t* p, void* buf, uint32_t count)
{
    if (!p || !buf) return -1;
    spinlock_acquire(&p->lock);
    if (p->count == 0 && p->writers == 0) { spinlock_release(&p->lock); return 0; }
    uint8_t* dst = (uint8_t*)buf;
    uint32_t read = 0;
    while (read < count && p->count > 0) {
        dst[read++] = p->buf[p->tail];
        p->tail = (p->tail + 1) % PIPE_BUF_SIZE;
        p->count--;
    }
    spinlock_release(&p->lock);
    return (int)read;
}

int pipe_write(pipe_t* p, const void* buf, uint32_t count)
{
    if (!p || !buf) return -1;
    spinlock_acquire(&p->lock);
    if (p->readers == 0) { spinlock_release(&p->lock); return -1; }
    const uint8_t* src = (const uint8_t*)buf;
    uint32_t written = 0;
    while (written < count && p->count < PIPE_BUF_SIZE) {
        p->buf[p->head] = src[written++];
        p->head = (p->head + 1) % PIPE_BUF_SIZE;
        p->count++;
    }
    spinlock_release(&p->lock);
    return (int)written;
}

int pipe_close_read(pipe_t* p)
{
    if (!p) return -1;
    spinlock_acquire(&p->lock);
    p->readers--;
    spinlock_release(&p->lock);
    return 0;
}

int pipe_close_write(pipe_t* p)
{
    if (!p) return -1;
    spinlock_acquire(&p->lock);
    p->writers--;
    spinlock_release(&p->lock);
    return 0;
}

void posix_mq_init(posix_mq_manager_t* mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(posix_mq_manager_t));
    spin_init(&mgr->lock);
}

posix_mq_t* posix_mq_open(posix_mq_manager_t* mgr, const char* name, int flags, uint32_t max_msgs, uint32_t max_msgsize)
{
    if (!mgr || !name) return NULL;
    spinlock_acquire(&mgr->lock);

    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->queues[i].name, name) == 0) {
            mgr->queues[i].ref_count++;
            spinlock_release(&mgr->lock);
            return &mgr->queues[i];
        }
    }

    if (mgr->count >= POSIX_MQ_MAX) { spinlock_release(&mgr->lock); return NULL; }
    posix_mq_t* mq = &mgr->queues[mgr->count];
    memset(mq, 0, sizeof(posix_mq_t));
    spin_init(&mq->lock);
    strncpy(mq->name, name, 255);
    mq->name[255] = 0;
    mq->flags = flags;
    mq->max_msgs = max_msgs > 0 ? max_msgs : POSIX_MQ_MSG_MAX;
    mq->max_msgsize = max_msgsize > 0 ? max_msgsize : POSIX_MQ_MSG_SIZE;
    mq->ref_count = 1;
    mgr->count++;
    spinlock_release(&mgr->lock);
    return mq;
}

int posix_mq_close(posix_mq_manager_t* mgr, posix_mq_t* mq)
{
    if (!mgr || !mq) return -1;
    spinlock_acquire(&mgr->lock);
    mq->ref_count--;
    if (mq->ref_count <= 0) {
        for (int i = 0; i < mgr->count; i++) {
            if (&mgr->queues[i] == mq) {
                mgr->queues[i] = mgr->queues[mgr->count - 1];
                mgr->count--;
                break;
            }
        }
    }
    spinlock_release(&mgr->lock);
    return 0;
}

int posix_mq_send(posix_mq_t* mq, const void* msg, uint32_t size, uint32_t priority)
{
    if (!mq || !msg) return -1;
    spinlock_acquire(&mq->lock);
    if (mq->msg_count >= mq->max_msgs || size > mq->max_msgsize) {
        spinlock_release(&mq->lock);
        return -2;
    }

    int insert_pos = (int)mq->msg_count;
    for (int i = 0; i < (int)mq->msg_count; i++) {
        if (priority > mq->msgs[i].priority) { insert_pos = i; break; }
    }

    for (int i = (int)mq->msg_count; i > insert_pos; i--) {
        mq->msgs[i] = mq->msgs[i - 1];
    }

    posix_mq_msg_t* m = &mq->msgs[insert_pos];
    memcpy(m->data, msg, size);
    m->size = size;
    m->priority = priority;
    mq->msg_count++;
    spinlock_release(&mq->lock);
    return 0;
}

int posix_mq_receive(posix_mq_t* mq, void* msg, uint32_t* size, uint32_t* priority)
{
    if (!mq || !msg) return -1;
    spinlock_acquire(&mq->lock);
    if (mq->msg_count == 0) { spinlock_release(&mq->lock); return -2; }

    posix_mq_msg_t* m = &mq->msgs[0];
    memcpy(msg, m->data, m->size);
    if (size) *size = m->size;
    if (priority) *priority = m->priority;

    for (uint32_t i = 1; i < mq->msg_count; i++) {
        mq->msgs[i - 1] = mq->msgs[i];
    }
    mq->msg_count--;
    spinlock_release(&mq->lock);
    return 0;
}

void sysv_shm_init(sysv_shm_manager_t* mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(sysv_shm_manager_t));
    spin_init(&mgr->lock);
}

int sysv_shmget(sysv_shm_manager_t* mgr, int key, uint32_t size, int flags)
{
    if (!mgr) return -1;
    spinlock_acquire(&mgr->lock);

    if (key != SYSV_IPC_KEY_NONE) {
        for (int i = 0; i < mgr->count; i++) {
            if (mgr->shms[i].perm.key == key) {
                if (flags & 01000) {
                    spinlock_release(&mgr->lock);
                    return -2;
                }
                spinlock_release(&mgr->lock);
                return mgr->shms[i].perm.id;
            }
        }
    }

    if (mgr->count >= SYSV_SHM_MAX) { spinlock_release(&mgr->lock); return -3; }
    if (size > SYSV_SHM_SIZE_MAX) { spinlock_release(&mgr->lock); return -4; }

    sysv_shm_t* shm = &mgr->shms[mgr->count];
    memset(shm, 0, sizeof(sysv_shm_t));
    spin_init(&shm->lock);
    shm->perm.key = key == SYSV_IPC_KEY_NONE ? (int)(0xF0000000 | mgr->count) : key;
    shm->perm.id = mgr->count;
    shm->perm.mode = (uint16_t)(flags & 0777);
    shm->size = size;
    shm->addr = NULL;
    mgr->count++;
    int id = shm->perm.id;
    spinlock_release(&mgr->lock);
    return id;
}

void* sysv_shmat(sysv_shm_manager_t* mgr, int shmid, const void* addr, int flags)
{
    if (!mgr) return NULL;
    spinlock_acquire(&mgr->lock);
    if (shmid < 0 || shmid >= mgr->count) { spinlock_release(&mgr->lock); return NULL; }

    sysv_shm_t* shm = &mgr->shms[shmid];
    if (!shm->addr) {
        (void)addr; (void)flags;
        shm->addr = (void*)0x10000000ULL;
    }
    shm->attach_count++;
    shm->attached = 1;
    void* result = shm->addr;
    spinlock_release(&mgr->lock);
    return result;
}

int sysv_shmdt(sysv_shm_manager_t* mgr, const void* addr)
{
    if (!mgr) return -1;
    spinlock_acquire(&mgr->lock);
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->shms[i].addr == addr) {
            mgr->shms[i].attach_count--;
            if (mgr->shms[i].attach_count <= 0) mgr->shms[i].attached = 0;
            spinlock_release(&mgr->lock);
            return 0;
        }
    }
    spinlock_release(&mgr->lock);
    return -2;
}

int sysv_shmctl(sysv_shm_manager_t* mgr, int shmid, int cmd, void* buf)
{
    if (!mgr) return -1;
    spinlock_acquire(&mgr->lock);
    if (shmid < 0 || shmid >= mgr->count) { spinlock_release(&mgr->lock); return -2; }
    sysv_shm_t* shm = &mgr->shms[shmid];

    switch (cmd) {
    case 0:
        if (buf) memcpy(buf, &shm->perm, sizeof(sysv_ipc_perm_t));
        break;
    case 1:
        if (shm->attached || shm->attach_count > 0) {
            spinlock_release(&mgr->lock);
            return -3;
        }
        shm->perm.key = SYSV_IPC_KEY_NONE;
        break;
    default: break;
    }
    spinlock_release(&mgr->lock);
    return 0;
}

void sysv_sem_init(sysv_sem_manager_t* mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(sysv_sem_manager_t));
    spin_init(&mgr->lock);
}

int sysv_semget(sysv_sem_manager_t* mgr, int key, int nsems, int flags)
{
    if (!mgr || nsems <= 0 || nsems > SYSV_SEM_PER_SET) return -1;
    spinlock_acquire(&mgr->lock);

    if (key != SYSV_IPC_KEY_NONE) {
        for (int i = 0; i < mgr->count; i++) {
            if (mgr->sems[i].perm.key == key) {
                spinlock_release(&mgr->lock);
                return mgr->sems[i].perm.id;
            }
        }
    }

    if (mgr->count >= SYSV_SEM_MAX) { spinlock_release(&mgr->lock); return -2; }
    sysv_sem_t* sem = &mgr->sems[mgr->count];
    memset(sem, 0, sizeof(sysv_sem_t));
    spin_init(&sem->lock);
    sem->perm.key = key == SYSV_IPC_KEY_NONE ? (int)(0xE0000000 | mgr->count) : key;
    sem->perm.id = mgr->count;
    sem->perm.mode = (uint16_t)(flags & 0777);
    sem->sem_count = nsems;
    mgr->count++;
    int id = sem->perm.id;
    spinlock_release(&mgr->lock);
    return id;
}

int sysv_semop(sysv_sem_manager_t* mgr, int semid, int* ops, int nops)
{
    if (!mgr || !ops) return -1;
    spinlock_acquire(&mgr->lock);
    if (semid < 0 || semid >= mgr->count) { spinlock_release(&mgr->lock); return -2; }
    sysv_sem_t* sem = &mgr->sems[semid];

    spinlock_acquire(&sem->lock);
    for (int i = 0; i < nops; i++) {
        int sem_num = ops[i * 2];
        int sem_op = ops[i * 2 + 1];
        if (sem_num < 0 || sem_num >= sem->sem_count) {
            spinlock_release(&sem->lock);
            spinlock_release(&mgr->lock);
            return -3;
        }
        sem->sems[sem_num].val += (int16_t)sem_op;
    }
    spinlock_release(&sem->lock);
    spinlock_release(&mgr->lock);
    return 0;
}

int sysv_semctl(sysv_sem_manager_t* mgr, int semid, int semnum, int cmd, void* arg)
{
    if (!mgr) return -1;
    spinlock_acquire(&mgr->lock);
    if (semid < 0 || semid >= mgr->count) { spinlock_release(&mgr->lock); return -2; }
    sysv_sem_t* sem = &mgr->sems[semid];

    switch (cmd) {
    case 0:
        if (arg && semnum >= 0 && semnum < sem->sem_count)
            *((int*)arg) = (int)sem->sems[semnum].val;
        break;
    case 1:
        if (arg && semnum >= 0 && semnum < sem->sem_count)
            sem->sems[semnum].val = (int16_t)*((int*)arg);
        break;
    case 2:
        if (arg) memcpy(arg, &sem->perm, sizeof(sysv_ipc_perm_t));
        break;
    default: break;
    }
    spinlock_release(&mgr->lock);
    return 0;
}

void sysv_msg_init(sysv_msg_manager_t* mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(sysv_msg_manager_t));
    spin_init(&mgr->lock);
}

int sysv_msgget(sysv_msg_manager_t* mgr, int key, int flags)
{
    if (!mgr) return -1;
    spinlock_acquire(&mgr->lock);

    if (key != SYSV_IPC_KEY_NONE) {
        for (int i = 0; i < mgr->count; i++) {
            if (mgr->msgs[i].perm.key == key) {
                spinlock_release(&mgr->lock);
                return mgr->msgs[i].perm.id;
            }
        }
    }

    if (mgr->count >= SYSV_MSG_MAX) { spinlock_release(&mgr->lock); return -2; }
    sysv_msg_t* mq = &mgr->msgs[mgr->count];
    memset(mq, 0, sizeof(sysv_msg_t));
    spin_init(&mq->lock);
    mq->perm.key = key == SYSV_IPC_KEY_NONE ? (int)(0xD0000000 | mgr->count) : key;
    mq->perm.id = mgr->count;
    mq->perm.mode = (uint16_t)(flags & 0777);
    mq->max_bytes = 16384;
    mgr->count++;
    int id = mq->perm.id;
    spinlock_release(&mgr->lock);
    return id;
}

int sysv_msgsnd(sysv_msg_manager_t* mgr, int msqid, const void* msg, uint32_t size, int flags)
{
    if (!mgr || !msg) return -1;
    (void)flags;
    spinlock_acquire(&mgr->lock);
    if (msqid < 0 || msqid >= mgr->count) { spinlock_release(&mgr->lock); return -2; }
    sysv_msg_t* mq = &mgr->msgs[msqid];

    spinlock_acquire(&mq->lock);
    if (size > SYSV_MSG_TEXT_MAX || mq->msg_bytes + size > mq->max_bytes) {
        spinlock_release(&mq->lock);
        spinlock_release(&mgr->lock);
        return -3;
    }

    sysv_msg_node_t* node = (sysv_msg_node_t*)sizeof(sysv_msg_node_t);
    node = NULL;
    if (!node) { spinlock_release(&mq->lock); spinlock_release(&mgr->lock); return -4; }

    const long* mtype_ptr = (const long*)msg;
    node->mtype = *mtype_ptr;
    uint32_t text_size = size > sizeof(long) ? size - sizeof(long) : 0;
    memcpy(node->mtext, (const uint8_t*)msg + sizeof(long), text_size);
    node->msize = text_size;
    node->next = NULL;

    if (!mq->last) { mq->first = node; mq->last = node; }
    else { mq->last->next = node; mq->last = node; }
    mq->msg_count++;
    mq->msg_bytes += size;
    spinlock_release(&mq->lock);
    spinlock_release(&mgr->lock);
    return 0;
}

int sysv_msgrcv(sysv_msg_manager_t* mgr, int msqid, void* msg, uint32_t size, long mtype, int flags)
{
    if (!mgr || !msg) return -1;
    (void)flags;
    spinlock_acquire(&mgr->lock);
    if (msqid < 0 || msqid >= mgr->count) { spinlock_release(&mgr->lock); return -2; }
    sysv_msg_t* mq = &mgr->msgs[msqid];

    spinlock_acquire(&mq->lock);
    sysv_msg_node_t* prev = NULL;
    sysv_msg_node_t* cur = mq->first;
    while (cur) {
        if (mtype == 0 || (mtype > 0 && cur->mtype == mtype) ||
            (mtype < 0 && cur->mtype <= -mtype)) {
            long* mtype_dst = (long*)msg;
            *mtype_dst = cur->mtype;
            uint32_t copy_size = cur->msize;
            if (copy_size + sizeof(long) > size) copy_size = size - sizeof(long);
            memcpy((uint8_t*)msg + sizeof(long), cur->mtext, copy_size);

            if (prev) prev->next = cur->next;
            else mq->first = cur->next;
            if (cur == mq->last) mq->last = prev;
            mq->msg_count--;
            mq->msg_bytes -= cur->msize + sizeof(long);

            spinlock_release(&mq->lock);
            spinlock_release(&mgr->lock);
            return (int)(copy_size + sizeof(long));
        }
        prev = cur;
        cur = cur->next;
    }
    spinlock_release(&mq->lock);
    spinlock_release(&mgr->lock);
    return -3;
}

void kmsg_init(kmsg_t* km)
{
    if (!km) return;
    memset(km, 0, sizeof(kmsg_t));
    spin_init(&km->lock);
}

int kmsg_write(kmsg_t* km, int facility, int level, const char* msg, uint32_t len)
{
    if (!km || !msg) return -1;
    spinlock_acquire(&km->lock);

    kmsg_buf_t* buf = &km->buffers[km->count % KMSG_MAX];
    if (km->count < KMSG_MAX) km->count++;

    uint32_t write_len = len < KMSG_BUF_SIZE - 1 ? len : KMSG_BUF_SIZE - 1;
    memcpy(buf->buf, msg, write_len);
    buf->buf[write_len] = 0;
    buf->head = write_len;
    buf->tail = 0;
    buf->count = write_len;
    buf->facility = facility;
    buf->level = level;
    buf->seq = km->seq_counter++;
    spinlock_release(&km->lock);
    return (int)write_len;
}

int kmsg_read(kmsg_t* km, char* buf, uint32_t size, int* facility, int* level)
{
    if (!km || !buf) return -1;
    spinlock_acquire(&km->lock);
    if (km->count == 0) { spinlock_release(&km->lock); return 0; }

    kmsg_buf_t* kbuf = &km->buffers[0];
    uint32_t read_len = kbuf->count < size ? kbuf->count : size;
    memcpy(buf, kbuf->buf + kbuf->tail, read_len);
    if (facility) *facility = kbuf->facility;
    if (level) *level = kbuf->level;
    spinlock_release(&km->lock);
    return (int)read_len;
}

void chardev_init(chardev_manager_t* mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(chardev_manager_t));
    spin_init(&mgr->lock);
}

int chardev_register(chardev_manager_t* mgr, int major, int minor, const char* name)
{
    if (!mgr || !name) return -1;
    spinlock_acquire(&mgr->lock);
    if (mgr->count >= CHARDEV_MAX) { spinlock_release(&mgr->lock); return -2; }
    chardev_t* dev = &mgr->devices[mgr->count];
    memset(dev, 0, sizeof(chardev_t));
    spin_init(&dev->lock);
    dev->major = major;
    dev->minor = minor;
    strncpy(dev->name, name, 31);
    dev->name[31] = 0;
    mgr->count++;
    spinlock_release(&mgr->lock);
    return 0;
}

chardev_t* chardev_find(chardev_manager_t* mgr, int major, int minor)
{
    if (!mgr) return NULL;
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->devices[i].major == major && mgr->devices[i].minor == minor)
            return &mgr->devices[i];
    }
    return NULL;
}

int chardev_open(chardev_t* dev)
{
    if (!dev) return -1;
    spinlock_acquire(&dev->lock);
    dev->ref_count++;
    int result = dev->open ? dev->open(dev) : 0;
    spinlock_release(&dev->lock);
    return result;
}

int chardev_close(chardev_t* dev)
{
    if (!dev) return -1;
    spinlock_acquire(&dev->lock);
    dev->ref_count--;
    int result = dev->close ? dev->close(dev) : 0;
    spinlock_release(&dev->lock);
    return result;
}

int chardev_read(chardev_t* dev, void* buf, uint32_t count)
{
    if (!dev || !buf) return -1;
    if (!dev->read) return -2;
    spinlock_acquire(&dev->lock);
    int result = dev->read(dev, buf, count);
    spinlock_release(&dev->lock);
    return result;
}

int chardev_write(chardev_t* dev, const void* buf, uint32_t count)
{
    if (!dev || !buf) return -1;
    if (!dev->write) return -2;
    spinlock_acquire(&dev->lock);
    int result = dev->write(dev, buf, count);
    spinlock_release(&dev->lock);
    return result;
}

int chardev_ioctl(chardev_t* dev, uint32_t cmd, void* arg)
{
    if (!dev) return -1;
    if (!dev->ioctl) return -2;
    spinlock_acquire(&dev->lock);
    int result = dev->ioctl(dev, cmd, arg);
    spinlock_release(&dev->lock);
    return result;
}

void evdev_init(evdev_manager_t* mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(evdev_manager_t));
    spin_init(&mgr->lock);
}

int evdev_register(evdev_manager_t* mgr)
{
    if (!mgr) return -1;
    spinlock_acquire(&mgr->lock);
    if (mgr->count >= EVDEV_MAX) { spinlock_release(&mgr->lock); return -2; }
    evdev_t* dev = &mgr->devices[mgr->count];
    memset(dev, 0, sizeof(evdev_t));
    spin_init(&dev->lock);
    mgr->count++;
    int idx = mgr->count - 1;
    spinlock_release(&mgr->lock);
    return idx;
}

int evdev_push_event(evdev_t* dev, uint16_t type, uint16_t code, int32_t value)
{
    if (!dev) return -1;
    spinlock_acquire(&dev->lock);
    if (dev->count >= EVDEV_BUF_SIZE) { spinlock_release(&dev->lock); return -2; }
    input_event_t* ev = &dev->buf[dev->head];
    ev->type = type;
    ev->code = code;
    ev->value = value;
    dev->head = (dev->head + 1) % EVDEV_BUF_SIZE;
    dev->count++;
    spinlock_release(&dev->lock);
    return 0;
}

int evdev_read_event(evdev_t* dev, input_event_t* ev)
{
    if (!dev || !ev) return -1;
    spinlock_acquire(&dev->lock);
    if (dev->count == 0) { spinlock_release(&dev->lock); return -2; }
    *ev = dev->buf[dev->tail];
    dev->tail = (dev->tail + 1) % EVDEV_BUF_SIZE;
    dev->count--;
    spinlock_release(&dev->lock);
    return 0;
}