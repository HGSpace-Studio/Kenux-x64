#ifndef CONTAINER_OS_CONTAINER_H
#define CONTAINER_OS_CONTAINER_H

#include <stdint.h>
#include <stddef.h>

#define CONTAINER_ID_LEN     33
#define CONTAINER_NAME_LEN   64
#define CONTAINER_IMAGE_LEN  64
#define CONTAINER_CMD_LEN    128
#define CONTAINER_LOG_SIZE   4096
#define MAX_CONTAINERS      16

typedef enum {
    CONTAINER_CREATED = 0,
    CONTAINER_RUNNING,
    CONTAINER_STOPPED,
    CONTAINER_PAUSED,
    CONTAINER_ERROR
} container_status_t;

typedef struct {
    int cpu_limit;
    uint64_t memory_limit;
} resource_limits_t;

typedef struct {
    char id[CONTAINER_ID_LEN];
    char name[CONTAINER_NAME_LEN];
    char image[CONTAINER_IMAGE_LEN];
    char command[CONTAINER_CMD_LEN];
    container_status_t status;
    resource_limits_t resources;
    char logs[CONTAINER_LOG_SIZE];
    int log_len;
    uint32_t pid;
} container_t;

int  container_create(container_t *c, const char *name, const char *image,
                      const char *command);
int  container_start(container_t *c);
int  container_stop(container_t *c);
int  container_destroy(container_t *c);
void container_append_log(container_t *c, const char *msg);
const char *container_status_str(container_status_t status);

#endif
