#ifndef KAPI_MEMORY_EXT_H
#define KAPI_MEMORY_EXT_H

#include <stdint.h>
#include <stddef.h>
#include "kapi_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Memory zone types */
#define KAPI_ZONE_DMA         0
#define KAPI_ZONE_NORMAL      1
#define KAPI_ZONE_HIGHMEM     2
#define KAPI_ZONE_MOVABLE     3
#define KAPI_ZONE_DEVICE      4

/* Memory pool types */
#define KAPI_POOL_SLAB        1
#define KAPI_POOL_Buddy       2
#define KAPI_POOL_CMA         3
#define KAPI_POOL_VMALLOC     4

/* Memory protection flags */
#define KAPI_MEM_PROT_NOACCESS    0x001
#define KAPI_MEM_PROT_READONLY    0x002
#define KAPI_MEM_PROT_READWRITE  0x003
#define KAPI_MEM_PROT_EXEC       0x004
#define KAPI_MEM_PROT_READWRITE_EXEC 0x007

/* Memory allocation flags */
#define KAPI_ALLOC_ATOMIC        0x001
#define KAPI_ALLOC_ZERO         0x002
#define KAPI_ALLOC_NOFAIL       0x004
#define KAPI_ALLOC_ALIGN         0x008

/* Enhanced memory statistics */
typedef struct {
    uint64_t total_phys;
    uint64_t free_phys;
    uint64_t used_phys;
    uint64_t cached_phys;
    uint64_t reclaimable;
    uint64_t slab_size;
    uint64_t buddy_free[15];  /* Buddy system free pages per order */
    uint64_t buddy_total[15];
    uint64_t cma_free;
    uint64_t cma_total;
    uint64_t vmalloc_used;
    uint64_t vmalloc_total;
    uint64_t pgtables;
    uint64_t swap_total;
    uint64_t swap_free;
} kapi_mem_ext_info_t;

/* Memory pool structure */
typedef struct kapi_mem_pool kapi_mem_pool_t;

/* Memory tracking structure */
typedef struct {
    void*             ptr;
    size_t            size;
    uint32_t          flags;
    uint64_t          alloc_time;
    uint64_t          free_time;
    const char*       caller;
    uint32_t          pid;
    uint32_t          line;
    char              file[256];
} kapi_mem_track_t;

/* Memory region descriptor */
typedef struct {
    uint64_t          start;
    uint64_t          end;
    uint64_t          size;
    const char*       name;
    uint32_t          type;
    uint32_t          flags;
    uint32_t          ref_count;
} kapi_mem_region_t;

/* Enhanced memory API */
int kapi_memory_init_ext(void);
void kapi_memory_cleanup_ext(void);

int kapi_memory_get_ext_info(kapi_mem_ext_info_t* info);

/* Memory pools */
kapi_mem_pool_t* kapi_memory_create_pool(const char* name, size_t size, 
                                        uint32_t pool_type, uint32_t flags);
void kapi_memory_destroy_pool(kapi_mem_pool_t* pool);
void* kapi_memory_pool_alloc(kapi_mem_pool_t* pool, size_t size, uint32_t flags);
void kapi_memory_pool_free(kapi_mem_pool_t* pool, void* ptr);
int kapi_memory_pool_stats(kapi_mem_pool_t* pool, 
                          uint64_t* total, uint64_t* used, uint64_t* free);

/* Memory allocation tracking */
int kapi_memory_tracking_enable(void);
int kapi_memory_tracking_disable(void);
int kapi_memory_track_alloc(void* ptr, size_t size, uint32_t flags, 
                           const char* file, int line);
int kapi_memory_track_free(void* ptr);
int kapi_memory_dump_leaks(void);
int kapi_memory_get_allocations(kapi_mem_track_t** allocations, uint32_t* count);

/* Memory regions */
int kapi_memory_region_create(const char* name, uint64_t start, uint64_t size,
                            uint32_t type, uint32_t flags);
int kapi_memory_region_destroy(const char* name);
int kapi_memory_region_find(const char* name, kapi_mem_region_t* region);
int kapi_memory_region_list(kapi_mem_region_t** regions, uint32_t* count);

/* Memory hotplug */
int kapi_memory_hotplug_add(uint64_t start, uint64_t size);
int kapi_memory_hotplug_remove(uint64_t start, uint64_t size);
int kapi_memory_hotplug_online(uint64_t start, uint64_t size);
int kapi_memory_hotplug_offline(uint64_t start, uint64_t size);

/* Memory cgroups (control groups) */
typedef struct kapi_cgroup kapi_cgroup_t;

#define KAPI_CGROUP_MEMORY    0
#define KAPI_CGROUP_CPU       1
#define KAPI_CGROUP_IO       2

int kapi_cgroup_create(const char* path, uint32_t subsystem);
int kapi_cgroup_destroy(const char* path);
int kapi_cgroup_add_task(const char* path, uint32_t pid);
int kapi_cgroup_remove_task(const char* path, uint32_t pid);
int kapi_cgroup_set_memory_limit(const char* path, uint64_t limit);
int kapi_cgroup_set_cpu_shares(const char* path, uint64_t shares);
int kapi_cgroup_set_io_weight(const char* path, uint64_t weight);

/* NUMA support */
typedef struct {
    uint32_t node_id;
    uint64_t total_memory;
    uint64_t free_memory;
    uint32_t cpu_count;
    uint32_t cpus[1024];  /* Bitmap of CPUs on this node */
} kapi_numa_node_t;

int kapi_numa_init(void);
int kapi_numa_get_node_count(uint32_t* count);
int kapi_numa_get_node(uint32_t node_id, kapi_numa_node_t* node);
int kapi_numa_set_node_affinity(uint32_t pid, uint32_t node_id);
int kapi_numa_preferred_node(void* ptr, uint32_t node_id);

/* Memory allocation hints */
void kapi_memory_set_allocation_strategy(const char* strategy);
const char* kapi_memory_get_allocation_strategy(void);

/* Memory debugging utilities */
int kapi_memory_validate_address(void* ptr);
int kapi_memory_dump_slab(void);
int kapi_memory_dump_buddy(void);
int kapi_memory_dump_regions(void);

/* Memory performance monitoring */
typedef struct {
    uint64_t alloc_count;
    uint64_t free_count;
    uint64_t alloc_size;
    uint64_t free_size;
    uint64_t fragmentation;
    uint64_t latency_max;
    uint64_t latency_avg;
    uint32_t current_allocations;
} kapi_mem_perf_stats_t;

int kapi_memory_get_performance_stats(kapi_mem_perf_stats_t* stats);
int kapi_memory_enable_performance_monitoring(void);
int kapi_memory_disable_performance_monitoring(void);

#ifdef __cplusplus
}
#endif

#endif