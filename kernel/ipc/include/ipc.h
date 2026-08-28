#ifndef KERNEL_IPC_H
#define KERNEL_IPC_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define PIPE_BUF_SIZE    4096
#define PIPE_MAX         1024

typedef struct {
    uint8_t   buf[PIPE_BUF_SIZE];
    uint32_t  head;
    uint32_t  tail;
    uint32_t  count;
    int       read_fd;
    int       write_fd;
    int       readers;
    int       writers;
    int       nonblocking;
    spinlock_t lock;
} pipe_t;

typedef struct {
    pipe_t    pipes[PIPE_MAX];
    int       count;
    spinlock_t lock;
} pipe_manager_t;

void    pipe_init(pipe_manager_t* mgr);
int     pipe_create(pipe_manager_t* mgr, int* read_fd, int* write_fd);
int     pipe_read(pipe_t* p, void* buf, uint32_t count);
int     pipe_write(pipe_t* p, const void* buf, uint32_t count);
int     pipe_close_read(pipe_t* p);
int     pipe_close_write(pipe_t* p);

#define POSIX_MQ_MAX           64
#define POSIX_MQ_MSG_MAX       128
#define POSIX_MQ_MSG_SIZE      8192
#define POSIX_MQ_PRIO_MAX      32

typedef struct {
    uint8_t   data[POSIX_MQ_MSG_SIZE];
    uint32_t  size;
    uint32_t  priority;
} posix_mq_msg_t;

typedef struct {
    char            name[256];
    posix_mq_msg_t  msgs[POSIX_MQ_MSG_MAX];
    uint32_t        msg_count;
    uint32_t        max_msgs;
    uint32_t        max_msgsize;
    int             flags;
    int             ref_count;
    spinlock_t      lock;
} posix_mq_t;

typedef struct {
    posix_mq_t queues[POSIX_MQ_MAX];
    int        count;
    spinlock_t lock;
} posix_mq_manager_t;

void         posix_mq_init(posix_mq_manager_t* mgr);
posix_mq_t*  posix_mq_open(posix_mq_manager_t* mgr, const char* name, int flags, uint32_t max_msgs, uint32_t max_msgsize);
int          posix_mq_close(posix_mq_manager_t* mgr, posix_mq_t* mq);
int          posix_mq_send(posix_mq_t* mq, const void* msg, uint32_t size, uint32_t priority);
int          posix_mq_receive(posix_mq_t* mq, void* msg, uint32_t* size, uint32_t* priority);

#define SYSV_IPC_MAX_IDS       64
#define SYSV_IPC_KEY_NONE      0

typedef struct {
    int   key;
    int   id;
    uid_t uid;
    gid_t gid;
    uint16_t mode;
    int   ref_count;
} sysv_ipc_perm_t;

#define SYSV_SHM_MAX          64
#define SYSV_SHM_SIZE_MAX     (4 * 1024 * 1024)

typedef struct {
    sysv_ipc_perm_t perm;
    void*           addr;
    uint32_t        size;
    int             attached;
    int             attach_count;
    spinlock_t      lock;
} sysv_shm_t;

typedef struct {
    sysv_shm_t shms[SYSV_SHM_MAX];
    int        count;
    spinlock_t lock;
} sysv_shm_manager_t;

#define SYSV_SEM_MAX          64
#define SYSV_SEM_PER_SET      16

typedef struct {
    int16_t val;
    int16_t semadj;
    uint16_t semncnt;
    uint16_t semzcnt;
} sysv_sem_val_t;

typedef struct {
    sysv_ipc_perm_t  perm;
    sysv_sem_val_t   sems[SYSV_SEM_PER_SET];
    int              sem_count;
    spinlock_t       lock;
} sysv_sem_t;

typedef struct {
    sysv_sem_t sems[SYSV_SEM_MAX];
    int        count;
    spinlock_t lock;
} sysv_sem_manager_t;

#define SYSV_MSG_MAX          64
#define SYSV_MSG_TEXT_MAX     8192

typedef struct sysv_msg_node sysv_msg_node_t;
struct sysv_msg_node {
    long          mtype;
    uint8_t       mtext[SYSV_MSG_TEXT_MAX];
    uint32_t      msize;
    sysv_msg_node_t* next;
};

typedef struct {
    sysv_ipc_perm_t  perm;
    sysv_msg_node_t* first;
    sysv_msg_node_t* last;
    uint32_t         msg_count;
    uint32_t         msg_bytes;
    uint32_t         max_bytes;
    spinlock_t       lock;
} sysv_msg_t;

