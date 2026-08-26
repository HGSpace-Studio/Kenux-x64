#include "include/namespace.h"
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

static u64 ns_ino_counter = 1;
static spinlock_t ns_global_lock = SPINLOCK_INIT;

static u64 alloc_ns_ino(void)
{
    u64 ino;
    spin_lock(&ns_global_lock);
    ino = ns_ino_counter++;
    spin_unlock(&ns_global_lock);
    return ino;
}

static void ns_init_common(namespace_t *ns, u32 type, ns_operations_t *ops)
{
    ns->type = type;
    ns->refcount = 1;
    ns->ops = ops;
    ns->flags = 0;
    ns->ino = alloc_ns_ino();
    spin_init(&ns->lock);
}

int ns_get(namespace_t *ns)
{
    if (!ns) return -1;
    __sync_fetch_and_add(&ns->refcount, 1);
    return 0;
}

void ns_put(namespace_t *ns)
{
    if (!ns) return;
    if (__sync_sub_and_fetch(&ns->refcount, 1) == 0) {
        if (ns->ops && ns->ops->put) {
            ns->ops->put(ns);
        }
    }
}

static void mnt_ns_put_op(void *ns)
{
    mnt_namespace_t *mnt_ns = (mnt_namespace_t *)ns;
    mnt_mount_t *mnt = mnt_ns->mount_list;
    while (mnt) {
        mnt_mount_t *next = mnt->mnt_next;
        kfree(mnt);
        mnt = next;
    }
    kfree(mnt_ns);
}

static ns_operations_t mnt_ns_ops = {
    .install = NULL,
    .put = mnt_ns_put_op
};

mnt_namespace_t *mnt_ns_create(void)
{
    mnt_namespace_t *ns = kzalloc(sizeof(mnt_namespace_t));
    if (!ns) return NULL;
    ns_init_common(&ns->ns, NS_TYPE_MNT, &mnt_ns_ops);
    ns->root = NULL;
    ns->nr_mounts = 0;
    ns->seq = 0;
    ns->mount_list = NULL;
    return ns;
}

void mnt_ns_destroy(mnt_namespace_t *ns)
{
    if (!ns) return;
    while (ns->ns.refcount > 1) {
        ns_put(&ns->ns);
    }
    ns_put(&ns->ns);
}

int mnt_ns_add_mount(mnt_namespace_t *ns, mnt_mount_t *mnt)
{
    if (!ns || !mnt) return -1;
    if (ns->nr_mounts >= MNT_NS_MAX_MOUNTS) return -1;

    spin_lock(&ns->ns.lock);
    mnt->mnt_id = ns->nr_mounts + 1;
    mnt->mnt_child_list = NULL;
    mnt->mnt_child = NULL;
    mnt->mnt_parent = ns->root;

    mnt->mnt_prev = NULL;
    mnt->mnt_next = ns->mount_list;
    if (ns->mount_list) {
        ns->mount_list->mnt_prev = mnt;
    }
    ns->mount_list = mnt;

    if (ns->root && !mnt->mnt_parent) {
        mnt->mnt_parent = ns->root;
    } else if (!ns->root) {
        ns->root = mnt;
        mnt->mnt_parent = NULL;
    }

    ns->nr_mounts++;
    ns->seq++;
    spin_unlock(&ns->ns.lock);
    return 0;
}

void mnt_ns_remove_mount(mnt_namespace_t *ns, mnt_mount_t *mnt)
{
    if (!ns || !mnt) return;

    spin_lock(&ns->ns.lock);
    if (mnt->mnt_prev) {
        mnt->mnt_prev->mnt_next = mnt->mnt_next;
    } else {
        ns->mount_list = mnt->mnt_next;
    }
    if (mnt->mnt_next) {
        mnt->mnt_next->mnt_prev = mnt->mnt_prev;
    }

    if (ns->root == mnt) {
        ns->root = ns->mount_list;
    }

    ns->nr_mounts--;
    ns->seq++;
    spin_unlock(&ns->ns.lock);
}

