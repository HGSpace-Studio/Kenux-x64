#include "volume.h"
#include <string.h>

static volume_t s_volumes[MAX_VOLUMES];
static int s_volume_count = 0;
static int s_initialized = 0;

static void safe_strncpy(char *dst, const char *src, size_t n)
{
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

int volume_init(void)
{
    if (s_initialized) return 0;
    memset(s_volumes, 0, sizeof(s_volumes));
    s_volume_count = 0;

    safe_strncpy(s_volumes[0].name, "data_vol", sizeof(s_volumes[0].name));
    safe_strncpy(s_volumes[0].mount_path, "/data", sizeof(s_volumes[0].mount_path));
    s_volumes[0].size = 64 * 1024 * 1024;
    s_volumes[0].used = 12 * 1024 * 1024;
    s_volumes[0].in_use = 0;
    s_volume_count = 1;

    s_initialized = 1;
    return 0;
}

int volume_create(const char *name, uint64_t size)
{
    if (!name || s_volume_count >= MAX_VOLUMES) return -1;
    if (volume_find(name)) return -1;

    volume_t *v = &s_volumes[s_volume_count];
    safe_strncpy(v->name, name, sizeof(v->name));
    safe_strncpy(v->mount_path, "/mnt/", sizeof(v->mount_path));
    strncat(v->mount_path, name, sizeof(v->mount_path) - strlen(v->mount_path) - 1);
    v->size = size;
    v->used = 0;
    v->in_use = 0;
    s_volume_count++;
    return 0;
}

int volume_remove(const char *name)
{
    volume_t *v = volume_find(name);
    if (!v) return -1;
    if (v->in_use) return -1;

    int idx = (int)(v - s_volumes);
    for (int i = idx; i < s_volume_count - 1; i++) {
        s_volumes[i] = s_volumes[i + 1];
    }
    memset(&s_volumes[s_volume_count - 1], 0, sizeof(volume_t));
    s_volume_count--;
    return 0;
}

int volume_list(volume_t *buf, int max)
{
    if (!buf || max <= 0) return 0;
    int n = s_volume_count < max ? s_volume_count : max;
    memcpy(buf, s_volumes, (size_t)n * sizeof(volume_t));
    return n;
}

volume_t *volume_find(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < s_volume_count; i++) {
        if (strcmp(s_volumes[i].name, name) == 0)
            return &s_volumes[i];
    }
    return NULL;
}

int volume_count(void)
{
    return s_volume_count;
}
