#include <arch/buddy.h>
#include <arch/memory.h>
#include <string.h>

static buddy_zone_t zones[ZONE_MAX];
static buddy_system_t buddy_system;
static int buddy_initialized = 0;

static const char* zone_names[ZONE_MAX] = {
    "DMA",
    "DMA32",
    "Normal"
};

static void __list_del(page_t* page)
{
    if (page->prev) page->prev->next = page->next;
    if (page->next) page->next->prev = page->prev;
    page->next = page->prev = NULL;
}

static void __list_add(buddy_zone_t* zone, uint32_t order, page_t* page)
{
    page->next = zone->free_list[order];
    page->prev = NULL;
    if (zone->free_list[order]) {
        zone->free_list[order]->prev = page;
    }
    zone->free_list[order] = page;
    zone->free_count[order]++;
}

static page_t* __list_pop(buddy_zone_t* zone, uint32_t order)
{
    page_t* page = zone->free_list[order];
    if (!page) return NULL;
    zone->free_list[order] = page->next;
    if (page->next) page->next->prev = NULL;
    page->next = page->prev = NULL;
    zone->free_count[order]--;
    return page;
}

static uint64_t __buddy_pfn(uint64_t pfn, uint32_t order)
{
    return pfn ^ (1ULL << order);
}

static int __in_freelist(buddy_zone_t* zone, uint32_t order, page_t* page)
{
    page_t* p = zone->free_list[order];
    while (p) {
        if (p == page) return 1;
        p = p->next;
    }
    return 0;
}

static int __phys_to_zone_type(uint64_t phys)
{
    if (phys < ZONE_DMA_END) return ZONE_DMA;
    if (phys < ZONE_DMA32_END) return ZONE_DMA32;
    return ZONE_NORMAL;
}

