#include "cgroup.h"
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

struct cgroupfs_root cgroup_roots[CGROUP_MAX_HIERARCHY];
struct cgroup* cgroup_default = NULL;
uint64_t cgroup_next_id = 1;

static spinlock_t cgroup_id_lock = SPINLOCK_INIT;

static uint64_t alloc_cgroup_id(void)
{
    uint64_t id;
    spin_lock(&cgroup_id_lock);
    id = cgroup_next_id++;
    spin_unlock(&cgroup_id_lock);
    return id;
}

static struct cgroup* find_child(struct cgroup* parent, const char* name)
{
    if (!parent || !name) return NULL;
    
    struct cgroup* child = parent->children;
    while (child) {
        if (strncmp(child->name, name, CGROUP_NAME_MAX) == 0) {
            return child;
        }
        child = child->sibling;
    }
    return NULL;
}

void cgroup_init(void)
{
    memset(cgroup_roots, 0, sizeof(cgroup_roots));
    cgroup_default = NULL;
    cgroup_next_id = 1;
    
    cgroup_roots[0].hierarchy_id = 0;
    cgroup_roots[0].subsys_mask = 0;
    spin_init(&cgroup_roots[0].lock);
    cgroup_roots[0].active = 1;
    
    cgroup_default = cgroup_create(NULL, "/");
    if (cgroup_default) {
        cgroup_roots[0].top_cgroup = cgroup_default;
        cgroup_default->root = &cgroup_roots[0];
    }
}

struct cgroup* cgroup_create(struct cgroup* parent, const char* name)
{
    if (!name || name[0] == '\0') return NULL;
    
    struct cgroup* cgrp = (struct cgroup*)kzalloc(sizeof(struct cgroup));
    if (!cgrp) return NULL;
    
    cgrp->id = alloc_cgroup_id();
    strncpy(cgrp->name, name, CGROUP_NAME_MAX - 1);
    cgrp->name[CGROUP_NAME_MAX - 1] = '\0';
    cgrp->parent = parent;
    cgrp->children = NULL;
    cgrp->sibling = NULL;
    cgrp->level = parent ? parent->level + 1 : 0;
    spin_init(&cgrp->lock);
    
    for (int i = 0; i < CGROUP_MAX_SUBSYS; i++) {
        cgrp->subsys[i] = NULL;
    }
    
    cgrp->nr_procs = 0;
    memset(cgrp->proc_pids, 0, sizeof(cgrp->proc_pids));
    
    cgrp->cpu_usage_ns = 0;
    cgrp->cpu_shares = CPU_SHARES_DEFAULT;
    cgrp->cpu_cfs_period_us = CPU_CFS_PERIOD_DEFAULT;
    cgrp->cpu_cfs_quota_us = CPU_CFS_QUOTA_DEFAULT;
    
    cgrp->mem_usage_pages = 0;
    cgrp->mem_limit_pages = MEM_LIMIT_MAX;
    cgrp->memsw_limit_pages = MEM_LIMIT_MAX;
    
    cgrp->root = NULL;
    cgrp->flags = 0;
    cgrp->populated = 0;
    
    if (parent) {
        spin_lock(&parent->lock);
        cgrp->sibling = parent->children;
        parent->children = cgrp;
        cgrp->root = parent->root;
        spin_unlock(&parent->lock);
    }
    
    return cgrp;
}

static void cgroup_destroy_recursive(struct cgroup* cgrp)
{
    if (!cgrp) return;
    
    struct cgroup* child = cgrp->children;
    while (child) {
        struct cgroup* next = child->sibling;
        cgroup_destroy_recursive(child);
        child = next;
    }
    
    for (int i = 0; i < CGROUP_MAX_SUBSYS; i++) {
        if (cgrp->subsys[i]) {
            kfree(cgrp->subsys[i]);
        }
    }
    
    kfree(cgrp);
}

void cgroup_destroy(struct cgroup* cgrp)
{
    if (!cgrp) return;
    if (cgrp == cgroup_default) return;
    
    spin_lock(&cgrp->lock);
    if (cgrp->nr_procs > 0) {
        spin_unlock(&cgrp->lock);
        return;
    }
    if (cgrp->children) {
        spin_unlock(&cgrp->lock);
        return;
    }
    spin_unlock(&cgrp->lock);
    
    if (cgrp->parent) {
        spin_lock(&cgrp->parent->lock);
        struct cgroup** p = &cgrp->parent->children;
        while (*p) {
            if (*p == cgrp) {
                *p = cgrp->sibling;
                break;
            }
            p = &(*p)->sibling;
        }
        spin_unlock(&cgrp->parent->lock);
    }
    
    cgroup_destroy_recursive(cgrp);
}

