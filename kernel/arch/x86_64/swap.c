

#include <arch/swap.h>
#include <arch/memory.h>
#include <arch/pagecache.h>
#include <arch/fs.h>
#include <arch/spinlock.h>
#include <arch/buddy.h>
#include <string.h>

static struct {
    swap_area_t  areas[SWAP_MAX_AREAS];
    spinlock_t   global_lock;
    uint32_t     area_count;
    uint64_t     swap_in_count;
    uint64_t     swap_out_count;
    uint64_t     swap_in_pages;
    uint64_t     swap_out_pages;
    int          initialized;
} g_swap;

static inline int bitmap_test(const uint8_t* bitmap, uint64_t bit)
{
    return (bitmap[bit >> 3] >> (bit & 7)) & 1;
}

static inline void bitmap_set(uint8_t* bitmap, uint64_t bit)
{
    bitmap[bit >> 3] |= (1U << (bit & 7));
}

static inline void bitmap_clear(uint8_t* bitmap, uint64_t bit)
{
    bitmap[bit >> 3] &= ~(1U << (bit & 7));
}

static uint64_t bitmap_find_free(const uint8_t* bitmap, uint64_t total_bits)
{
    for (uint64_t i = 0; i < total_bits; i++) {
        if (!bitmap_test(bitmap, i))
            return i;
    }
    return UINT64_MAX;
}

static uint64_t bitmap_find_free_range(const uint8_t* bitmap,
                                        uint64_t total_bits, uint64_t count)
{
    if (count == 0)
        return UINT64_MAX;

    uint64_t consecutive = 0;
    uint64_t start = 0;

    for (uint64_t i = 0; i < total_bits; i++) {
        if (!bitmap_test(bitmap, i)) {
            if (consecutive == 0)
                start = i;
            consecutive++;
            if (consecutive >= count)
                return start;
        } else {
            consecutive = 0;
        }
    }
    return UINT64_MAX;
}

static int swap_read_slot(swap_area_t* area, uint64_t slot_index, void* buffer)
{
    if (!area->active || slot_index >= area->total_slots)
        return -1;

    uint64_t offset = slot_index * SWAP_SLOT_SIZE;

    if (area->type == SWAP_TYPE_PARTITION) {

        memset(buffer, 0, SWAP_SLOT_SIZE);
        return 0;
    }
    else if (area->type == SWAP_TYPE_FILE) {

        if (area->fd < 0)
            return -1;

        if (vfs_lseek(area->fd, (int64_t)offset, 0) != (int64_t)offset)
            return -1;

        int bytes = vfs_read(area->fd, buffer, SWAP_SLOT_SIZE);
        if (bytes != SWAP_SLOT_SIZE)
            return -1;

        return 0;
    }

    return -1;
}

static int swap_write_slot(swap_area_t* area, uint64_t slot_index, const void* buffer)
{
    if (!area->active || slot_index >= area->total_slots)
        return -1;

    uint64_t offset = slot_index * SWAP_SLOT_SIZE;

    if (area->type == SWAP_TYPE_PARTITION) {

        return 0;
    }
    else if (area->type == SWAP_TYPE_FILE) {

        if (area->fd < 0)
            return -1;

        if (vfs_lseek(area->fd, (int64_t)offset, 0) != (int64_t)offset)
            return -1;

        int bytes = vfs_write(area->fd, buffer, SWAP_SLOT_SIZE);
        if (bytes != SWAP_SLOT_SIZE)
            return -1;

        return 0;
    }

    return -1;
}

void swap_init(void)
{
    memset(&g_swap, 0, sizeof(g_swap));
    spin_init(&g_swap.global_lock);
    g_swap.area_count = 0;
    g_swap.swap_in_count = 0;
    g_swap.swap_out_count = 0;
    g_swap.swap_in_pages = 0;
    g_swap.swap_out_pages = 0;
    g_swap.initialized = 1;
}