static void uts_ns_put_op(void *ns)
{
    kfree((uts_namespace_t *)ns);
}

static ns_operations_t uts_ns_ops = {
    .install = NULL,
    .put = uts_ns_put_op
};

uts_namespace_t *uts_ns_create(void)
{
    uts_namespace_t *ns = kzalloc(sizeof(uts_namespace_t));
    if (!ns) return NULL;
    ns_init_common(&ns->ns, NS_TYPE_UTS, &uts_ns_ops);
    strncpy(ns->hostname, "kenux", UTS_HOSTNAME_MAX - 1);
    strncpy(ns->domainname, "", UTS_DOMAINNAME_MAX);
    strncpy(ns->osname, "Kenux", UTS_HOSTNAME_MAX - 1);
    strncpy(ns->release, "1.0.0", UTS_HOSTNAME_MAX - 1);
    strncpy(ns->version, "#1 SMP", UTS_HOSTNAME_MAX - 1);
    strncpy(ns->machine, "x86_64", UTS_HOSTNAME_MAX - 1);
    return ns;
}

void uts_ns_destroy(uts_namespace_t *ns)
{
    if (!ns) return;
    while (ns->ns.refcount > 1) {
        ns_put(&ns->ns);
    }
    ns_put(&ns->ns);
}

void uts_ns_set_hostname(uts_namespace_t *ns, const char *name)
{
    if (!ns || !name) return;
    spin_lock(&ns->ns.lock);
    strncpy(ns->hostname, name, UTS_HOSTNAME_MAX - 1);
    ns->hostname[UTS_HOSTNAME_MAX - 1] = '\0';
    spin_unlock(&ns->ns.lock);
}

void uts_ns_set_domainname(uts_namespace_t *ns, const char *name)
{
    if (!ns || !name) return;
    spin_lock(&ns->ns.lock);
    strncpy(ns->domainname, name, UTS_DOMAINNAME_MAX - 1);
    ns->domainname[UTS_DOMAINNAME_MAX - 1] = '\0';
    spin_unlock(&ns->ns.lock);
}

static void ipc_ns_put_op(void *ns)
{
    kfree((ipc_namespace_t *)ns);
}

static ns_operations_t ipc_ns_ops = {
    .install = NULL,
    .put = ipc_ns_put_op
};

ipc_namespace_t *ipc_ns_create(void)
{
    ipc_namespace_t *ns = kzalloc(sizeof(ipc_namespace_t));
    if (!ns) return NULL;
    ns_init_common(&ns->ns, NS_TYPE_IPC, &ipc_ns_ops);
    memset(ns->msg_ids, 0, sizeof(ns->msg_ids));
    ns->msg_ids_count = 0;
    memset(ns->sem_ids, 0, sizeof(ns->sem_ids));
    ns->sem_ids_count = 0;
    memset(ns->shm_ids, 0, sizeof(ns->shm_ids));
    ns->shm_ids_count = 0;
    return ns;
}

void ipc_ns_destroy(ipc_namespace_t *ns)
{
    if (!ns) return;
    while (ns->ns.refcount > 1) {
        ns_put(&ns->ns);
    }
    ns_put(&ns->ns);
}

static void net_ns_put_op(void *ns)
{
    net_namespace_t *net_ns = (net_namespace_t *)ns;
    for (u32 i = 0; i < NET_IF_INDEX_MAX; i++) {
        if (net_ns->devices[i]) {
            kfree(net_ns->devices[i]);
        }
    }
    kfree(net_ns);
}

static ns_operations_t net_ns_ops = {
    .install = NULL,
    .put = net_ns_put_op
};