int cgroup_attach_task(struct cgroup* cgrp, uint64_t pid)
{
    if (!cgrp || pid == 0) return -1;
    
    spin_lock(&cgrp->lock);
    if (cgrp->nr_procs >= CGROUP_MAX_PROCS) {
        spin_unlock(&cgrp->lock);
        return -1;
    }
    
    for (uint64_t i = 0; i < cgrp->nr_procs; i++) {
        if (cgrp->proc_pids[i] == pid) {
            spin_unlock(&cgrp->lock);
            return 0;
        }
    }
    
    cgrp->proc_pids[cgrp->nr_procs] = pid;
    cgrp->nr_procs++;
    cgrp->populated = 1;
    spin_unlock(&cgrp->lock);
    
    return 0;
}

int cgroup_detach_task(struct cgroup* cgrp, uint64_t pid)
{
    if (!cgrp) return -1;
    
    spin_lock(&cgrp->lock);
    for (uint64_t i = 0; i < cgrp->nr_procs; i++) {
        if (cgrp->proc_pids[i] == pid) {
            for (uint64_t j = i; j < cgrp->nr_procs - 1; j++) {
                cgrp->proc_pids[j] = cgrp->proc_pids[j + 1];
            }
            cgrp->nr_procs--;
            if (cgrp->nr_procs == 0) {
                cgrp->populated = 0;
            }
            spin_unlock(&cgrp->lock);
            return 0;
        }
    }
    spin_unlock(&cgrp->lock);
    
    return -1;
}

struct cgroup* cgroup_find_by_name(const char* path)
{
    if (!path || !cgroup_default) return NULL;
    
    if (strcmp(path, "/") == 0) {
        return cgroup_default;
    }
    
    if (path[0] != '/') return NULL;
    
    char temp[512];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    struct cgroup* cgrp = cgroup_default;
    char* p = temp + 1;
    
    while (*p) {
        char* token = p;
        while (*p && *p != '/') p++;
        if (*p == '/') *p++ = '\0';
        
        if (token[0] == '\0') continue;
        
        cgrp = find_child(cgrp, token);
        if (!cgrp) return NULL;
        
        while (*p == '/') p++;
    }
    
    return cgrp;
}

struct cgroup* cgroup_get_root(uint64_t hierarchy_id)
{
    if (hierarchy_id >= CGROUP_MAX_HIERARCHY) return NULL;
    if (!cgroup_roots[hierarchy_id].active) return NULL;
    return cgroup_roots[hierarchy_id].top_cgroup;
}

void cgroup_cpu_account(struct cgroup* cgrp, uint64_t delta_ns)
{
    if (!cgrp) return;
    struct cgroup* p = cgrp;
    while (p) {
        p->cpu_usage_ns += delta_ns;
        p = p->parent;
    }
}

void cgroup_mem_account(struct cgroup* cgrp, uint64_t pages)
{
    if (!cgrp) return;
    struct cgroup* p = cgrp;
    while (p) {
        p->mem_usage_pages += pages;
        p = p->parent;
    }
}

void cgroup_mem_unaccount(struct cgroup* cgrp, uint64_t pages)
{
    if (!cgrp) return;
    struct cgroup* p = cgrp;
    while (p) {
        if (p->mem_usage_pages >= pages) {
            p->mem_usage_pages -= pages;
        } else {
            p->mem_usage_pages = 0;
        }
        p = p->parent;
    }
}

int cgroup_mem_charge(struct cgroup* cgrp, uint64_t pages)
{
    if (!cgrp) return -1;
    
    struct cgroup* p = cgrp;
    while (p) {
        if (p->mem_limit_pages != MEM_LIMIT_MAX &&
            p->mem_usage_pages + pages > p->mem_limit_pages) {
            return -1;
        }
        p = p->parent;
    }
    
    cgroup_mem_account(cgrp, pages);
    return 0;
}

int cgroup_file_register(struct cgroup* cgrp, const char* name,
                         int (*read)(struct cgroup*, char*, uint64_t),
                         int (*write)(struct cgroup*, const char*, uint64_t))
{
    (void)cgrp;
    (void)name;
    (void)read;
    (void)write;
    return 0;
}

int cgroup_file_read(struct cgroup* cgrp, const char* name, char* buf, uint64_t size)
{
    (void)cgrp;
    (void)name;
    (void)buf;
    (void)size;
    return -1;
}

int cgroup_file_write(struct cgroup* cgrp, const char* name, const char* buf, uint64_t size)
{
    (void)cgrp;
    (void)name;
    (void)buf;
    (void)size;
    return -1;
}

int cgroup_subsys_register(struct cgroup_subsys* ss)
{
    (void)ss;
    return 0;
}

struct cgroup_subsys_state* cgroup_get_css(struct cgroup* cgrp, uint64_t subsys_id)
{
    if (!cgrp || subsys_id >= CGROUP_MAX_SUBSYS) return NULL;
    return cgrp->subsys[subsys_id];
}

struct css_set* css_set_alloc(void)
{
    return (struct css_set*)kzalloc(sizeof(struct css_set));
}

void css_set_free(struct css_set* set)
{
    if (set) kfree(set);
}

void css_set_attach(struct css_set* set, uint64_t pid)
{
    (void)set;
    (void)pid;
}
