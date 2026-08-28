#ifndef CGROUP_H
#define CGROUP_H

#include <arch/types.h>
#include <arch/spinlock.h>

typedef int atomic_t;

#define CGROUP_NAME_MAX         64
#define CGROUP_MAX_SUBSYS       8
#define CGROUP_MAX_HIERARCHY    8
#define CGROUP_MAX_CGROUPS      256
#define CGROUP_MAX_PROCS        1024
#define CGROUP_MAX_FILES        16

#define CPU_SHARES_DEFAULT      1024
#define CPU_CFS_PERIOD_DEFAULT  100000
#define CPU_CFS_QUOTA_DEFAULT   -1

#ifndef MEM_LIMIT_MAX
#define MEM_LIMIT_MAX           ((uint64_t)(-1))
#endif

struct cgroup_subsys;
struct cgroup;
struct css_set;
struct cgroupfs_root;

struct cgroup_subsys_state {
    struct cgroup_subsys* ss;
    struct cgroup* cgroup;
    atomic_t refcnt;
};

struct cgroup {
    uint64_t id;
    char name[CGROUP_NAME_MAX];
    struct cgroup* parent;
    struct cgroup* children;
    struct cgroup* sibling;
    uint64_t level;
    spinlock_t lock;

    struct cgroup_subsys_state* subsys[CGROUP_MAX_SUBSYS];

    uint64_t nr_procs;
    uint64_t proc_pids[CGROUP_MAX_PROCS];

    uint64_t cpu_usage_ns;
    uint64_t cpu_shares;
    uint64_t cpu_cfs_period_us;
    int64_t  cpu_cfs_quota_us;

    uint64_t mem_usage_pages;
    uint64_t mem_limit_pages;
    uint64_t memsw_limit_pages;

    struct cgroupfs_root* root;
    int flags;
    int populated;
};

struct css_set {
    uint64_t id;
    struct cgroup_subsys_state* subsys[CGROUP_MAX_SUBSYS];
    uint64_t nr_tasks;
    spinlock_t lock;
};

struct cgroup_subsys {
    char name[CGROUP_NAME_MAX];
    uint64_t subsys_id;
    struct cgroup_subsys_state* (*create)(struct cgroup* cgrp);
    void (*destroy)(struct cgroup_subsys_state* css);
    int (*can_attach)(struct cgroup* cgrp, void* task);
    void (*attach)(struct cgroup* cgrp, void* task);
    void (*fork)(struct cgroup_subsys_state* css, void* task);
    void (*exit)(struct cgroup_subsys_state* css, void* task);
    int (*populate)(struct cgroup* cgrp);
};

struct cgroupfs_root {
    uint64_t hierarchy_id;
    uint64_t subsys_mask;
    struct cgroup* top_cgroup;
    spinlock_t lock;
    int active;
};

struct cgroup_file_ops {
    char name[CGROUP_NAME_MAX];
    int (*read)(struct cgroup* cgrp, char* buf, uint64_t size);
    int (*write)(struct cgroup* cgrp, const char* buf, uint64_t size);
};

struct cgroup_file {
    char name[CGROUP_NAME_MAX];
    struct cgroup_file_ops ops;
    struct cgroup* cgroup;
    struct cgroup_file* next;
};

void cgroup_init(void);
struct cgroup* cgroup_create(struct cgroup* parent, const char* name);
void cgroup_destroy(struct cgroup* cgrp);
int cgroup_attach_task(struct cgroup* cgrp, uint64_t pid);
int cgroup_detach_task(struct cgroup* cgrp, uint64_t pid);
struct cgroup* cgroup_find_by_name(const char* path);
struct cgroup* cgroup_get_root(uint64_t hierarchy_id);

void cgroup_cpu_account(struct cgroup* cgrp, uint64_t delta_ns);
void cgroup_mem_account(struct cgroup* cgrp, uint64_t pages);
void cgroup_mem_unaccount(struct cgroup* cgrp, uint64_t pages);
int cgroup_mem_charge(struct cgroup* cgrp, uint64_t pages);

int cgroup_file_register(struct cgroup* cgrp, const char* name,
                         int (*read)(struct cgroup*, char*, uint64_t),
                         int (*write)(struct cgroup*, const char*, uint64_t));
int cgroup_file_read(struct cgroup* cgrp, const char* name, char* buf, uint64_t size);
int cgroup_file_write(struct cgroup* cgrp, const char* name, const char* buf, uint64_t size);

int cgroup_subsys_register(struct cgroup_subsys* ss);
struct cgroup_subsys_state* cgroup_get_css(struct cgroup* cgrp, uint64_t subsys_id);

struct css_set* css_set_alloc(void);
void css_set_free(struct css_set* set);
void css_set_attach(struct css_set* set, uint64_t pid);

extern struct cgroupfs_root cgroup_roots[CGROUP_MAX_HIERARCHY];
extern struct cgroup* cgroup_default;
extern uint64_t cgroup_next_id;

#endif