net_namespace_t *net_ns_create(void)
{
    net_namespace_t *ns = kzalloc(sizeof(net_namespace_t));
    if (!ns) return NULL;
    ns_init_common(&ns->ns, NS_TYPE_NET, &net_ns_ops);
    memset(ns->devices, 0, sizeof(ns->devices));
    ns->nr_devices = 0;
    memset(ns->routes, 0, sizeof(ns->routes));
    ns->nr_routes = 0;
    memset(ns->nf_hooks, 0, sizeof(ns->nf_hooks));
    spin_init(&ns->lock);
    return ns;
}

void net_ns_destroy(net_namespace_t *ns)
{
    if (!ns) return;
    while (ns->ns.refcount > 1) {
        ns_put(&ns->ns);
    }
    ns_put(&ns->ns);
}

static void pid_ns_put_op(void *ns)
{
    pid_namespace_t *pid_ns = (pid_namespace_t *)ns;
    if (pid_ns->pid_allocated) {
        kfree(pid_ns->pid_allocated);
    }
    kfree(pid_ns);
}

static ns_operations_t pid_ns_ops = {
    .install = NULL,
    .put = pid_ns_put_op
};

pid_namespace_t *pid_ns_create(pid_namespace_t *parent)
{
    pid_namespace_t *ns = kzalloc(sizeof(pid_namespace_t));
    if (!ns) return NULL;
    ns_init_common(&ns->ns, NS_TYPE_PID, &pid_ns_ops);
    ns->parent = parent;
    ns->level = parent ? parent->level + 1 : 0;
    ns->child_reaper_pid = 1;
    ns->last_pid = 0;
    ns->pid_allocated = kzalloc(PID_NS_PID_MAX / 8 + 1);
    if (!ns->pid_allocated) {
        kfree(ns);
        return NULL;
    }
    spin_init(&ns->pid_lock);
    return ns;
}

void pid_ns_destroy(pid_namespace_t *ns)
{
    if (!ns) return;
    while (ns->ns.refcount > 1) {
        ns_put(&ns->ns);
    }
    ns_put(&ns->ns);
}

static int ns_alloc_pid(pid_namespace_t *ns)
{
    if (!ns || !ns->pid_allocated) return -1;

    spin_lock(&ns->pid_lock);
    for (u32 i = 1; i < PID_NS_PID_MAX; i++) {
        u32 byte_idx = i / 8;
        u32 bit_idx = i % 8;
        if (!(ns->pid_allocated[byte_idx] & (1 << bit_idx))) {
            ns->pid_allocated[byte_idx] |= (1 << bit_idx);
            ns->last_pid = i;
            spin_unlock(&ns->pid_lock);
            return (int)i;
        }
    }
    spin_unlock(&ns->pid_lock);
    return -1;
}

int pid_ns_alloc_pid(pid_namespace_t *ns)
{
    return ns_alloc_pid(ns);
}

void pid_ns_free_pid(pid_namespace_t *ns, u32 pid)
{
    if (!ns || !ns->pid_allocated || pid == 0 || pid >= PID_NS_PID_MAX) return;
    spin_lock(&ns->pid_lock);
    u32 byte_idx = pid / 8;
    u32 bit_idx = pid % 8;
    ns->pid_allocated[byte_idx] &= ~(1 << bit_idx);
    spin_unlock(&ns->pid_lock);
}

static void user_ns_put_op(void *ns)
{
    kfree((user_namespace_t *)ns);
}

static ns_operations_t user_ns_ops = {
    .install = NULL,
    .put = user_ns_put_op
};

user_namespace_t *user_ns_create(user_namespace_t *parent)
{
    user_namespace_t *ns = kzalloc(sizeof(user_namespace_t));
    if (!ns) return NULL;
    ns_init_common(&ns->ns, NS_TYPE_USER, &user_ns_ops);
    ns->parent = parent;
    ns->uid_map_count = 0;
    ns->gid_map_count = 0;
    ns->owner = 0;
    ns->group = 0;
    ns->level = parent ? parent->level + 1 : 0;
    return ns;
}

