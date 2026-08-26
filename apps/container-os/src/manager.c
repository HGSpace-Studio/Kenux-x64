#include "manager.h"
#include "image.h"
#include <string.h>

static container_manager_t s_mgr;
static int s_initialized = 0;

void manager_init(void)
{
    if (s_initialized) return;
    memset(&s_mgr, 0, sizeof(s_mgr));
    image_init();
    s_initialized = 1;
}

int manager_create(const char *name, const char *image, const char *command)
{
    if (s_mgr.count >= MAX_CONTAINERS) return -1;
    if (manager_find_by_name(name) >= 0) return -1;

    int idx = s_mgr.count;
    if (container_create(&s_mgr.containers[idx], name, image, command) < 0)
        return -1;
    s_mgr.count++;
    return idx;
}

int manager_start(int index)
{
    if (index < 0 || index >= s_mgr.count) return -1;
    return container_start(&s_mgr.containers[index]);
}

int manager_stop(int index)
{
    if (index < 0 || index >= s_mgr.count) return -1;
    return container_stop(&s_mgr.containers[index]);
}

int manager_remove(int index)
{
    if (index < 0 || index >= s_mgr.count) return -1;
    container_destroy(&s_mgr.containers[index]);
    for (int i = index; i < s_mgr.count - 1; i++) {
        s_mgr.containers[i] = s_mgr.containers[i + 1];
    }
    memset(&s_mgr.containers[s_mgr.count - 1], 0, sizeof(container_t));
    s_mgr.count--;
    return 0;
}

container_t *manager_get(int index)
{
    if (index < 0 || index >= s_mgr.count) return NULL;
    return &s_mgr.containers[index];
}

int manager_count(void)
{
    return s_mgr.count;
}

int manager_find_by_name(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < s_mgr.count; i++) {
        if (strcmp(s_mgr.containers[i].name, name) == 0)
            return i;
    }
    return -1;
}
