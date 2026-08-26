#ifndef _CGROUP_H
#define _CGROUP_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define CGROUP_MAX_SUBSYS       16
#define CGROUP_MAX_DEPTH        32
#define CGROUP_NAME_MAX         128
#define CGROUP_PATH_MAX         1024
#define CGROUP_MAX_TASKS        4096
#define CGROUP_MAX_PROCS        4096
#define CGROUP_MAX_CHILDREN     512
#define CGROUP_MAX_HIERARCHIES  16
#define CGROUP_SUBSYS_NAME_MAX  32

#define CPU_SHARES_DEFAULT      1024
#define CPU_CFS_PERIOD_US       100000
#define CPU_CFS_QUOTA_US         (-1)

#define MEM_LIMIT_IN_BYTES_MAX  ((u64)(-1))
#define MEM_USAGE_IN_BYTES_MAX  ((u64)(-1))

#define BLKIO_WEIGHT_DEFAULT     1000
#define BLKIO_WEIGHT_MAX        10000

#define DEV_MAJOR_MAX           0xfff
#define DEV_MINOR_MAX           0xfffff

#define CGROUP_SUBSYS_CPU       0
#define CGROUP_SUBSYS_MEMORY    1
#define CGROUP_SUBSYS_BLKIO     2
#define CGROUP_SUBSYS_DEVICES   3

typedef struct cgroup_task {
    u64 pid;
    struct cgroup_task *next;
    struct cgroup_task *prev;
} cgroup_task_t;

typedef struct {
    u64 cpu_shares;
    s64 cpu_cfs_quota_us;
    u64 cpu_cfs_period_us;
    u64 cpu_runtime_us;
    u64 cpu_nr_periods;
    u64 cpu_nr_throttled;
    u64 cpu_throttled_time_ns;
    u64 cpu_stat[8];
} cgroup_cpu_state_t;

typedef struct {
    u64 memory_limit_in_bytes;
    u64 memory_usage_in_bytes;
    u64 memory_max_usage_in_bytes;
    u64 memory_failcnt;
    u64 memory_swap_limit_in_bytes;
    u64 memory_swap_usage_in_bytes;
    u64 memory_kmem_limit_in_bytes;
    u64 memory_kmem_usage_in_bytes;
    u64 memory_slab_limit_in_bytes;
    u64 memory_slab_usage_in_bytes;
    u32 oom_control;
    u32 under_oom;
    u64 oom_kill_count;
    u64 oom_notify_mask;
    void (*oom_notify)(struct cgroup *);
} cgroup_memory_state_t;

typedef struct {
    u64 blkio_throttle_read_bps;
    u64 blkio_throttle_write_bps;
    u64 blkio_throttle_read_iops;
    u64 blkio_throttle_write_iops;
    u32 blkio_weight;
} cgroup_blkio_state_t;

typedef struct {
    u8 access;
    u8 type;
    u32 major;
    u32 minor;
    struct cgroup_device_whitelist *next;
} cgroup_device_whitelist_t;

typedef struct {
    cgroup_device_whitelist_t *whitelist_head;
    u32 deny_all;
} cgroup_devices_state_t;

typedef struct cgroup_subsys_state {
    int subsys_id;
    struct cgroup *cgroup;
    void *priv;
    spinlock_t lock;
} cgroup_subsys_state_t;

typedef int (*cgroup_subsys_attach_fn)(struct cgroup *cgrp, cgroup_task_t *task);
typedef void (*cgroup_subsys_detach_fn)(struct cgroup *cgrp, cgroup_task_t *task);
typedef int (*cgroup_subsys_fork_fn)(struct cgroup *cgrp, cgroup_task_t *parent, cgroup_task_t *child);
typedef void (*cgroup_subsys_exit_fn)(struct cgroup *cgrp, cgroup_task_t *task);
typedef cgroup_subsys_state_t *(*cgroup_subsys_create_fn)(struct cgroup *cgrp);
typedef void (*cgroup_subsys_destroy_fn)(cgroup_subsys_state_t *css);

