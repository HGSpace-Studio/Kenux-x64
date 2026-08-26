

#ifndef ARCH_BUDDY_H
#define ARCH_BUDDY_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define BUDDY_MIN_ORDER     0
#define BUDDY_MAX_ORDER     11
#define BUDDY_ORDERS        (BUDDY_MAX_ORDER + 1)

#define ZONE_DMA            0
#define ZONE_DMA32          1
#define ZONE_NORMAL         2
#define ZONE_MAX            3

#define ZONE_DMA_END        (16 * 1024 * 1024)
#define ZONE_DMA32_END      (4ULL * 1024 * 1024 * 1024)

#define GFP_KERNEL          0x00
#define GFP_DMA             0x01
#define GFP_DMA32           0x02
#define GFP_ATOMIC          0x04
#define GFP_ZERO            0x08
#define GFP_HIGHUSER        0x10

typedef struct page {
    struct page* next;
    struct page* prev;
    uint32_t     order;
    uint32_t     flags;
    uint64_t     pfn;
    uint32_t     refcount;
    int          zone_idx;
    struct kmem_cache* slab_cache;
} page_t;

#define PAGE_FLAG_USED      0x01
#define PAGE_FLAG_RESERVED  0x02
#define PAGE_FLAG_SLAB      0x04
#define PAGE_FLAG_DMA       0x08
#define PAGE_FLAG_DIRTY     0x10
#define PAGE_FLAG_LOCKED    0x20

typedef struct {
    page_t* free_list[BUDDY_ORDERS];
    uint64_t  free_count[BUDDY_ORDERS];
    spinlock_t lock;

    page_t* page_array;
    uint64_t page_array_size;
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t reserved_pages;
    uint64_t free_pages;
    uint64_t managed_pages;

    uint64_t base_pfn;
    uint64_t end_pfn;
    int      zone_type;
    const char* zone_name;
} buddy_zone_t;

typedef struct {
    buddy_zone_t* zones[ZONE_MAX];
    int nr_zones;
    page_t* global_page_array;
    uint64_t total_system_pages;
    spinlock_t global_lock;
} buddy_system_t;

int buddy_init(buddy_zone_t* zone, uint64_t base_addr, uint64_t size_bytes);
void buddy_global_init(uint64_t base_addr, uint64_t total_size);

page_t* buddy_alloc_pages(buddy_zone_t* zone, uint32_t order);
void buddy_free_pages(buddy_zone_t* zone, page_t* page);
page_t* buddy_alloc_pages_n(buddy_zone_t* zone, uint32_t n);

page_t* alloc_pages_zone(uint32_t order, int zone_hint);
page_t* alloc_pages_dma(uint32_t order);
page_t* alloc_pages_dma32(uint32_t order);

void* page_to_virt(const page_t* page);
page_t* virt_to_page(buddy_zone_t* zone, void* virt);

page_t* pfn_to_page(buddy_zone_t* zone, uint64_t pfn);
uint64_t page_to_pfn(const page_t* page);

void buddy_get_stats(buddy_zone_t* zone, uint64_t* total, uint64_t* free, uint64_t* used);
void buddy_get_global_stats(uint64_t* total, uint64_t* free, uint64_t* used);

buddy_zone_t* buddy_get_main_zone(void);
buddy_zone_t* buddy_get_zone(int zone_type);
buddy_system_t* buddy_get_system(void);

page_t* alloc_pages(uint32_t order);
void free_pages(page_t* page);
void* alloc_pages_virt(uint32_t order);
void free_pages_virt(void* virt);

void page_ref(page_t* page);
void page_unref(page_t* page);
uint32_t page_get_refcount(page_t* page);

uint64_t buddy_count_free_pages(buddy_zone_t* zone);
uint64_t buddy_count_free_pages_global(void);

int buddy_defragment(buddy_zone_t* zone);
void buddy_dump_info(buddy_zone_t* zone);

#endif