void user_ns_destroy(user_namespace_t *ns)
{
    if (!ns) return;
    while (ns->ns.refcount > 1) {
        ns_put(&ns->ns);
    }
    ns_put(&ns->ns);
}

namespace_t *ns_create(u32 type)
{
    switch (type) {
    case NS_TYPE_MNT: return (namespace_t *)mnt_ns_create();
    case NS_TYPE_UTS: return (namespace_t *)uts_ns_create();
    case NS_TYPE_IPC: return (namespace_t *)ipc_ns_create();
    case NS_TYPE_NET: return (namespace_t *)net_ns_create();
    case NS_TYPE_PID: return (namespace_t *)pid_ns_create(NULL);
    case NS_TYPE_USER: return (namespace_t *)user_ns_create(NULL);
    case NS_TYPE_CGROUP: return NULL;
    default: return NULL;
    }
}

static int unshare_flags_to_ns_type(u32 type)
{
    return (int)type;
}

int ns_unshare(u32 type)
{
    if (type == 0) return 0;

    u32 ns_type = unshare_flags_to_ns_type(type);
    namespace_t *ns = ns_create(ns_type);
    if (!ns) return -1;

    ns_put(&((namespace_t *)ns));
    return 0;
}

int ns_setns(u64 fd, u32 type)
{
    (void)fd;
    (void)type;
    return 0;
}

task_namespaces_t *task_ns_alloc(void)
{
    task_namespaces_t *ns = kzalloc(sizeof(task_namespaces_t));
    if (!ns) return NULL;
    ns->mnt_ns = mnt_ns_create();
    ns->uts_ns = uts_ns_create();
    ns->ipc_ns = ipc_ns_create();
    ns->net_ns = net_ns_create();
    ns->pid_ns = pid_ns_create(NULL);
    ns->user_ns = user_ns_create(NULL);
    return ns;
}

void task_ns_free(task_namespaces_t *ns)
{
    if (!ns) return;
    if (ns->mnt_ns) ns_put(&ns->mnt_ns->ns);
    if (ns->uts_ns) ns_put(&ns->uts_ns->ns);
    if (ns->ipc_ns) ns_put(&ns->ipc_ns->ns);
    if (ns->net_ns) ns_put(&ns->net_ns->ns);
    if (ns->pid_ns) ns_put(&ns->pid_ns->ns);
    if (ns->user_ns) ns_put(&ns->user_ns->ns);
    kfree(ns);
}

static mnt_namespace_t *dup_mnt_ns(mnt_namespace_t *old)
{
    if (!old) return NULL;
    mnt_namespace_t *ns = mnt_ns_create();
    if (!ns) return NULL;
    spin_lock(&old->ns.lock);
    mnt_mount_t *mnt = old->mount_list;
    while (mnt) {
        mnt_mount_t *new_mnt = kzalloc(sizeof(mnt_mount_t));
        if (new_mnt) {
            memcpy(new_mnt, mnt, sizeof(mnt_mount_t));
            new_mnt->mnt_prev = NULL;
            new_mnt->mnt_next = ns->mount_list;
            if (ns->mount_list) {
                ns->mount_list->mnt_prev = new_mnt;
            }
            ns->mount_list = new_mnt;
            ns->nr_mounts++;
        }
        mnt = mnt->mnt_next;
    }
    ns->root = old->root;
    ns->seq = old->seq + 1;
    spin_unlock(&old->ns.lock);
    return ns;
}

static uts_namespace_t *dup_uts_ns(uts_namespace_t *old)
{
    if (!old) return NULL;
    uts_namespace_t *ns = kzalloc(sizeof(uts_namespace_t));
    if (!ns) return NULL;
    memcpy(ns, old, sizeof(uts_namespace_t));
    ns->ns.refcount = 1;
    ns->ns.ino = alloc_ns_ino();
    spin_init(&ns->ns.lock);
    return ns;
}

