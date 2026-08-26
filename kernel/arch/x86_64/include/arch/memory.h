#ifndef ARCH_X86_64_MEMORY_H
#define ARCH_X86_64_MEMORY_H

#include <arch/types.h>

#define MEMORY_POOL_SIZE     2048
#define MEMORY_BLOCK_SIZE    4096
#define MEMORY_MAX_ORDER     12

#define PAGE_SIZE            4096
#define PAGE_SHIFT           12
#define PAGE_PRESENT         (1ULL << 0)
#define PAGE_WRITABLE        (1ULL << 1)
#define PAGE_USER            (1ULL << 2)
#define PAGE_NO_EXEC         (1ULL << 63)
#define PAGE_GLOBAL          (1ULL << 8)
#define PAGE_SIZE_2M         (1ULL << 7)

#define KERNEL_VMA           0xFFFFFFFF80000000ULL
#define KERNEL_VMA_END       0xFFFFFFFFA0000000ULL
#define USER_VMA_START       0x0000000000400000ULL
#define USER_VMA_END         0x0000000000800000ULL

#define PTE_FLAGS(addr, flags) (((uint64_t)(addr)) | (flags))
#define PTE_ADDR(pte)          ((pte) & 0x000FFFFFFFFFF000ULL)
#define PTE_FLAGS_ONLY(pte)    ((pte) & 0xFFF0000000000FFFULL)

typedef struct {
    uint64_t address;
    uint64_t size;
    uint8_t  used;
    uint8_t  order;
} memory_block_t;

typedef struct {
    memory_block_t blocks[MEMORY_POOL_SIZE];
    uint64_t total_size;
    uint64_t used_size;
    uint64_t alloc_count;
    uint64_t free_count;
} memory_pool_t;

typedef struct {
    uint64_t start;
    uint64_t end;
    uint64_t flags;
    uint64_t type;
    char     name[32];
} vma_region_t;

#define VMA_MAX_REGIONS 128

typedef struct {
    vma_region_t regions[VMA_MAX_REGIONS];
    int count;
    uint64_t heap_start;
    uint64_t heap_end;
    uint64_t stack_top;
} vma_t;

void memory_init(void);
void* memory_alloc(uint64_t size);
void  memory_free(void* ptr);
uint64_t memory_get_free(void);
uint64_t memory_get_total(void);

void*  memory_alloc_aligned(uint64_t size, uint64_t alignment);
void*  memory_realloc(void* ptr, uint64_t old_size, uint64_t new_size);
void*  memory_zalloc(uint64_t size);
uint64_t memory_alloc_physical(uint64_t count);
void   memory_free_physical(uint64_t addr, uint64_t count);
void   memory_get_stats(uint64_t* total, uint64_t* free, uint64_t* alloc_count, uint64_t* free_count);

void   vma_init(vma_t* vma);
int    vma_map(vma_t* vma, uint64_t start, uint64_t end, uint64_t flags, const char* name);
int    vma_unmap(vma_t* vma, uint64_t start, uint64_t end);
vma_region_t* vma_find(vma_t* vma, uint64_t addr);
uint64_t vma_alloc_range(vma_t* vma, uint64_t size, uint64_t align);

void   pmap_init(void);
void*  pmap_create(void);
void   pmap_switch(void* pml4);
void*  pmap_get(void);
int    pmap_map_page(void* pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags);
void   pmap_unmap_page(void* pml4, uint64_t vaddr);
uint64_t pmap_get_physical(void* pml4, uint64_t vaddr);

#endif