typedef struct {
    sysv_msg_t msgs[SYSV_MSG_MAX];
    int        count;
    spinlock_t lock;
} sysv_msg_manager_t;

void          sysv_shm_init(sysv_shm_manager_t* mgr);
int           sysv_shmget(sysv_shm_manager_t* mgr, int key, uint32_t size, int flags);
void*         sysv_shmat(sysv_shm_manager_t* mgr, int shmid, const void* addr, int flags);
int           sysv_shmdt(sysv_shm_manager_t* mgr, const void* addr);
int           sysv_shmctl(sysv_shm_manager_t* mgr, int shmid, int cmd, void* buf);

void          sysv_sem_init(sysv_sem_manager_t* mgr);
int           sysv_semget(sysv_sem_manager_t* mgr, int key, int nsems, int flags);
int           sysv_semop(sysv_sem_manager_t* mgr, int semid, int* ops, int nops);
int           sysv_semctl(sysv_sem_manager_t* mgr, int semid, int semnum, int cmd, void* arg);

void          sysv_msg_init(sysv_msg_manager_t* mgr);
int           sysv_msgget(sysv_msg_manager_t* mgr, int key, int flags);
int           sysv_msgsnd(sysv_msg_manager_t* mgr, int msqid, const void* msg, uint32_t size, int flags);
int           sysv_msgrcv(sysv_msg_manager_t* mgr, int msqid, void* msg, uint32_t size, long mtype, int flags);

#define KMSG_BUF_SIZE    4096
#define KMSG_MAX         8

typedef struct {
    char     buf[KMSG_BUF_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t seq;
    int      facility;
    int      level;
    spinlock_t lock;
} kmsg_buf_t;

typedef struct {
    kmsg_buf_t buffers[KMSG_MAX];
    int        count;
    uint32_t   seq_counter;
    spinlock_t lock;
} kmsg_t;

void     kmsg_init(kmsg_t* km);
int      kmsg_write(kmsg_t* km, int facility, int level, const char* msg, uint32_t len);
int      kmsg_read(kmsg_t* km, char* buf, uint32_t size, int* facility, int* level);

#define CHARDEV_MAX       128

typedef struct chardev chardev_t;

struct chardev {
    int          major;
    int          minor;
    char         name[32];
    int          ref_count;
    int          (*open)(chardev_t* dev);
    int          (*close)(chardev_t* dev);
    int          (*read)(chardev_t* dev, void* buf, uint32_t count);
    int          (*write)(chardev_t* dev, const void* buf, uint32_t count);
    int          (*ioctl)(chardev_t* dev, uint32_t cmd, void* arg);
    void*        priv;
    spinlock_t   lock;
};

typedef struct {
    chardev_t  devices[CHARDEV_MAX];
    int        count;
    spinlock_t lock;
} chardev_manager_t;

void       chardev_init(chardev_manager_t* mgr);
int        chardev_register(chardev_manager_t* mgr, int major, int minor, const char* name);
chardev_t* chardev_find(chardev_manager_t* mgr, int major, int minor);
int        chardev_open(chardev_t* dev);
int        chardev_close(chardev_t* dev);
int        chardev_read(chardev_t* dev, void* buf, uint32_t count);
int        chardev_write(chardev_t* dev, const void* buf, uint32_t count);
int        chardev_ioctl(chardev_t* dev, uint32_t cmd, void* arg);

#define EVDEV_MAX          32
#define EVDEV_BUF_SIZE     256

#define EV_SYN             0x00
#define EV_KEY             0x01
#define EV_REL             0x02
#define EV_ABS             0x03
#define EV_MSC             0x04
#define EV_SW              0x05
#define EV_LED             0x11
#define EV_SND             0x12
#define EV_REP             0x13
#define EV_FF              0x14

typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t  value;
} input_event_t;

typedef struct {
    input_event_t  buf[EVDEV_BUF_SIZE];
    uint32_t       head;
    uint32_t       tail;
    uint32_t       count;
    uint32_t       ev_bits[8];
    uint32_t       key_bits[12];
    uint32_t       rel_bits[2];
    uint32_t       abs_bits[2];
    int            grab_fd;
    spinlock_t     lock;
} evdev_t;

typedef struct {
    evdev_t    devices[EVDEV_MAX];
    int        count;
    spinlock_t lock;
} evdev_manager_t;

void     evdev_init(evdev_manager_t* mgr);
int      evdev_register(evdev_manager_t* mgr);
int      evdev_push_event(evdev_t* dev, uint16_t type, uint16_t code, int32_t value);
int      evdev_read_event(evdev_t* dev, input_event_t* ev);

#endif