int buddy_init(buddy_zone_t* zone, uint64_t base_addr, uint64_t size_bytes)
{
    memset(zone, 0, sizeof(buddy_zone_t));
    spin_init(&zone->lock);

    zone->base_pfn = base_addr / PAGE_SIZE;
    zone->end_pfn = zone->base_pfn + (size_bytes / PAGE_SIZE);
    zone->total_pages = zone->end_pfn - zone->base_pfn;

    if (zone->total_pages == 0) return -1;

    uint64_t array_pages = (zone->total_pages * sizeof(page_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    zone->page_array = (page_t*)base_addr;
    zone->page_array_size = zone->total_pages;

    memset(zone->page_array, 0, array_pages * PAGE_SIZE);

    for (uint64_t i = 0; i < zone->total_pages; i++) {
        page_t* p = &zone->page_array[i];
        p->pfn = zone->base_pfn + i;
        p->order = 0;
        p->flags = 0;
        p->next = p->prev = NULL;
        p->slab_cache = NULL;
        p->refcount = 0;
        p->zone_idx = zone->zone_type;
    }

    zone->reserved_pages = array_pages;
    zone->used_pages = 0;

    uint64_t free_start = array_pages;
    uint64_t free_pages = zone->total_pages - array_pages;

    for (uint64_t i = 0; i < array_pages && i < zone->total_pages; i++) {
        zone->page_array[i].flags |= PAGE_FLAG_RESERVED;
    }

    while (free_pages > 0) {
        uint32_t order = BUDDY_MAX_ORDER;
        while ((1ULL << order) > free_pages && order > BUDDY_MIN_ORDER) {
            order--;
        }

        page_t* page = &zone->page_array[free_start];
        page->order = order;
        __list_add(zone, order, page);

        free_start += (1ULL << order);
        free_pages -= (1ULL << order);
    }

    zone->free_pages = 0;
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++) {
        zone->free_pages += zone->free_count[i] * (1ULL << i);
    }
    zone->managed_pages = zone->free_pages;

    return 0;
}

void buddy_global_init(uint64_t base_addr, uint64_t total_size)
{
    if (buddy_initialized) return;

    memset(&buddy_system, 0, sizeof(buddy_system_t));
    spin_init(&buddy_system.global_lock);
    buddy_system.nr_zones = 0;

    uint64_t dma_end = ZONE_DMA_END;
    if (dma_end > total_size) dma_end = total_size;

    uint64_t dma_size = dma_end - (base_addr < dma_end ? base_addr : dma_end);
    if (dma_size > 0 && dma_size >= PAGE_SIZE) {
        zones[ZONE_DMA].zone_type = ZONE_DMA;
        zones[ZONE_DMA].zone_name = zone_names[ZONE_DMA];
        buddy_init(&zones[ZONE_DMA], base_addr, dma_size);
        buddy_system.zones[ZONE_DMA] = &zones[ZONE_DMA];
        buddy_system.nr_zones++;
    }

    uint64_t dma32_end = ZONE_DMA32_END;
    if (dma32_end > total_size) dma32_end = total_size;

    uint64_t dma32_start = dma_end;
    uint64_t dma32_size = dma32_end - dma32_start;
    if (dma32_size > 0 && dma32_size >= PAGE_SIZE) {
        zones[ZONE_DMA32].zone_type = ZONE_DMA32;
        zones[ZONE_DMA32].zone_name = zone_names[ZONE_DMA32];
        buddy_init(&zones[ZONE_DMA32], dma32_start, dma32_size);
        buddy_system.zones[ZONE_DMA32] = &zones[ZONE_DMA32];
        buddy_system.nr_zones++;
    }

    uint64_t normal_start = dma32_end;
    uint64_t normal_size = total_size - normal_start;
    if (normal_size > 0 && normal_size >= PAGE_SIZE) {
        zones[ZONE_NORMAL].zone_type = ZONE_NORMAL;
        zones[ZONE_NORMAL].zone_name = zone_names[ZONE_NORMAL];
        buddy_init(&zones[ZONE_NORMAL], normal_start, normal_size);
        buddy_system.zones[ZONE_NORMAL] = &zones[ZONE_NORMAL];
        buddy_system.nr_zones++;
    }

    buddy_system.total_system_pages = total_size / PAGE_SIZE;

    if (buddy_system.nr_zones == 0) {
        zones[ZONE_NORMAL].zone_type = ZONE_NORMAL;
        zones[ZONE_NORMAL].zone_name = zone_names[ZONE_NORMAL];
        buddy_init(&zones[ZONE_NORMAL], base_addr, total_size);
        buddy_system.zones[ZONE_NORMAL] = &zones[ZONE_NORMAL];
        buddy_system.nr_zones = 1;
    }

    buddy_initialized = 1;
}

page_t* buddy_alloc_pages(buddy_zone_t* zone, uint32_t order)
{
    if (!zone || order > BUDDY_MAX_ORDER) return NULL;

    spin_lock(&zone->lock);

    uint32_t current_order = order;
    while (current_order <= BUDDY_MAX_ORDER) {
        if (zone->free_list[current_order]) {
            break;
        }
        current_order++;
    }

    if (current_order > BUDDY_MAX_ORDER) {
        spin_unlock(&zone->lock);
        return NULL;
    }

    page_t* page = __list_pop(zone, current_order);
    while (page && (page->flags & PAGE_FLAG_RESERVED)) {
        page = __list_pop(zone, current_order);
    }
    if (!page) {
        spin_unlock(&zone->lock);
        return NULL;
    }
    page->flags |= PAGE_FLAG_USED;
    page->refcount = 1;

    while (current_order > order) {
        current_order--;
        uint64_t buddy_pfn_val = __buddy_pfn(page->pfn, current_order);
        page_t* buddy = pfn_to_page(zone, buddy_pfn_val);
        if (buddy) {
            buddy->order = current_order;
            buddy->flags = 0;
            buddy->refcount = 0;
            __list_add(zone, current_order, buddy);
        }
    }

    page->order = order;
    zone->used_pages += (1ULL << order);
    zone->free_pages -= (1ULL << order);
    spin_unlock(&zone->lock);
    return page;
}

void buddy_free_pages(buddy_zone_t* zone, page_t* page)
{
    if (!zone || !page || !(page->flags & PAGE_FLAG_USED)) return;

    spin_lock(&zone->lock);

    uint32_t order = page->order;
    page->flags &= ~PAGE_FLAG_USED;
    page->refcount = 0;
    zone->used_pages -= (1ULL << order);
    zone->free_pages += (1ULL << order);

    for (;;) {
        if (order >= BUDDY_MAX_ORDER) break;

        uint64_t buddy_pfn_val = __buddy_pfn(page->pfn, order);
        page_t* buddy = pfn_to_page(zone, buddy_pfn_val);

        if (!buddy) break;
        if (buddy->pfn < zone->base_pfn || buddy->pfn >= zone->end_pfn) break;
        if (buddy->flags & PAGE_FLAG_USED) break;
        if (buddy->order != order) break;
        if (!__in_freelist(zone, order, buddy)) break;

        __list_del(buddy);
        zone->free_count[order]--;

        if (buddy->pfn < page->pfn) {
            page = buddy;
        }
        order++;
    }

    page->order = order;
    __list_add(zone, order, page);

    spin_unlock(&zone->lock);
}

page_t* buddy_alloc_pages_n(buddy_zone_t* zone, uint32_t n)
{
    if (n == 0) return NULL;
    if (n > (1ULL << BUDDY_MAX_ORDER)) return NULL;

    uint32_t order = 0;
    while ((1U << order) < n && order < BUDDY_MAX_ORDER) {
        order++;
    }
    return buddy_alloc_pages(zone, order);
}

page_t* alloc_pages_zone(uint32_t order, int zone_hint)
{
    if (!buddy_initialized) return NULL;

    if (zone_hint >= 0 && zone_hint < ZONE_MAX && buddy_system.zones[zone_hint]) {
        page_t* p = buddy_alloc_pages(buddy_system.zones[zone_hint], order);
        if (p) return p;
    }

    for (int z = ZONE_NORMAL; z >= ZONE_DMA; z--) {
        if (z == zone_hint) continue;
        if (!buddy_system.zones[z]) continue;
        page_t* p = buddy_alloc_pages(buddy_system.zones[z], order);
        if (p) return p;
    }

    return NULL;
}

page_t* alloc_pages_dma(uint32_t order)
{
    return alloc_pages_zone(order, ZONE_DMA);
}

page_t* alloc_pages_dma32(uint32_t order)
{
    return alloc_pages_zone(order, ZONE_DMA32);
}

void* page_to_virt(const page_t* page)
{
    return (void*)(page->pfn * PAGE_SIZE);
}

page_t* virt_to_page(buddy_zone_t* zone, void* virt)
{
    uint64_t pfn = (uint64_t)virt / PAGE_SIZE;
    if (!zone) return NULL;
    if (pfn < zone->base_pfn || pfn >= zone->end_pfn) return NULL;
    return &zone->page_array[pfn - zone->base_pfn];
}

page_t* pfn_to_page(buddy_zone_t* zone, uint64_t pfn)
{
    if (!zone) return NULL;
    if (pfn < zone->base_pfn || pfn >= zone->end_pfn) return NULL;
    return &zone->page_array[pfn - zone->base_pfn];
}

uint64_t page_to_pfn(const page_t* page)
{
    return page->pfn;
}

void page_ref(page_t* page)
{
    if (page) {
        __sync_add_and_fetch(&page->refcount, 1);
    }
}

void page_unref(page_t* page)
{
    if (page) {
        __sync_sub_and_fetch(&page->refcount, 1);
    }
}

uint32_t page_get_refcount(page_t* page)
{
    return page ? page->refcount : 0;
}

void buddy_get_stats(buddy_zone_t* zone, uint64_t* total, uint64_t* free, uint64_t* used)
{
    if (!zone) {
        if (total) *total = 0;
        if (free) *free = 0;
        if (used) *used = 0;
        return;
    }
    if (total) *total = zone->total_pages;
    if (used)  *used  = zone->used_pages;
    if (free) {
        uint64_t f = 0;
        for (int i = 0; i <= BUDDY_MAX_ORDER; i++) {
            f += zone->free_count[i] * (1ULL << i);
        }
        *free = f;
    }
}

void buddy_get_global_stats(uint64_t* total, uint64_t* free, uint64_t* used)
{
    uint64_t t = 0, f = 0, u = 0;
    for (int z = 0; z < ZONE_MAX; z++) {
        if (!buddy_system.zones[z]) continue;
        uint64_t zt, zf, zu;
        buddy_get_stats(buddy_system.zones[z], &zt, &zf, &zu);
        t += zt;
        f += zf;
        u += zu;
    }
    if (total) *total = t;
    if (free) *free = f;
    if (used) *used = u;
}

uint64_t buddy_count_free_pages(buddy_zone_t* zone)
{
    if (!zone) return 0;
    uint64_t free = 0;
    spin_lock(&zone->lock);
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++) {
        free += zone->free_count[i] * (1ULL << i);
    }
    spin_unlock(&zone->lock);
    return free;
}

