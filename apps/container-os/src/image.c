#include "image.h"
#include <string.h>

static container_image_t s_images[MAX_IMAGES];
static int s_image_count = 0;
static int s_initialized = 0;

static void safe_strncpy(char *dst, const char *src, size_t n)
{
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

int image_init(void)
{
    if (s_initialized) return 0;
    memset(s_images, 0, sizeof(s_images));
    s_image_count = 0;

    safe_strncpy(s_images[0].name, "busybox", sizeof(s_images[0].name));
    safe_strncpy(s_images[0].tag, "latest", sizeof(s_images[0].tag));
    safe_strncpy(s_images[0].os, "linux", sizeof(s_images[0].os));
    safe_strncpy(s_images[0].arch, "x86_64", sizeof(s_images[0].arch));
    s_images[0].size = 2 * 1024 * 1024;
    s_images[0].ref_count = 0;
    s_image_count = 1;

    safe_strncpy(s_images[1].name, "alpine", sizeof(s_images[1].name));
    safe_strncpy(s_images[1].tag, "3.18", sizeof(s_images[1].tag));
    safe_strncpy(s_images[1].os, "linux", sizeof(s_images[1].os));
    safe_strncpy(s_images[1].arch, "x86_64", sizeof(s_images[1].arch));
    s_images[1].size = 8 * 1024 * 1024;
    s_images[1].ref_count = 0;
    s_image_count = 2;

    s_initialized = 1;
    return 0;
}

int image_list(container_image_t *buf, int max)
{
    if (!buf || max <= 0) return 0;
    int n = s_image_count < max ? s_image_count : max;
    memcpy(buf, s_images, (size_t)n * sizeof(container_image_t));
    return n;
}

container_image_t *image_find(const char *name, const char *tag)
{
    if (!name) return NULL;
    const char *t = tag ? tag : "latest";
    for (int i = 0; i < s_image_count; i++) {
        if (strcmp(s_images[i].name, name) == 0 &&
            strcmp(s_images[i].tag, t) == 0) {
            return &s_images[i];
        }
    }
    return NULL;
}

int image_pull(const char *name, const char *tag)
{
    if (!name) return -1;
    if (image_find(name, tag)) return 0;
    if (s_image_count >= MAX_IMAGES) return -1;

    const char *t = tag ? tag : "latest";
    container_image_t *img = &s_images[s_image_count];
    safe_strncpy(img->name, name, sizeof(img->name));
    safe_strncpy(img->tag, t, sizeof(img->tag));
    safe_strncpy(img->os, "linux", sizeof(img->os));
    safe_strncpy(img->arch, "x86_64", sizeof(img->arch));
    img->size = 16 * 1024 * 1024;
    img->ref_count = 0;
    s_image_count++;
    return 0;
}

int image_remove(const char *name, const char *tag)
{
    container_image_t *img = image_find(name, tag);
    if (!img) return -1;
    if (img->ref_count > 0) return -1;

    int idx = (int)(img - s_images);
    for (int i = idx; i < s_image_count - 1; i++) {
        s_images[i] = s_images[i + 1];
    }
    memset(&s_images[s_image_count - 1], 0, sizeof(container_image_t));
    s_image_count--;
    return 0;
}

int image_count(void)
{
    return s_image_count;
}
