#ifndef CONTAINER_OS_IMAGE_H
#define CONTAINER_OS_IMAGE_H

#include <stdint.h>
#include <stddef.h>

#define IMAGE_NAME_LEN   64
#define IMAGE_TAG_LEN    32
#define IMAGE_OS_LEN     32
#define IMAGE_ARCH_LEN   16
#define MAX_IMAGES       16

typedef struct {
    char name[IMAGE_NAME_LEN];
    char tag[IMAGE_TAG_LEN];
    char os[IMAGE_OS_LEN];
    char arch[IMAGE_ARCH_LEN];
    uint64_t size;
    int ref_count;
} container_image_t;

int  image_init(void);
int  image_list(container_image_t *buf, int max);
int  image_pull(const char *name, const char *tag);
int  image_remove(const char *name, const char *tag);
container_image_t *image_find(const char *name, const char *tag);
int  image_count(void);

#endif