uint64_t buddy_count_free_pages_global(void)
{
    uint64_t total = 0;
    for (int z = 0; z < ZONE_MAX; z++) {
        if (buddy_system.zones[z]) {
            total += buddy_count_free_pages(buddy_system.zones[z]);
        }
    }
    return total;
}

int buddy_defragment(buddy_zone_t* zone)
{
    if (!zone) return -1;

    spin_lock(&zone->lock);

    int merged = 0;
    for (int order = BUDDY_MIN_ORDER; order < BUDDY_MAX_ORDER; order++) {
        page_t* p = zone->free_list[order];
        while (p) {
            page_t* next = p->next;
            uint64_t buddy_pfn_val = __buddy_pfn(p->pfn, order);
            page_t* buddy = pfn_to_page(zone, buddy_pfn_val);

            if (buddy && !(buddy->flags & PAGE_FLAG_USED) &&
                buddy->order == order && __in_freelist(zone, order, buddy)) {
                __list_del(p);
                __list_del(buddy);
                zone->free_count[order] -= 2;

                page_t* merged_page = (p->pfn < buddy->pfn) ? p : buddy;
                merged_page->order = order + 1;
                __list_add(zone, order + 1, merged_page);
                merged++;
            }
            p = next;
        }
    }

    spin_unlock(&zone->lock);
    return merged;
}

