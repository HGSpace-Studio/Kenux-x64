/*
 * Kenux Advanced OS Skeleton - Container implementation
 *
 * Skeleton: container table + lifecycle state machine. Real container
 * runtime requires namespace creation, cgroup attach, and chroot/pivot.
 */

#include "kapi_container.h"
#include "kapi.h"

#include <string.h>

static kapi_container_t kapi_container_table[KAPI_CONTAINER_MAX];
static int kapi_container_initialized = 0;

int kapi_container_init(void)
{
    if (kapi_container_initialized) {
        return KAPI_CONTAINER_OK;
    }
    memset(kapi_container_table, 0, sizeof(kapi_container_table));
    kapi_container_initialized = 1;
    return KAPI_CONTAINER_OK;
}

void kapi_container_exit(void)
{
    memset(kapi_container_table, 0, sizeof(kapi_container_table));
    kapi_container_initialized = 0;
}

static kapi_container_t *container_slot_alloc(void)
{
    for (int i = 0; i < KAPI_CONTAINER_MAX; i++) {
        if (!kapi_container_table[i].registered) {
            return &kapi_container_table[i];
        }
    }
    return NULL;
}

int kapi_container_create(const char *name,
                          const kapi_container_config_t *cfg)
{
    if (!name || !cfg) {
        return KAPI_CONTAINER_EINVAL;
    }
    if (kapi_container_find(name)) {
        return KAPI_CONTAINER_EEXIST;
    }
    kapi_container_t *c = container_slot_alloc();
    if (!c) {
        return KAPI_CONTAINER_ENOMEM;
    }
    memset(c, 0, sizeof(*c));
    strncpy(c->name, name, KAPI_CONTAINER_NAME_MAX - 1);
    c->config = *cfg;
    c->state = KAPI_CONTAINER_STATE_CREATED;
    c->registered = 1;

    /* Allocate requested namespaces */
    kapi_ns_type_t ns_types[] = {
        cfg->net_ns  ? KAPI_NS_TYPE_NET   : KAPI_NS_TYPE_MAX,
        cfg->pid_ns  ? KAPI_NS_TYPE_PID   : KAPI_NS_TYPE_MAX,
        cfg->mnt_ns  ? KAPI_NS_TYPE_MNT   : KAPI_NS_TYPE_MAX,
        cfg->ipc_ns  ? KAPI_NS_TYPE_IPC   : KAPI_NS_TYPE_MAX,
        cfg->uts_ns  ? KAPI_NS_TYPE_UTS   : KAPI_NS_TYPE_MAX,
        cfg->user_ns ? KAPI_NS_TYPE_USER  : KAPI_NS_TYPE_MAX
    };
    for (int i = 0; i < 6; i++) {
        if (ns_types[i] == KAPI_NS_TYPE_MAX) {
            continue;
        }
        kapi_ns_t *ns = kapi_ns_create(ns_types[i], c->name);
        if (ns && c->num_namespaces < KAPI_CONTAINER_MAX_NAMESPACES) {
            c->namespaces[c->num_namespaces++] = ns;
        }
    }
    /* TODO: cgroup creation under the container's cgroup_id */
    return KAPI_CONTAINER_OK;
}

int kapi_container_start(const char *name)
{
    kapi_container_t *c = kapi_container_find(name);
    if (!c) {
        return KAPI_CONTAINER_ENOENT;
    }
    if (c->state != KAPI_CONTAINER_STATE_CREATED &&
        c->state != KAPI_CONTAINER_STATE_STOPPED) {
        return KAPI_CONTAINER_ESTATE;
    }
    /* TODO: fork into namespaces, pivot_root, exec init_path */
    c->state = KAPI_CONTAINER_STATE_RUNNING;
    return KAPI_CONTAINER_OK;
}

int kapi_container_stop(const char *name, int force)
{
    kapi_container_t *c = kapi_container_find(name);
    if (!c) {
        return KAPI_CONTAINER_ENOENT;
    }
    if (c->state != KAPI_CONTAINER_STATE_RUNNING &&
        c->state != KAPI_CONTAINER_STATE_PAUSED) {
        return KAPI_CONTAINER_ESTATE;
    }
    /* TODO: signal init (SIGTERM, then SIGKILL if force) */
    (void)force;
    c->state = KAPI_CONTAINER_STATE_STOPPED;
    return KAPI_CONTAINER_OK;
}

int kapi_container_pause(const char *name)
{
    kapi_container_t *c = kapi_container_find(name);
    if (!c) {
        return KAPI_CONTAINER_ENOENT;
    }
    if (c->state != KAPI_CONTAINER_STATE_RUNNING) {
        return KAPI_CONTAINER_ESTATE;
    }
    /* TODO: freezer cgroup state FROZEN */
    c->state = KAPI_CONTAINER_STATE_PAUSED;
    return KAPI_CONTAINER_OK;
}

int kapi_container_resume(const char *name)
{
    kapi_container_t *c = kapi_container_find(name);
    if (!c) {
        return KAPI_CONTAINER_ENOENT;
    }
    if (c->state != KAPI_CONTAINER_STATE_PAUSED) {
        return KAPI_CONTAINER_ESTATE;
    }
    c->state = KAPI_CONTAINER_STATE_RUNNING;
    return KAPI_CONTAINER_OK;
}

int kapi_container_remove(const char *name)
{
    kapi_container_t *c = kapi_container_find(name);
    if (!c) {
        return KAPI_CONTAINER_ENOENT;
    }
    if (c->state == KAPI_CONTAINER_STATE_RUNNING) {
        return KAPI_CONTAINER_EBUSY;
    }
    /* TODO: destroy namespaces, remove cgroup */
    memset(c, 0, sizeof(*c));
    return KAPI_CONTAINER_OK;
}

kapi_container_t *kapi_container_find(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (int i = 0; i < KAPI_CONTAINER_MAX; i++) {
        if (kapi_container_table[i].registered &&
            strncmp(kapi_container_table[i].name, name,
                    KAPI_CONTAINER_NAME_MAX) == 0) {
            return &kapi_container_table[i];
        }
    }
    return NULL;
}

int kapi_container_list(char *out, size_t len)
{
    if (!out) {
        return KAPI_CONTAINER_EINVAL;
    }
    size_t off = 0;
    for (int i = 0; i < KAPI_CONTAINER_MAX; i++) {
        if (kapi_container_table[i].registered) {
            size_t need = strlen(kapi_container_table[i].name) + 2;
            if (off + need > len) {
                return KAPI_CONTAINER_ENOMEM;
            }
            off += (size_t)snprintf(out + off, len - off, "%s ",
                                    kapi_container_table[i].name);
        }
    }
    return KAPI_CONTAINER_OK;
}

int kapi_container_get_state(const char *name,
                             kapi_container_state_t *out)
{
    kapi_container_t *c = kapi_container_find(name);
    if (!c || !out) {
        return KAPI_CONTAINER_ENOENT;
    }
    *out = c->state;
    return KAPI_CONTAINER_OK;
}

int kapi_container_image_pull(const char *ref)
{
    (void)ref;
    /* TODO: fetch image from registry + unpack layer tarballs */
    return KAPI_CONTAINER_OK;
}

int kapi_container_image_list(char *out, size_t len)
{
    (void)out; (void)len;
    /* TODO: enumerate local image store */
    return KAPI_CONTAINER_OK;
}
