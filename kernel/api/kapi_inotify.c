#include "kapi_inotify.h"
#include "kapi.h"
#include <string.h>

typedef struct {
    int      valid;
    int      wd;
    int      fd;
    char     path[256];
    uint32_t mask;
} kapi_inotify_watch_t;

typedef struct {
    int                      valid;
    int                      fd;
    int                      cloexec;
    int                      nonblock;
    kapi_inotify_watch_t*    watches;
    int                      watch_count;
    int                      watch_capacity;
} kapi_inotify_inst_t;

static kapi_inotify_inst_t inotify_instances[KAPI_INOTIFY_MAX_INSTANCES];
static int inotify_next_fd = 500;
static int inotify_next_wd = 1;

int kapi_inotify_init1(int flags)
{
    for (int i = 0; i < KAPI_INOTIFY_MAX_INSTANCES; i++) {
        if (!inotify_instances[i].valid) {
            inotify_instances[i].valid = 1;
            inotify_instances[i].fd = inotify_next_fd++;
            inotify_instances[i].cloexec = (flags & KAPI_IN_CLOEXEC) ? 1 : 0;
            inotify_instances[i].nonblock = (flags & KAPI_IN_NONBLOCK) ? 1 : 0;
            inotify_instances[i].watches = (kapi_inotify_watch_t*)kapi_kmalloc(sizeof(kapi_inotify_watch_t) * 16);
            if (!inotify_instances[i].watches) {
                inotify_instances[i].valid = 0;
                return KAPI_ENOMEM;
            }
            inotify_instances[i].watch_count = 0;
            inotify_instances[i].watch_capacity = 16;
            return inotify_instances[i].fd;
        }
    }
    return KAPI_ENOMEM;
}

int kapi_inotify_init(void)
{
    return kapi_inotify_init1(0);
}

int kapi_inotify_add_watch(int fd, const char* pathname, uint32_t mask)
{
    if (!pathname) return KAPI_EINVAL;

    for (int i = 0; i < KAPI_INOTIFY_MAX_INSTANCES; i++) {
        if (inotify_instances[i].valid && inotify_instances[i].fd == fd) {
            kapi_inotify_inst_t* inst = &inotify_instances[i];

            for (int j = 0; j < inst->watch_count; j++) {
                if (strcmp(inst->watches[j].path, pathname) == 0) {
                    inst->watches[j].mask = mask;
                    return inst->watches[j].wd;
                }
            }

            if (inst->watch_count >= inst->watch_capacity) {
                int new_cap = inst->watch_capacity * 2;
                kapi_inotify_watch_t* new_watches = (kapi_inotify_watch_t*)kapi_kmalloc(sizeof(kapi_inotify_watch_t) * new_cap);
                if (!new_watches) return KAPI_ENOMEM;
                memcpy(new_watches, inst->watches, sizeof(kapi_inotify_watch_t) * inst->watch_count);
                kapi_kfree(inst->watches);
                inst->watches = new_watches;
                inst->watch_capacity = new_cap;
            }

            kapi_inotify_watch_t* w = &inst->watches[inst->watch_count++];
            w->valid = 1;
            w->wd = inotify_next_wd++;
            w->fd = fd;
            strncpy(w->path, pathname, sizeof(w->path) - 1);
            w->path[sizeof(w->path) - 1] = '\0';
            w->mask = mask;
            return w->wd;
        }
    }
    return KAPI_EINVAL;
}

int kapi_inotify_rm_watch(int fd, int wd)
{
    for (int i = 0; i < KAPI_INOTIFY_MAX_INSTANCES; i++) {
        if (inotify_instances[i].valid && inotify_instances[i].fd == fd) {
            kapi_inotify_inst_t* inst = &inotify_instances[i];
            for (int j = 0; j < inst->watch_count; j++) {
                if (inst->watches[j].wd == wd) {
                    inst->watches[j] = inst->watches[inst->watch_count - 1];
                    inst->watch_count--;
                    return KAPI_OK;
                }
            }
            return KAPI_EINVAL;
        }
    }
    return KAPI_EINVAL;
}

int kapi_inotify_init_module(void)
{
    memset(inotify_instances, 0, sizeof(inotify_instances));
    return KAPI_OK;
}