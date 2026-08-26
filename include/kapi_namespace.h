#ifndef KAPI_NAMESPACE_H
#define KAPI_NAMESPACE_H

/*
 * Kenux Advanced OS Skeleton - Namespaces (isolation primitives)
 *
 * Provides PID, mount, network, IPC, UTS, and user namespace types
 * for container isolation. Each task may belong to a namespace per
 * type. Skeleton: API only.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_NS_NAME_MAX        32
#define KAPI_NS_MAX_PER_TYPE    1024

typedef struct kapi_ns kapi_ns_t;

typedef enum {
    KAPI_NS_OK            = 0,
    KAPI_NS_EINVAL        = -1,
    KAPI_NS_ENOMEM        = -2,
    KAPI_NS_ENOENT        = -3,
    KAPI_NS_EEXIST        = -4,
    KAPI_NS_EBUSY         = -5
} kapi_ns_err_t;

typedef enum {
    KAPI_NS_TYPE_PID    = 0,
    KAPI_NS_TYPE_MNT    = 1,
    KAPI_NS_TYPE_NET    = 2,
    KAPI_NS_TYPE_IPC    = 3,
    KAPI_NS_TYPE_UTS    = 4,
    KAPI_NS_TYPE_USER   = 5,
    KAPI_NS_TYPE_CGROUP = 6,
    KAPI_NS_TYPE_MAX    = 7
} kapi_ns_type_t;

struct kapi_ns {
    uint32_t      id;             /* inum */
    kapi_ns_type_t type;
    char          name[KAPI_NS_NAME_MAX];
    int32_t       parent_pid;     /* creator */
    uint32_t      refcount;
    int           registered;
};

/* Subsystem lifecycle */
int kapi_ns_init(void);
void kapi_ns_exit(void);

/* Create / destroy */
kapi_ns_t *kapi_ns_create(kapi_ns_type_t type, const char *name);
int kapi_ns_destroy(kapi_ns_t *ns);
int kapi_ns_destroy_by_id(uint32_t id, kapi_ns_type_t type);

/* Lookup */
kapi_ns_t *kapi_ns_find_by_id(uint32_t id, kapi_ns_type_t type);
kapi_ns_t *kapi_ns_find_by_name(const char *name, kapi_ns_type_t type);
int kapi_ns_list(kapi_ns_type_t type, kapi_ns_t *out, uint32_t max,
                 uint32_t *count);

/* Task association */
int kapi_ns_attach(int32_t pid, kapi_ns_type_t type, kapi_ns_t *ns);
int kapi_ns_detach(int32_t pid, kapi_ns_type_t type);
kapi_ns_t *kapi_ns_get_task_ns(int32_t pid, kapi_ns_type_t type);

/* Cloning (clone-like operation returns the new namespace) */
kapi_ns_t *kapi_ns_clone(int32_t pid, kapi_ns_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_NAMESPACE_H */