void buddy_dump_info(buddy_zone_t* zone)
{
    if (!zone) return;
}

page_t* alloc_pages(uint32_t order)
{
    if (!buddy_initialized) return NULL;
    return alloc_pages_zone(order, ZONE_NORMAL);
}

void free_pages(page_t* page)
{
    if (!page || !buddy_initialized) return;

    int zone_idx = page->zone_idx;
    if (zone_idx >= 0 && zone_idx < ZONE_MAX && buddy_system.zones[zone_idx]) {
        buddy_free_pages(buddy_system.zones[zone_idx], page);
        return;
    }

    for (int z = 0; z < ZONE_MAX; z++) {
        if (!buddy_system.zones[z]) continue;
        if (page->pfn >= buddy_system.zones[z]->base_pfn &&
            page->pfn < buddy_system.zones[z]->end_pfn) {
            buddy_free_pages(buddy_system.zones[z], page);
            return;
        }
    }
}

void* alloc_pages_virt(uint32_t order)
{
    page_t* p = alloc_pages(order);
    return p ? page_to_virt(p) : NULL;
}

void free_pages_virt(void* virt)
{
    if (!virt || !buddy_initialized) return;
    uint64_t pfn = (uint64_t)virt / PAGE_SIZE;

    for (int z = 0; z < ZONE_MAX; z++) {
        if (!buddy_system.zones[z]) continue;
        if (pfn >= buddy_system.zones[z]->base_pfn &&
            pfn < buddy_system.zones[z]->end_pfn) {
            page_t* p = virt_to_page(buddy_system.zones[z], virt);
            if (p) free_pages(p);
            return;
        }
    }
}

buddy_zone_t* buddy_get_main_zone(void)
{
    if (buddy_system.zones[ZONE_NORMAL]) return buddy_system.zones[ZONE_NORMAL];
    if (buddy_system.zones[ZONE_DMA32]) return buddy_system.zones[ZONE_DMA32];
    if (buddy_system.zones[ZONE_DMA]) return buddy_system.zones[ZONE_DMA];
    return &zones[ZONE_NORMAL];
}

buddy_zone_t* buddy_get_zone(int zone_type)
{
    if (zone_type < 0 || zone_type >= ZONE_MAX) return NULL;
    return buddy_system.zones[zone_type];
}

buddy_system_t* buddy_get_system(void)
{
    return &buddy_system;
}