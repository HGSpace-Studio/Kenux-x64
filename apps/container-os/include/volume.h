#ifndef CONTAINER_OS_VOLUME_H
#define CONTAINER_OS_VOLUME_H

#include <stdint.h>
#include <stddef.h>

#define VOLUME_NAME_LEN   64
#define VOLUME_PATH_LEN   128
#define MAX_VOLUMES       16

typedef struct {
    char name[VOLUME_NAME_LEN];
    char mount_path[VOLUME_PATH_LEN];
    uint64_t size;
    uint64_t used;
    int in_use;
} volume_t;

int  volume_init(void);
int  volume_create(const char *name, uint64_t size);
int  volume_remove(const char *name);
int  volume_list(volume_t *buf, int max);
volume_t *volume_find(const char *name);
int  volume_count(void);

#endif
