#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define PID_NS_LEVEL_MAX        32
#define PID_NS_PID_MAX          32768
#define MNT_NS_MAX_MOUNTS       256
#define NS_NAME_LEN             32

struct ns_common {
    uint64_t st_ino;
    uint32_t refcnt;
    spinlock_t lock;
};

struct pid_namespace {
    struct ns_common ns;
    struct pid_namespace* parent;
    uint32_t level;
    uint32_t last_pid;
    uint32_t pid_max;
    uint32_t* pid_map;
    uint32_t* pid_ns_map;
    uint64_t nr_pids;
    void* child_reaper;
    spinlock_t pid_lock;
};

struct mount {
    struct mount* mnt_parent;
    struct mount* mnt_mounts;
    struct mount* mnt_child;
    struct mount* mnt_list;
    void* mnt_root;
    void* mnt_sb;
    char mnt_devname[256];
    uint64_t mnt_flags;
    uint32_t mnt_id;
    struct mnt_namespace* mnt_ns;
};

struct mnt_namespace {
    struct ns_common ns;
    struct mount* root;
    uint64_t nr_mounts;
    uint64_t seq;
    spinlock_t lock;
};

struct uts_namespace {
    struct ns_common ns;
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct ipc_namespace {
    struct ns_common ns;
    uint32_t msg_max;
    uint32_t msg_mnb;
    uint32_t sem_msl;
    uint32_t sem_mns;
};

struct net_namespace {
    struct ns_common ns;
    uint32_t ifindex;
    spinlock_t lock;
};

struct task_namespaces {
    struct pid_namespace* pid_ns;
    struct mnt_namespace* mnt_ns;
    struct uts_namespace* uts_ns;
    struct ipc_namespace* ipc_ns;
    struct net_namespace* net_ns;
};

void ns_init(void);

struct pid_namespace* pid_ns_create(struct pid_namespace* parent);
void pid_ns_destroy(struct pid_namespace* ns);
int pid_ns_alloc_pid(struct pid_namespace* ns);
void pid_ns_free_pid(struct pid_namespace* ns, int pid);
int pid_ns_map_pid(struct pid_namespace* target, struct pid_namespace* current, int pid);
int pid_ns_visible(struct pid_namespace* outer, struct pid_namespace* inner);

struct mnt_namespace* mnt_ns_create(void);
void mnt_ns_destroy(struct mnt_namespace* ns);
int mnt_ns_add_mount(struct mnt_namespace* ns, struct mount* mnt);
void mnt_ns_remove_mount(struct mnt_namespace* ns, struct mount* mnt);
struct mount* mnt_ns_lookup(struct mnt_namespace* ns, const char* path);
int mnt_ns_dup(struct mnt_namespace* old, struct mnt_namespace* new);

struct uts_namespace* uts_ns_create(const char* nodename);
void uts_ns_destroy(struct uts_namespace* ns);
void uts_ns_set_hostname(struct uts_namespace* ns, const char* name);
void uts_ns_set_domainname(struct uts_namespace* ns, const char* name);

struct ipc_namespace* ipc_ns_create(void);
void ipc_ns_destroy(struct ipc_namespace* ns);

struct net_namespace* net_ns_create(void);
void net_ns_destroy(struct net_namespace* ns);

void task_ns_init(struct task_namespaces* ns);
void task_ns_dup(struct task_namespaces* dst, const struct task_namespaces* src);
void task_ns_exit(struct task_namespaces* ns);

struct pid_namespace* task_active_pid_ns(void* task);

#endif
