

#include "kapi_percpu.h"
#include "kapi.h"
#include "kapi_smp.h"
#include <arch/slab.h>
#include <string.h>

#define KAPI_PERCPU_MAX_AREAS  64

static kapi_percpu_area_t g_percpu_areas[KAPI_PERCPU_MAX_AREAS];
static int g_percpu_init_done = 0;

int kapi_percpu_init(void)
{
    memset(g_percpu_areas, 0, sizeof(g_percpu_areas));
    g_percpu_init_done = 1;
    return 0;
}

int kapi_percpu_alloc(size_t size)
{
    if (!g_percpu_init_done) kapi_percpu_init();

    for (int i = 0; i < KAPI_PERCPU_MAX_AREAS; i++) {
        if (!g_percpu_areas[i].in_use) {
            /* 单 CPU 简化实现：分配 KAPI_PERCPU_MAX_CPUS 份 */
            void* data = kzalloc(size * KAPI_PERCPU_MAX_CPUS);
            if (!data) return -1;
            g_percpu_areas[i].data = data;
            g_percpu_areas[i].size = size;
            g_percpu_areas[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

void kapi_percpu_free(int area_id)
{
    if (area_id < 0 || area_id >= KAPI_PERCPU_MAX_AREAS) return;
    if (!g_percpu_areas[area_id].in_use) return;
    kfree(g_percpu_areas[area_id].data);
    g_percpu_areas[area_id].data = NULL;
    g_percpu_areas[area_id].size = 0;
    g_percpu_areas[area_id].in_use = 0;
}

void* kapi_percpu_ptr(int area_id)
{
    return kapi_percpu_ptr_on_cpu(area_id, kapi_percpu_cpu_id());
}

void* kapi_percpu_ptr_on_cpu(int area_id, int cpu)
{
    if (area_id < 0 || area_id >= KAPI_PERCPU_MAX_AREAS) return NULL;
    if (!g_percpu_areas[area_id].in_use) return NULL;
    if (cpu < 0) cpu = 0;
    if (cpu >= KAPI_PERCPU_MAX_CPUS) cpu = KAPI_PERCPU_MAX_CPUS - 1;
    uint8_t* base = (uint8_t*)g_percpu_areas[area_id].data;
    return base + (size_t)cpu * g_percpu_areas[area_id].size;
}

int kapi_percpu_cpu_id(void)
{
    return kapi_smp_processor_id();
}

void kapi_percpu_add(int area_id, int64_t delta)
{
    int64_t* p = (int64_t*)kapi_percpu_ptr(area_id);
    if (p) *p += delta;
}

int64_t kapi_percpu_read(int area_id)
{
    int64_t* p = (int64_t*)kapi_percpu_ptr(area_id);
    return p ? *p : 0;
}

void kapi_percpu_write(int area_id, int64_t value)
{
    int64_t* p = (int64_t*)kapi_percpu_ptr(area_id);
    if (p) *p = value;
}
