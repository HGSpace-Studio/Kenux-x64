/*
 * Kenux Advanced OS Skeleton - Container implementation
 *
 * Skeleton: container table + lifecycle state machine. Real container
 * runtime requires namespace creation, cgroup attach, and chroot/pivot.
 */

#include "kapi_container.h"
#include "kapi.h"

#include <stdio.h>
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

int kapi_container_create(const char *name,
                          const kapi_container_config_t *cfg)
{
    if (!name || !cfg) {
        return KAPI_CONTAINER_EINVAL;
    }
    if (kapi_container_find(name)) {
        return KAPI_CONTAINER_EEXIST;
    }
    return KAPI_CONTAINER_ENOTSUP;
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
    return KAPI_CONTAINER_ENOTSUP;
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
    (void)force;
    return KAPI_CONTAINER_ENOTSUP;
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
    return KAPI_CONTAINER_ENOTSUP;
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
    return KAPI_CONTAINER_ENOTSUP;
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
    return KAPI_CONTAINER_ENOTSUP;
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
    return KAPI_CONTAINER_ENOTSUP;
}

int kapi_container_image_list(char *out, size_t len)
{
    (void)out; (void)len;
    return KAPI_CONTAINER_ENOTSUP;
}
