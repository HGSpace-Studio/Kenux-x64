

#ifndef KAPI_MEMORY_H
#define KAPI_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_MEM_DEFAULT     0x00
#define KAPI_MEM_KERNEL      0x01
#define KAPI_MEM_USER        0x02
#define KAPI_MEM_DMA         0x04
#define KAPI_MEM_UNCACHED    0x08
#define KAPI_MEM_WRITEBACK   0x10
#define KAPI_MEM_WRCOMBINE   0x20
#define KAPI_MEM_ZEROED      0x40

#define KAPI_PROT_NONE       0x00
#define KAPI_PROT_READ       0x01
#define KAPI_PROT_WRITE      0x02
#define KAPI_PROT_EXEC       0x04

#define KAPI_MAP_SHARED      0x01
#define KAPI_MAP_PRIVATE     0x02
#define KAPI_MAP_FIXED       0x04
#define KAPI_MAP_ANONYMOUS   0x08
#define KAPI_MAP_GROWSDOWN   0x10
#define KAPI_MAP_GROWSUP     0x20
#define KAPI_MAP_DENYWRITE   0x40
#define KAPI_MAP_EXECUTABLE  0x80
#define KAPI_MAP_LOCKED      0x100
#define KAPI_MAP_NORESERVE   0x200

#define KAPI_PAGE_SIZE       4096UL
#define KAPI_PAGE_SHIFT      12
#define KAPI_PAGE_MASK       (~(KAPI_PAGE_SIZE - 1))

#define KAPI_HUGE_2MB        (2UL * 1024UL * 1024UL)
#define KAPI_HUGE_1GB        (1UL * 1024UL * 1024UL * 1024UL)

typedef struct {
    uint64_t total;
    uint64_t free;
    uint64_t used;
    uint64_t cached;
    uint64_t total_pages;
    uint64_t free_pages;
    uint64_t kernel_total;
    uint64_t kernel_used;
    uint64_t user_total;
    uint64_t user_used;
    uint64_t dma_total;
    uint64_t dma_used;
    uint64_t slab_total;
    uint64_t slab_used;
    uint64_t page_tables;
    uint64_t vmalloc_total;
    uint64_t vmalloc_used;
} kapi_mem_info_t;

typedef struct {
    void*    base;
    size_t   size;
    uint32_t flags;
    bool     in_use;
    bool     is_kernel;
    bool     is_dma;
    uint64_t paddr;
} kapi_alloc_info_t;

typedef struct {
    size_t allocations;
    size_t deallocations;
    size_t total_allocated;
    size_t total_freed;
    size_t peak_usage;
    size_t current_usage;
    size_t largest_allocation;
    size_t smallest_allocation;
    size_t failed_allocations;
    size_t realloc_count;
} kapi_malloc_stats_t;

typedef struct {
    uint64_t total_pages;
    uint64_t free_pages;
    uint64_t allocated_pages;
    uint64_t reserved_pages;
    uint64_t high_water_mark;
    uint64_t low_water_mark;
    uint64_t alloc_count;
    uint64_t free_count;
    uint64_t fail_count;
    uint64_t fragment_count;
    double   fragmentation_ratio;
} kapi_page_stats_t;

typedef struct {
    uint64_t vma_count;
    uint64_t total_vmas;
    uint64_t mapped_memory;
    uint64_t shared_memory;
    uint64_t private_memory;
    uint64_t anonymous_memory;
    uint64_t code_size;
    uint64_t data_size;
    uint64_t heap_size;
    uint64_t stack_size;
    uint64_t mmap_base;
    uint64_t mmap_end;
    uint64_t brk_start;
    uint64_t brk_end;
} kapi_vma_stats_t;

void* kapi_malloc(size_t size);

void kapi_free(void* ptr);

void* kapi_realloc(void* ptr, size_t size);

void* kapi_calloc(size_t count, size_t size);

void* kapi_memalign(size_t alignment, size_t size);

void* kapi_valloc(size_t size);

void* kapi_pvalloc(size_t size);

int kapi_posix_memalign(void** memptr, size_t alignment, size_t size);

char* kapi_strdup(const char* str);

char* kapi_strndup(const char* str, size_t n);

size_t kapi_malloc_usable_size(void* ptr);

int kapi_malloc_trim(size_t pad);

void kapi_malloc_stats(kapi_malloc_stats_t* stats);

int kapi_mallopt(int param, int value);

uint64_t kapi_malloc_footprint(void);

uint64_t kapi_malloc_max_footprint(void);

uint64_t kapi_page_alloc(int count, uint32_t flags);

void kapi_page_free(uint64_t paddr, int count);

uint64_t kapi_page_alloc_contiguous(int count, uint32_t flags);

uint64_t kapi_hugepage_alloc(int order, uint32_t flags);

void kapi_hugepage_free(uint64_t paddr, int order);

int kapi_page_reserve(uint64_t paddr, int count);

int kapi_page_release(uint64_t paddr, int count);

bool kapi_page_is_reserved(uint64_t paddr);

bool kapi_page_is_free(uint64_t paddr);

uint64_t kapi_virt_to_phys(void* vaddr);

void* kapi_phys_to_virt(uint64_t paddr);

int kapi_page_set_flags(uint64_t vaddr, size_t size, uint64_t flags);

int kapi_page_get_flags(uint64_t vaddr);

uint64_t kapi_mmap(uint64_t vaddr, uint64_t paddr, size_t size, int prot);

int kapi_munmap(uint64_t vaddr, size_t size);

int kapi_mprotect(uint64_t vaddr, size_t size, int prot);

int kapi_mlock(const void* addr, size_t len);

int kapi_munlock(const void* addr, size_t len);

int kapi_msync(void* addr, size_t len, int flags);

int kapi_madvise(void* addr, size_t len, int advice);

int kapi_mincore(void* addr, size_t len, unsigned char* vec);

void* kapi_shmget(key_t key, size_t size, int shmflg);

int kapi_shmat(int shmid, const void* shmaddr, int shmflg);

int kapi_shmdt(const void* shmaddr);

int kapi_shmctl(int shmid, int cmd, struct shmid_ds* buf);

int kapi_mem_get_info(kapi_mem_info_t* info);

uint64_t kapi_mem_get_usage(void);

double kapi_mem_get_usage_percent(void);

int kapi_mem_get_page_stats(kapi_page_stats_t* stats);

int kapi_mem_get_vma_stats(kapi_vma_stats_t* stats);

int kapi_mem_get_alloc_info(void* ptr, kapi_alloc_info_t* info);

void kapi_mem_compact(void);

void kapi_mem_defrag(void);

int kapi_mem_pressure_level(void);

bool kapi_mem_available(size_t size);

int kapi_mem_set_limit(size_t limit);

size_t kapi_mem_get_limit(void);

int kapi_mem_enable_overcommit(bool enable);

bool kapi_mem_is_overcommit_enabled(void);

void kapi_mem_dump_stats(void);

void kapi_mem_dump_maps(void);

#ifdef __cplusplus
}
#endif

#endif