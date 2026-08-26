#ifndef _NAMESPACE_H
#define _NAMESPACE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define NS_TYPE_MNT       1
#define NS_TYPE_UTS       2
#define NS_TYPE_IPC       3
#define NS_TYPE_NET       4
#define NS_TYPE_PID       5
#define NS_TYPE_USER      6
#define NS_TYPE_CGROUP    7

#define NS_TYPE_MAX       8
#define NS_NAME_MAX       32

#define PID_NS_LEVEL_MAX       32
#define PID_NS_PID_MAX         32768
#define MNT_NS_MAX_MOUNTS      512
#define UTS_HOSTNAME_MAX       64
#define UTS_DOMAINNAME_MAX     64
#define IPC_MSG_IDS_MAX        256
#define IPC_SEM_IDS_MAX        256
#define IPC_SHM_IDS_MAX        256
#define NET_IF_INDEX_MAX       256
#define NET_ROUTE_TABLE_MAX     256
#define USER_NS_MAX_MAPPINGS    16

typedef struct ns_operations {
    int (*install)(void *ns, void *task);
    void (*put)(void *ns);
} ns_operations_t;

typedef struct namespace {
    u32 type;
    volatile u32 refcount;
    ns_operations_t *ops;
    u32 flags;
    u64 ino;
    spinlock_t lock;
} namespace_t;

typedef struct mnt_mount {
    u64 mnt_id;
    void *mnt_root;
    void *mnt_sb;
    char devname[256];
    u64 mnt_flags;
    struct mnt_mount *mnt_parent;
    struct mnt_mount *mnt_child;
    struct mnt_mount *mnt_child_list;
    struct mnt_mount *mnt_next;
    struct mnt_mount *mnt_prev;
} mnt_mount_t;

typedef struct mnt_namespace {
    namespace_t ns;
    mnt_mount_t *root;
    u64 nr_mounts;
    u64 seq;
    mnt_mount_t *mount_list;
} mnt_namespace_t;

typedef struct uts_namespace {
    namespace_t ns;
    char hostname[UTS_HOSTNAME_MAX];
    char domainname[UTS_DOMAINNAME_MAX];
    char osname[UTS_HOSTNAME_MAX];
    char release[UTS_HOSTNAME_MAX];
    char version[UTS_HOSTNAME_MAX];
    char machine[UTS_HOSTNAME_MAX];
} uts_namespace_t;

typedef struct ipc_id_entry {
    u32 key;
    u32 id;
    u32 perms;
    u32 seq;
    void *object;
} ipc_id_entry_t;

typedef struct ipc_namespace {
    namespace_t ns;
    ipc_id_entry_t msg_ids[IPC_MSG_IDS_MAX];
    u32 msg_ids_count;
    ipc_id_entry_t sem_ids[IPC_SEM_IDS_MAX];
    u32 sem_ids_count;
    ipc_id_entry_t shm_ids[IPC_SHM_IDS_MAX];
    u32 shm_ids_count;
} ipc_namespace_t;

typedef struct net_device {
    u32 ifindex;
    char name[16];
    u8 mac[6];
    u32 flags;
    u32 mtu;
    void *priv;
} net_device_t;

typedef struct net_route_entry {
    u32 dst;
    u32 mask;
    u32 gw;
    u32 ifindex;
    u32 metric;
} net_route_entry_t;

typedef struct net_nf_hook {
    void (*hook)(void *skb, void *priv);
    void *priv;
    int pf;
    int hooknum;
    int priority;
    struct net_nf_hook *next;
} net_nf_hook_t;

typedef struct net_namespace {
    namespace_t ns;
    net_device_t *devices[NET_IF_INDEX_MAX];
    u32 nr_devices;
    net_route_entry_t routes[NET_ROUTE_TABLE_MAX];
    u32 nr_routes;
    net_nf_hook_t nf_hooks[8][8];
    spinlock_t lock;
} net_namespace_t;

typedef struct pid_namespace {
    namespace_t ns;
    struct pid_namespace *parent;
    u32 level;
    u32 child_reaper_pid;
    u32 last_pid;
    u32 *pid_allocated;
    spinlock_t pid_lock;
} pid_namespace_t;

typedef struct uid_gid_mapping {
    u32 first;
    u32 lower_first;
    u32 count;
} uid_gid_mapping_t;

typedef struct user_namespace {
    namespace_t ns;
    struct user_namespace *parent;
    uid_gid_mapping_t uid_map[USER_NS_MAX_MAPPINGS];
    u32 uid_map_count;
    uid_gid_mapping_t gid_map[USER_NS_MAX_MAPPINGS];
    u32 gid_map_count;
    u32 owner;
    u32 group;
    u32 level;
} user_namespace_t;

typedef struct task_namespaces {
    mnt_namespace_t *mnt_ns;
    uts_namespace_t *uts_ns;
    ipc_namespace_t *ipc_ns;
    net_namespace_t *net_ns;
    pid_namespace_t *pid_ns;
    user_namespace_t *user_ns;
} task_namespaces_t;

int ns_init(void);

namespace_t *ns_create(u32 type);
int ns_get(namespace_t *ns);
void ns_put(namespace_t *ns);

int ns_unshare(u32 type);
int ns_setns(u64 fd, u32 type);

task_namespaces_t *copy_namespaces(u64 flags, task_namespaces_t *old_ns);
void put_namespaces(task_namespaces_t *ns);

mnt_namespace_t *mnt_ns_create(void);
void mnt_ns_destroy(mnt_namespace_t *ns);
int mnt_ns_add_mount(mnt_namespace_t *ns, mnt_mount_t *mnt);
void mnt_ns_remove_mount(mnt_namespace_t *ns, mnt_mount_t *mnt);

uts_namespace_t *uts_ns_create(void);
void uts_ns_destroy(uts_namespace_t *ns);
void uts_ns_set_hostname(uts_namespace_t *ns, const char *name);
void uts_ns_set_domainname(uts_namespace_t *ns, const char *name);

ipc_namespace_t *ipc_ns_create(void);
void ipc_ns_destroy(ipc_namespace_t *ns);

net_namespace_t *net_ns_create(void);
void net_ns_destroy(net_namespace_t *ns);

pid_namespace_t *pid_ns_create(pid_namespace_t *parent);
void pid_ns_destroy(pid_namespace_t *ns);
int pid_ns_alloc_pid(pid_namespace_t *ns);
void pid_ns_free_pid(pid_namespace_t *ns, u32 pid);

user_namespace_t *user_ns_create(user_namespace_t *parent);
void user_ns_destroy(user_namespace_t *ns);

task_namespaces_t *task_ns_alloc(void);
void task_ns_free(task_namespaces_t *ns);

#endif
