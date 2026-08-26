

#ifndef ARCH_SWAP_H
#define ARCH_SWAP_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define SWAP_SLOT_SIZE             4096
#define SWAP_MAP_MAX_SLOTS         262144
#define SWAP_MAX_AREAS             16

#define SWAP_TYPE_NONE             0
#define SWAP_TYPE_PARTITION        1
#define SWAP_TYPE_FILE             2

#define SWAP_ENTRY_AREA_SHIFT      56
#define SWAP_ENTRY_SLOT_MASK       0x00FFFFFFFFFFFFFFULL
#define SWAP_ENTRY_AREA_MASK       (0xFFULL << SWAP_ENTRY_AREA_SHIFT)

#define SWAP_ENTRY_GET_AREA(entry)    ((uint32_t)((entry) >> SWAP_ENTRY_AREA_SHIFT))
#define SWAP_ENTRY_GET_SLOT(entry)    ((uint64_t)((entry) & SWAP_ENTRY_SLOT_MASK))

#define SWAP_ENTRY_ENCODE(area, slot)  \
    (((uint64_t)(area) << SWAP_ENTRY_AREA_SHIFT) | ((uint64_t)(slot) & SWAP_ENTRY_SLOT_MASK))

#define SWAP_PTE_IS_SWAP(pte)         (((pte) & PAGE_PRESENT) == 0 && ((pte) & 0x1))
#define SWAP_PTE_TO_ENTRY(pte)        ((pte) >> 1)
#define SWAP_ENTRY_TO_PTE(entry)      (((entry) << 1) | 0x1)

typedef struct swap_area {
    uint32_t     type;
    int32_t      active;

    uint64_t     device;
    char         file_path[256];
    int          fd;

    uint64_t     total_slots;
    uint64_t     used_slots;
    uint64_t     free_slots;

    uint8_t*     bitmap;
    uint64_t     bitmap_size;

    spinlock_t   lock;

    uint64_t     alloc_count;
    uint64_t     free_count;
} swap_area_t;

typedef uint64_t swap_entry_t;

typedef struct {
    uint64_t     total_swap;
    uint64_t     used_swap;
    uint64_t     free_swap;
    uint64_t     swap_in_count;
    uint64_t     swap_out_count;
    uint64_t     swap_in_pages;
    uint64_t     swap_out_pages;
} swap_stats_t;

void swap_init(void);

int swap_add_area(uint32_t type, uint64_t device,
                  const char* file_path, uint64_t size_pages);

int swap_remove_area(uint32_t area_index);

swap_entry_t swap_alloc_entry(void);

int swap_free_entry(swap_entry_t entry);

swap_entry_t swap_out(uint64_t page_phys, uint64_t vaddr);

int swap_in(swap_entry_t entry, uint64_t new_page_phys, uint64_t vaddr);

void swap_get_stats(swap_stats_t* stats);

uint64_t swap_shrink_caches(uint64_t target_pages);

#endif