int swap_add_area(uint32_t type, uint64_t device,
                  const char* file_path, uint64_t size_pages)
{
    if (!g_swap.initialized)
        return -1;

    if (type != SWAP_TYPE_PARTITION && type != SWAP_TYPE_FILE)
        return -1;

    if (size_pages == 0 || size_pages > SWAP_MAP_MAX_SLOTS)
        return -1;

    spin_lock(&g_swap.global_lock);

    if (g_swap.area_count >= SWAP_MAX_AREAS) {
        spin_unlock(&g_swap.global_lock);
        return -1;
    }

    uint64_t current_total = 0;
    for (uint32_t i = 0; i < g_swap.area_count; i++) {
        current_total += g_swap.areas[i].total_slots;
    }
    if (current_total + size_pages > UINT64_MAX) {
        spin_unlock(&g_swap.global_lock);
        return -1;
    }

    swap_area_t* area = &g_swap.areas[g_swap.area_count];
    memset(area, 0, sizeof(swap_area_t));

    area->type = type;
    area->active = 1;
    area->device = device;
    area->total_slots = size_pages;
    area->used_slots = 0;
    area->free_slots = size_pages;

    area->bitmap_size = (size_pages + 7) / 8;
    area->bitmap = (uint8_t*)memory_zalloc(area->bitmap_size);
    if (!area->bitmap) {
        spin_unlock(&g_swap.global_lock);
        return -1;
    }

    memset(area->bitmap, 0, area->bitmap_size);

    spin_init(&area->lock);
    area->alloc_count = 0;
    area->free_count = 0;

    if (type == SWAP_TYPE_FILE && file_path) {
        strncpy(area->file_path, file_path, sizeof(area->file_path) - 1);
        area->fd = vfs_open(file_path, FS_O_RDWR | FS_O_CREAT, 0);
        if (area->fd < 0) {
            memory_free(area->bitmap);
            area->bitmap = NULL;
            spin_unlock(&g_swap.global_lock);
            return -1;
        }

        uint64_t expected_size = size_pages * SWAP_SLOT_SIZE;

        uint8_t zero_page[SWAP_SLOT_SIZE];
        memset(zero_page, 0, SWAP_SLOT_SIZE);
        uint64_t current = 0;
        while (current < expected_size) {
            int n = vfs_write(area->fd, zero_page, SWAP_SLOT_SIZE);
            if (n <= 0)
                break;
            current += n;
        }

        vfs_lseek(area->fd, 0, 0);
    } else {
        area->fd = -1;
        area->file_path[0] = '\0';
    }

    g_swap.area_count++;
    spin_unlock(&g_swap.global_lock);

    return 0;
}

int swap_remove_area(uint32_t area_index)
{
    if (!g_swap.initialized)
        return -1;

    spin_lock(&g_swap.global_lock);

    if (area_index >= g_swap.area_count) {
        spin_unlock(&g_swap.global_lock);
        return -1;
    }

    swap_area_t* area = &g_swap.areas[area_index];

    if (area->used_slots != 0) {
        spin_unlock(&g_swap.global_lock);
        return -1;
    }

    area->active = 0;

    if (area->fd >= 0) {
        vfs_close(area->fd);
        area->fd = -1;
    }

    if (area->bitmap) {
        memory_free(area->bitmap);
        area->bitmap = NULL;
    }

    if (area_index < g_swap.area_count - 1) {
        memcpy(area, &g_swap.areas[g_swap.area_count - 1], sizeof(swap_area_t));
    }
    g_swap.area_count--;

    spin_unlock(&g_swap.global_lock);

    return 0;
}

swap_entry_t swap_alloc_entry(void)
{
    if (!g_swap.initialized)
        return 0;

    spin_lock(&g_swap.global_lock);

    for (uint32_t i = 0; i < g_swap.area_count; i++) {
        swap_area_t* area = &g_swap.areas[i];

        if (!area->active || area->free_slots == 0)
            continue;

        spin_lock(&area->lock);

        uint64_t slot = bitmap_find_free(area->bitmap, area->total_slots);
        if (slot == UINT64_MAX) {
            spin_unlock(&area->lock);
            continue;
        }

        bitmap_set(area->bitmap, slot);
        area->used_slots++;
        area->free_slots--;
        area->alloc_count++;

        spin_unlock(&area->lock);

        swap_entry_t entry = SWAP_ENTRY_ENCODE(i, slot);

        spin_unlock(&g_swap.global_lock);
        return entry;
    }

    spin_unlock(&g_swap.global_lock);
    return 0;
}

int swap_free_entry(swap_entry_t entry)
{
    if (!g_swap.initialized || entry == 0)
        return -1;

    uint32_t area_idx = SWAP_ENTRY_GET_AREA(entry);
    uint64_t slot_idx = SWAP_ENTRY_GET_SLOT(entry);

    spin_lock(&g_swap.global_lock);

    if (area_idx >= g_swap.area_count) {
        spin_unlock(&g_swap.global_lock);
        return -1;
    }

    swap_area_t* area = &g_swap.areas[area_idx];

    if (!area->active || slot_idx >= area->total_slots) {
        spin_unlock(&g_swap.global_lock);
        return -1;
    }

    spin_lock(&area->lock);

    if (!bitmap_test(area->bitmap, slot_idx)) {
        spin_unlock(&area->lock);
        spin_unlock(&g_swap.global_lock);
        return -1;
    }

    bitmap_clear(area->bitmap, slot_idx);
    area->used_slots--;
    area->free_slots++;
    area->free_count++;

    spin_unlock(&area->lock);
    spin_unlock(&g_swap.global_lock);

    return 0;
}

