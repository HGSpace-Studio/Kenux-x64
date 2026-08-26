#ifndef CONTAINER_OS_MANAGER_H
#define CONTAINER_OS_MANAGER_H

#include "container.h"
#include <stddef.h>

typedef struct {
    container_t containers[MAX_CONTAINERS];
    int count;
} container_manager_t;

void manager_init(void);
int  manager_create(const char *name, const char *image, const char *command);
int  manager_start(int index);
int  manager_stop(int index);
int  manager_remove(int index);
container_t *manager_get(int index);
int  manager_count(void);
int  manager_find_by_name(const char *name);

#endif
