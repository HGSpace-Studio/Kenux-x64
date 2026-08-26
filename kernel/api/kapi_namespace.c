/*
 * Kenux Advanced OS Skeleton - Namespaces implementation
 *
 * Skeleton: per-type namespace tables with id/refcount tracking. Task
 * association requires integration with the process table (pid -> ns).
 */

#include "kapi_namespace.h"
#include "kapi.h"

#include <string.h>

static kapi_ns_t kapi_ns_table[KAPI_NS_TYPE_MAX][KAPI_NS_MAX_PER_TYPE];
static int kapi_ns_initialized = 0;
static uint32_t kapi_ns_next_id = 1;

int kapi_ns_init(void)
{
    if (kapi_ns_initialized) {
        return KAPI_NS_OK;
    }
    memset(kapi_ns_table, 0, sizeof(kapi_ns_table));
    kapi_ns_next_id = 1;
    kapi_ns_initialized = 1;
    return KAPI_NS_OK;
}

void kapi_ns_exit(void)
{
    memset(kapi_ns_table, 0, sizeof(kapi_ns_table));
    kapi_ns_initialized = 0;
}

static kapi_ns_t *ns_slot_alloc(kapi_ns_type_t type)
{
    if (type >= KAPI_NS_TYPE_MAX) {
        return NULL;
    }
    for (int i = 0; i < KAPI_NS_MAX_PER_TYPE; i++) {
        if (!kapi_ns_table[type][i].registered) {
            return &kapi_ns_table[type][i];
        }
    }
    return NULL;
}

kapi_ns_t *kapi_ns_create(kapi_ns_type_t type, const char *name)
{
    if (type >= KAPI_NS_TYPE_MAX) {
        return NULL;
    }
    if (name && kapi_ns_find_by_name(name, type)) {
        return NULL; /* EEXIST semantics */
    }
    kapi_ns_t *ns = ns_slot_alloc(type);
    if (!ns) {
        return NULL;
    }
    memset(ns, 0, sizeof(*ns));
    ns->id = kapi_ns_next_id++;
    ns->type = type;
    if (name) {
        strncpy(ns->name, name, KAPI_NS_NAME_MAX - 1);
    }
    ns->refcount = 1;
    ns->registered = 1;
    /* TODO: initialize type-specific state (mount tree, netdev, etc.) */
    return ns;
}

int kapi_ns_destroy(kapi_ns_t *ns)
{
    if (!ns || !ns->registered) {
        return KAPI_NS_EINVAL;
    }
    if (ns->refcount > 1) {
        return KAPI_NS_EBUSY;
    }
    /* TODO: tear down type-specific resources */
    memset(ns, 0, sizeof(*ns));
    return KAPI_NS_OK;
}

int kapi_ns_destroy_by_id(uint32_t id, kapi_ns_type_t type)
{
    if (type >= KAPI_NS_TYPE_MAX) {
        return KAPI_NS_EINVAL;
    }
    for (int i = 0; i < KAPI_NS_MAX_PER_TYPE; i++) {
        if (kapi_ns_table[type][i].registered &&
            kapi_ns_table[type][i].id == id) {
            return kapi_ns_destroy(&kapi_ns_table[type][i]);
        }
    }
    return KAPI_NS_ENOENT;
}

kapi_ns_t *kapi_ns_find_by_id(uint32_t id, kapi_ns_type_t type)
{
    if (type >= KAPI_NS_TYPE_MAX) {
        return NULL;
    }
    for (int i = 0; i < KAPI_NS_MAX_PER_TYPE; i++) {
        if (kapi_ns_table[type][i].registered &&
            kapi_ns_table[type][i].id == id) {
            return &kapi_ns_table[type][i];
        }
    }
    return NULL;
}

kapi_ns_t *kapi_ns_find_by_name(const char *name, kapi_ns_type_t type)
{
    if (!name || type >= KAPI_NS_TYPE_MAX) {
        return NULL;
    }
    for (int i = 0; i < KAPI_NS_MAX_PER_TYPE; i++) {
        if (kapi_ns_table[type][i].registered &&
            strncmp(kapi_ns_table[type][i].name, name,
                    KAPI_NS_NAME_MAX) == 0) {
            return &kapi_ns_table[type][i];
        }
    }
    return NULL;
}

int kapi_ns_list(kapi_ns_type_t type, kapi_ns_t *out, uint32_t max,
                 uint32_t *count)
{
    if (type >= KAPI_NS_TYPE_MAX || !out || !count) {
        return KAPI_NS_EINVAL;
    }
    uint32_t n = 0;
    for (int i = 0; i < KAPI_NS_MAX_PER_TYPE && n < max; i++) {
        if (kapi_ns_table[type][i].registered) {
            out[n++] = kapi_ns_table[type][i];
        }
    }
    *count = n;
    return KAPI_NS_OK;
}

int kapi_ns_attach(int32_t pid, kapi_ns_type_t type, kapi_ns_t *ns)
{
    if (type >= KAPI_NS_TYPE_MAX || !ns) {
        return KAPI_NS_EINVAL;
    }
    /* TODO: update task_struct->nsproxy[type] and bump refcount */
    (void)pid;
    ns->refcount++;
    return KAPI_NS_OK;
}

int kapi_ns_detach(int32_t pid, kapi_ns_type_t type)
{
    (void)pid; (void)type;
    /* TODO: drop refcount via task_struct lookup */
    return KAPI_NS_OK;
}

kapi_ns_t *kapi_ns_get_task_ns(int32_t pid, kapi_ns_type_t type)
{
    (void)pid; (void)type;
    /* TODO: look up task and return its namespace for type */
    return NULL;
}

kapi_ns_t *kapi_ns_clone(int32_t pid, kapi_ns_type_t type)
{
    kapi_ns_t *parent = kapi_ns_get_task_ns(pid, type);
    (void)parent;
    /* TODO: copy parent's namespace state into a fresh ns */
    return kapi_ns_create(type, NULL);
}