static ipc_namespace_t *dup_ipc_ns(ipc_namespace_t *old)
{
    if (!old) return NULL;
    ipc_namespace_t *ns = kzalloc(sizeof(ipc_namespace_t));
    if (!ns) return NULL;
    memcpy(ns, old, sizeof(ipc_namespace_t));
    ns->ns.refcount = 1;
    ns->ns.ino = alloc_ns_ino();
    spin_init(&ns->ns.lock);
    return ns;
}

static net_namespace_t *dup_net_ns(net_namespace_t *old)
{
    if (!old) return NULL;
    net_namespace_t *ns = net_ns_create();
    if (!ns) return NULL;
    spin_lock(&old->lock);
    memcpy(ns->devices, old->devices, sizeof(ns->devices));
    ns->nr_devices = old->nr_devices;
    memcpy(ns->routes, old->routes, sizeof(ns->routes));
    ns->nr_routes = old->nr_routes;
    memcpy(ns->nf_hooks, old->nf_hooks, sizeof(ns->nf_hooks));
    spin_unlock(&old->lock);
    return ns;
}

static pid_namespace_t *dup_pid_ns(pid_namespace_t *old)
{
    if (!old) return NULL;
    return pid_ns_create(old);
}

static user_namespace_t *dup_user_ns(user_namespace_t *old)
{
    if (!old) return NULL;
    return user_ns_create(old);
}

#define CLONE_NEWNS    0x00020000
#define CLONE_NEWUTS   0x04000000
#define CLONE_NEWIPC   0x08000000
#define CLONE_NEWNET   0x40000000
#define CLONE_NEWPID   0x20000000
#define CLONE_NEWUSER  0x10000000

task_namespaces_t *copy_namespaces(u64 flags, task_namespaces_t *old_ns)
{
    if (!old_ns) return task_ns_alloc();

    task_namespaces_t *new_ns = kzalloc(sizeof(task_namespaces_t));
    if (!new_ns) return NULL;

    if (flags & CLONE_NEWNS) {
        new_ns->mnt_ns = dup_mnt_ns(old_ns->mnt_ns);
    } else if (old_ns->mnt_ns) {
        ns_get(&old_ns->mnt_ns->ns);
        new_ns->mnt_ns = old_ns->mnt_ns;
    }

    if (flags & CLONE_NEWUTS) {
        new_ns->uts_ns = dup_uts_ns(old_ns->uts_ns);
    } else if (old_ns->uts_ns) {
        ns_get(&old_ns->uts_ns->ns);
        new_ns->uts_ns = old_ns->uts_ns;
    }

    if (flags & CLONE_NEWIPC) {
        new_ns->ipc_ns = dup_ipc_ns(old_ns->ipc_ns);
    } else if (old_ns->ipc_ns) {
        ns_get(&old_ns->ipc_ns->ns);
        new_ns->ipc_ns = old_ns->ipc_ns;
    }

    if (flags & CLONE_NEWNET) {
        new_ns->net_ns = dup_net_ns(old_ns->net_ns);
    } else if (old_ns->net_ns) {
        ns_get(&old_ns->net_ns->ns);
        new_ns->net_ns = old_ns->net_ns;
    }

    if (flags & CLONE_NEWPID) {
        new_ns->pid_ns = dup_pid_ns(old_ns->pid_ns);
    } else if (old_ns->pid_ns) {
        ns_get(&old_ns->pid_ns->ns);
        new_ns->pid_ns = old_ns->pid_ns;
    }

    if (flags & CLONE_NEWUSER) {
        new_ns->user_ns = dup_user_ns(old_ns->user_ns);
    } else if (old_ns->user_ns) {
        ns_get(&old_ns->user_ns->ns);
        new_ns->user_ns = old_ns->user_ns;
    }

    return new_ns;
}

void put_namespaces(task_namespaces_t *ns)
{
    task_ns_free(ns);
}

int ns_init(void)
{
    ns_ino_counter = 1;
    spin_init(&ns_global_lock);
    return 0;
}
