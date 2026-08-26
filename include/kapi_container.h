#ifndef KAPI_CONTAINER_H
#define KAPI_CONTAINER_H

/*
 * Kenux Advanced OS Skeleton - Container Subsystem
 *
 * Docker/LXC-compatible container lifecycle built on kapi_namespace
 * and cgroup resource control. Skeleton: API + data structures.
 */

#include <stdint.h>
#include <stddef.h>
#include "kapi_namespace.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_CONTAINER_NAME_MAX    64
#define KAPI_CONTAINER_MAX         256
#define KAPI_CONTAINER_MAX_NAMESPACES 8

typedef struct kapi_container kapi_container_t;

typedef enum {
    KAPI_CONTAINER_OK             = 0,
    KAPI_CONTAINER_EINVAL         = -1,
    KAPI_CONTAINER_ENOMEM         = -2,
    KAPI_CONTAINER_ENOENT        = -3,
    KAPI_CONTAINER_EEXIST        = -4,
    KAPI_CONTAINER_EBUSY         = -5,
    KAPI_CONTAINER_ESTATE        = -6
} kapi_container_err_t;

typedef enum {
    KAPI_CONTAINER_STATE_CREATED  = 0,
    KAPI_CONTAINER_STATE_RUNNING  = 1,
    KAPI_CONTAINER_STATE_PAUSED   = 2,
    KAPI_CONTAINER_STATE_STOPPED  = 3,
    KAPI_CONTAINER_STATE_EXITED   = 4
} kapi_container_state_t;

typedef struct {
    char     rootfs_path[256];
    char     hostname[64];
    char     init_path[128];
    char    *argv[16];
    uint8_t  net_ns : 1;
    uint8_t  pid_ns : 1;
    uint8_t  mnt_ns : 1;
    uint8_t  ipc_ns : 1;
    uint8_t  uts_ns : 1;
    uint8_t  user_ns: 1;
} kapi_container_config_t;

struct kapi_container {
    char     name[KAPI_CONTAINER_NAME_MAX];
    kapi_container_state_t state;
    kapi_container_config_t config;
    kapi_ns_t *namespaces[KAPI_CONTAINER_MAX_NAMESPACES];
    uint32_t num_namespaces;
    int32_t  pid;
    uint64_t cgroup_id;
    int      registered;
};

/* Subsystem lifecycle */
int kapi_container_init(void);
void kapi_container_exit(void);

/* Lifecycle */
int kapi_container_create(const char *name,
                          const kapi_container_config_t *cfg);
int kapi_container_start(const char *name);
int kapi_container_stop(const char *name, int force);
int kapi_container_pause(const char *name);
int kapi_container_resume(const char *name);
int kapi_container_remove(const char *name);
kapi_container_t *kapi_container_find(const char *name);

/* Listing / inspection */
int kapi_container_list(char *out, size_t len);
int kapi_container_get_state(const char *name,
                             kapi_container_state_t *out);

/* Image management (skeletal) */
int kapi_container_image_pull(const char *ref);
int kapi_container_image_list(char *out, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_CONTAINER_H */