typedef struct cgroup_subsys {
    char name[CGROUP_SUBSYS_NAME_MAX];
    int id;
    cgroup_subsys_create_fn css_alloc;
    cgroup_subsys_destroy_fn css_free;
    cgroup_subsys_attach_fn attach;
    cgroup_subsys_detach_fn detach;
    cgroup_subsys_fork_fn fork;
    cgroup_subsys_exit_fn exit;
    int enabled;
} cgroup_subsys_t;

typedef struct cgroup {
    u64 id;
    char name[CGROUP_NAME_MAX];
    struct cgroup *parent;
    struct cgroup *children;
    struct cgroup *sibling_next;
    struct cgroup *sibling_prev;
    u32 depth;
    spinlock_t lock;
    u32 flags;
    cgroup_subsys_state_t *subsys_state[CGROUP_MAX_SUBSYS];
    cgroup_task_t *tasks_head;
    cgroup_task_t *tasks_tail;
    u64 nr_tasks;
    struct cgroup_hierarchy *hierarchy;
} cgroup_t;

typedef struct cgroup_hierarchy {
    u64 subsys_bitmask;
    cgroup_t *root_cgroup;
    char name[CGROUP_NAME_MAX];
    u32 hierarchy_id;
    spinlock_t lock;
    int active;
} cgroup_hierarchy_t;

int cgroup_init(void);
cgroup_t *cgroup_create(cgroup_t *parent, const char *name);
int cgroup_mkdir(cgroup_t *parent, const char *name);
void cgroup_destroy(cgroup_t *cgrp);
int cgroup_attach_task(cgroup_t *cgrp, u64 pid);
int cgroup_detach_task(cgroup_t *cgrp, u64 pid);
int cgroup_path(cgroup_t *cgrp, char *buf, u64 buflen);
u64 cgroup_task_count(cgroup_t *cgrp);

int cgroup_register_subsys(cgroup_subsys_t *ss);
cgroup_subsys_t *cgroup_get_subsys(int id);

cgroup_cpu_state_t *cgroup_cpu_state(cgroup_t *cgrp);
cgroup_memory_state_t *cgroup_mem_state(cgroup_t *cgrp);
cgroup_blkio_state_t *cgroup_blkio_state(cgroup_t *cgrp);
cgroup_devices_state_t *cgroup_dev_state(cgroup_t *cgrp);

cgroup_t *cgroup_find_by_name(const char *path);

int cgroup_cpu_set_shares(cgroup_t *cgrp, u64 shares);
int cgroup_cpu_set_cfs_quota(cgroup_t *cgrp, s64 quota_us);
int cgroup_cpu_set_cfs_period(cgroup_t *cgrp, u64 period_us);

int cgroup_mem_set_limit(cgroup_t *cgrp, u64 limit_bytes);
int cgroup_mem_check_oom(cgroup_t *cgrp);
void cgroup_mem_account(cgroup_t *cgrp, u64 delta_bytes);
void cgroup_mem_unaccount(cgroup_t *cgrp, u64 delta_bytes);

int cgroup_blkio_set_read_bps(cgroup_t *cgrp, u64 bps);
int cgroup_blkio_set_write_bps(cgroup_t *cgrp, u64 bps);
int cgroup_blkio_set_weight(cgroup_t *cgrp, u32 weight);

int cgroup_dev_allow(cgroup_t *cgrp, u8 type, u32 major, u32 minor, u8 access);
int cgroup_dev_deny(cgroup_t *cgrp, u8 type, u32 major, u32 minor, u8 access);
int cgroup_dev_check(cgroup_t *cgrp, u8 type, u32 major, u32 minor, u8 access);

cgroup_t *cgroup_from_root(cgroup_hierarchy_t *hierarchy);

extern cgroup_hierarchy_t cgroup_hierarchies[CGROUP_MAX_HIERARCHIES];
extern cgroup_subsys_t *cgroup_subsys[CGROUP_MAX_SUBSYS];
extern cgroup_t *cgroup_root_default;
extern u64 cgroup_next_id;

#endif