swap_entry_t swap_out(uint64_t page_phys, uint64_t vaddr)
{
    if (!g_swap.initialized || page_phys == 0)
        return 0;

    swap_entry_t entry = swap_alloc_entry();
    if (entry == 0)
        return 0;

    uint32_t area_idx = SWAP_ENTRY_GET_AREA(entry);
    uint64_t slot_idx = SWAP_ENTRY_GET_SLOT(entry);

    spin_lock(&g_swap.global_lock);
    swap_area_t* area = &g_swap.areas[area_idx];
    spin_unlock(&g_swap.global_lock);

    spin_lock(&area->lock);

    void* page_virt = (void*)(page_phys + KERNEL_VMA);
    if (page_virt == NULL) {

        page_virt = (void*)page_phys;
    }

    int ret = swap_write_slot(area, slot_idx, page_virt);
    if (ret < 0) {

        bitmap_clear(area->bitmap, slot_idx);
        area->used_slots--;
        area->free_slots++;
        spin_unlock(&area->lock);
        return 0;
    }

    spin_unlock(&area->lock);

    void* pml4 = pmap_get();
    if (pml4 && vaddr != 0) {

        pmap_unmap_page(pml4, vaddr);

        uint64_t swap_pte = SWAP_ENTRY_TO_PTE(entry);

        memory_free_physical(page_phys, 1);
    }

    spin_lock(&g_swap.global_lock);
    g_swap.swap_out_count++;
    g_swap.swap_out_pages++;
    spin_unlock(&g_swap.global_lock);

    return entry;
}

int swap_in(swap_entry_t entry, uint64_t new_page_phys, uint64_t vaddr)
{
    if (!g_swap.initialized || entry == 0 || new_page_phys == 0)
        return -1;

    uint32_t area_idx = SWAP_ENTRY_GET_AREA(entry);
    uint64_t slot_idx = SWAP_ENTRY_GET_SLOT(entry);

    if (area_idx >= g_swap.area_count) {
        return -1;
    }

    spin_lock(&g_swap.global_lock);
    swap_area_t* area = &g_swap.areas[area_idx];
    spin_unlock(&g_swap.global_lock);

    if (!area->active || slot_idx >= area->total_slots) {
        return -1;
    }

    spin_lock(&area->lock);

    if (!bitmap_test(area->bitmap, slot_idx)) {
        spin_unlock(&area->lock);
        return -1;
    }

    void* page_virt = (void*)(new_page_phys + KERNEL_VMA);
    if (page_virt == NULL) {
        page_virt = (void*)new_page_phys;
    }

    int ret = swap_read_slot(area, slot_idx, page_virt);
    if (ret < 0) {
        spin_unlock(&area->lock);
        return -1;
    }

    bitmap_clear(area->bitmap, slot_idx);
    area->used_slots--;
    area->free_slots++;
    area->free_count++;

    spin_unlock(&area->lock);

    if (vaddr != 0) {
        void* pml4 = pmap_get();
        if (pml4) {

            uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
            pmap_map_page(pml4, vaddr, new_page_phys, flags);

            __asm__ volatile ("invlpg (%0)" : : "r"(vaddr) : "memory");
        }
    }

    spin_lock(&g_swap.global_lock);
    g_swap.swap_in_count++;
    g_swap.swap_in_pages++;
    spin_unlock(&g_swap.global_lock);

    return 0;
}

void swap_get_stats(swap_stats_t* stats)
{
    if (!stats || !g_swap.initialized)
        return;

    memset(stats, 0, sizeof(swap_stats_t));

    spin_lock(&g_swap.global_lock);

    for (uint32_t i = 0; i < g_swap.area_count; i++) {
        swap_area_t* area = &g_swap.areas[i];

        if (!area->active)
            continue;

        stats->total_swap += area->total_slots;
        stats->used_swap += area->used_slots;
        stats->free_swap += area->free_slots;
    }

    stats->swap_in_count = g_swap.swap_in_count;
    stats->swap_out_count = g_swap.swap_out_count;
    stats->swap_in_pages = g_swap.swap_in_pages;
    stats->swap_out_pages = g_swap.swap_out_pages;

    spin_unlock(&g_swap.global_lock);
}

uint64_t swap_shrink_caches(uint64_t target_pages)
{
    if (!g_swap.initialized || target_pages == 0)
        return 0;

    uint64_t reclaimed = 0;

    while (reclaimed < target_pages) {

        uint64_t total_cached = 0, dirty_cached = 0;
        pagecache_get_stats(&total_cached, &dirty_cached);

        if (total_cached - dirty_cached == 0 && dirty_cached == 0)
            break;

        swap_stats_t st;
        swap_get_stats(&st);
        if (st.free_swap == 0)
            break;

        reclaimed++;

        if (reclaimed >= target_pages)
            break;
    }

    return reclaimed;
}
