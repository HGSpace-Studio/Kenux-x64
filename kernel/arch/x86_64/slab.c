

#include <arch/slab.h>
#include <arch/buddy.h>
#include <arch/memory.h>
#include <string.h>

static kmem_cache_t cache_table[SLAB_MAX_CACHES];
static uint32_t cache_count = 0;
static spinlock_t cache_table_lock = SPINLOCK_INIT;

static kmem_cache_t* kmalloc_caches[KMALLOC_CACHES];

static uint32_t __slab_order(uint32_t obj_size, uint32_t objs_per_slab)
{
    uint32_t need = obj_size * objs_per_slab;
    uint32_t order = 0;
    while ((PAGE_SIZE << order) < need && order < BUDDY_MAX_ORDER) {
        order++;
    }
    return order;
}

static void* __alloc_slab_pages(uint32_t order)
{
    page_t* page = alloc_pages(order);
    return page ? page_to_virt(page) : NULL;
}

static void __free_slab_pages(void* mem)
{
    if (!mem) return;
    page_t* page = virt_to_page(buddy_get_main_zone(), mem);
    if (page) free_pages(page);
}

static slab_t* __slab_create(kmem_cache_t* cache)
{
    slab_t* slab = (slab_t*)__alloc_slab_pages(cache->slab_order);
    if (!slab) return NULL;

    /* 关键修复: 标记所有占用页为 SLAB 页, 否则 kfree 会把 slab 对象
       当成 buddy 页框释放, 破坏 buddy 元数据导致内存统计失控. */
    buddy_zone_t* zone = buddy_get_main_zone();
    uint32_t npages = 1U << cache->slab_order;
    for (uint32_t i = 0; i < npages; i++) {
        page_t* p = virt_to_page(zone, (uint8_t*)slab + i * PAGE_SIZE);
        if (p) {
            p->flags |= PAGE_FLAG_SLAB;
            p->slab_cache = cache;
        }
    }

    uint8_t* mem = (uint8_t*)slab;
    uint32_t slab_size = PAGE_SIZE << cache->slab_order;

    slab_t* s = (slab_t*)(mem + slab_size - sizeof(slab_t));
    memset(s, 0, sizeof(slab_t));

    s->mem = mem;
    s->obj_size = cache->obj_size;
    s->obj_count = cache->objs_per_slab;
    s->free_count = cache->objs_per_slab;
    s->state = SLAB_EMPTY;

    void** current = NULL;
    for (uint32_t i = 0; i < cache->objs_per_slab; i++) {
        void* obj = mem + i * cache->obj_size;
        *(void**)obj = current;
        current = obj;
    }
    s->free_list = current;

    return s;
}

static void __slab_destroy(kmem_cache_t* cache, slab_t* slab)
{
    /* 清除 SLAB 页标志和反向映射 */
    buddy_zone_t* zone = buddy_get_main_zone();
    uint32_t npages = 1U << cache->slab_order;
    for (uint32_t i = 0; i < npages; i++) {
        page_t* p = virt_to_page(zone, (uint8_t*)slab->mem + i * PAGE_SIZE);
        if (p) {
            p->flags &= ~PAGE_FLAG_SLAB;
            p->slab_cache = NULL;
        }
    }
    __free_slab_pages(slab->mem);
}

static void __slab_add(slab_t** list, slab_t* slab)
{
    slab->next = *list;
    slab->prev = NULL;
    if (*list) {
        (*list)->prev = slab;
    }
    *list = slab;
}

static void __slab_remove(slab_t** list, slab_t* slab)
{
    if (slab->prev) slab->prev->next = slab->next;
    else *list = slab->next;
    if (slab->next) slab->next->prev = slab->prev;
    slab->next = slab->prev = NULL;
}

static void __slab_rehome(kmem_cache_t* cache, slab_t* slab)
{
    slab_t** from = NULL;
    slab_t** to = NULL;

    if (slab->state == SLAB_FULL) from = &cache->full_slabs;
    else if (slab->state == SLAB_PARTIAL) from = &cache->partial_slabs;
    else from = &cache->empty_slabs;

    if (slab->free_count == 0) {
        slab->state = SLAB_FULL;
        to = &cache->full_slabs;
    } else if (slab->free_count == slab->obj_count) {
        slab->state = SLAB_EMPTY;
        to = &cache->empty_slabs;
    } else {
        slab->state = SLAB_PARTIAL;
        to = &cache->partial_slabs;
    }

    if (from != to) {
        __slab_remove(from, slab);
        __slab_add(to, slab);
    }
}

void slab_init(void)
{
    memset(cache_table, 0, sizeof(cache_table));
    cache_count = 0;
    spin_init(&cache_table_lock);

    for (int i = 3; i <= KMALLOC_SHIFT_HIGH; i++) {
        uint32_t size = 1U << i;
        char name[32];
        name[0] = 'k'; name[1] = 'm'; name[2] = '_';

        uint32_t n = size;
        int pos = 3;
        char buf[16];
        int bpos = 0;
        do {
            buf[bpos++] = '0' + (n % 10);
            n /= 10;
        } while (n > 0);
        while (bpos-- > 0 && pos < 31) {
            name[pos++] = buf[bpos];
        }
        name[pos] = '\0';
        kmalloc_caches[i] = kmem_cache_create(name, size, size);
    }
}

kmem_cache_t* kmem_cache_create(const char* name, uint32_t size, uint32_t align)
{
    if (size == 0 || size > (PAGE_SIZE << BUDDY_MAX_ORDER)) return NULL;
    if (align == 0) align = 8;

    uint32_t obj_size = (size + align - 1) & ~(align - 1);
    if (obj_size < sizeof(void*)) obj_size = sizeof(void*);

    spin_lock(&cache_table_lock);
    if (cache_count >= SLAB_MAX_CACHES) {
        spin_unlock(&cache_table_lock);
        return NULL;
    }

    kmem_cache_t* cache = &cache_table[cache_count++];
    spin_unlock(&cache_table_lock);

    memset(cache, 0, sizeof(kmem_cache_t));
    if (name) {
        strncpy(cache->name, name, SLAB_NAME_MAX - 1);
        cache->name[SLAB_NAME_MAX - 1] = '\0';
    }
    cache->obj_size = obj_size;
    cache->align = align;
    cache->objs_per_slab = (PAGE_SIZE * 2) / obj_size;
    if (cache->objs_per_slab < 1) cache->objs_per_slab = 1;
    cache->slab_order = __slab_order(obj_size, cache->objs_per_slab);
    spin_init(&cache->lock);

    return cache;
}

void kmem_cache_destroy(kmem_cache_t* cache)
{
    if (!cache) return;
    spin_lock(&cache->lock);

    slab_t* lists[3] = { cache->full_slabs, cache->partial_slabs, cache->empty_slabs };
    for (int i = 0; i < 3; i++) {
        slab_t* slab = lists[i];
        while (slab) {
            slab_t* next = slab->next;
            __slab_destroy(cache, slab);
            slab = next;
        }
    }
    cache->full_slabs = cache->partial_slabs = cache->empty_slabs = NULL;
    spin_unlock(&cache->lock);
}

void* kmem_cache_alloc(kmem_cache_t* cache)
{
    if (!cache) return NULL;
    spin_lock(&cache->lock);

    slab_t* slab = cache->partial_slabs;
    if (!slab) {
        slab = cache->empty_slabs;
        if (slab) {
            __slab_remove(&cache->empty_slabs, slab);
            __slab_add(&cache->partial_slabs, slab);
        }
    }

    if (!slab) {

        slab = __slab_create(cache);
        if (!slab) {
            spin_unlock(&cache->lock);
            return NULL;
        }
        __slab_add(&cache->partial_slabs, slab);
    }

    void* obj = slab->free_list;
    if (obj) {
        slab->free_list = *(void**)obj;
        slab->free_count--;
        cache->total_objs++;
        cache->free_objs--;
        __slab_rehome(cache, slab);
    }

    spin_unlock(&cache->lock);
    return obj;
}

void kmem_cache_free(kmem_cache_t* cache, void* obj)
{
    if (!cache || !obj) return;
    spin_lock(&cache->lock);

    slab_t* slab = NULL;
    slab_t* lists[3] = { cache->full_slabs, cache->partial_slabs, cache->empty_slabs };
    for (int i = 0; i < 3 && !slab; i++) {
        slab_t* s = lists[i];
        while (s) {
            uint8_t* start = (uint8_t*)s->mem;
            uint8_t* end = start + (PAGE_SIZE << cache->slab_order);
            if ((uint8_t*)obj >= start && (uint8_t*)obj < end) {
                slab = s;
                break;
            }
            s = s->next;
        }
    }

    if (!slab) {
        spin_unlock(&cache->lock);
        return;
    }

    *(void**)obj = slab->free_list;
    slab->free_list = obj;
    slab->free_count++;
    cache->total_objs--;
    cache->free_objs++;

    uint32_t old_state = slab->state;
    __slab_rehome(cache, slab);

    if (old_state != SLAB_EMPTY && slab->state == SLAB_EMPTY) {

    }

    spin_unlock(&cache->lock);
}

void* kmalloc(uint64_t size)
{
    if (size == 0) return NULL;
    if (size > (1ULL << KMALLOC_SHIFT_HIGH)) {

        uint32_t order = 0;
        while ((PAGE_SIZE << order) < size && order < BUDDY_MAX_ORDER) order++;
        return alloc_pages_virt(order);
    }

    uint32_t idx = 3;
    while ((1ULL << idx) < size && idx < KMALLOC_SHIFT_HIGH) idx++;
    if (kmalloc_caches[idx]) {
        return kmem_cache_alloc(kmalloc_caches[idx]);
    }
    return NULL;
}

void* kzalloc(uint64_t size)
{
    void* p = kmalloc(size);
    if (p) memset(p, 0, size);
    return p;
}

void kfree(void* ptr)
{
    if (!ptr) return;

    page_t* page = virt_to_page(buddy_get_main_zone(), ptr);
    if (!page) return;

    if (page->flags & PAGE_FLAG_SLAB) {
        /* 用反向映射 O(1) 定位 cache, 替代原来 O(N) 遍历所有 cache 的实现 */
        kmem_cache_t* cache = page->slab_cache;
        if (cache) {
            kmem_cache_free(cache, ptr);
        }
    } else {
        free_pages_virt(ptr);
    }
}

uint64_t ksize(void* ptr)
{
    if (!ptr) return 0;

    page_t* page = virt_to_page(buddy_get_main_zone(), ptr);
    if (!page) return 0;

    if (page->flags & PAGE_FLAG_SLAB) {
        kmem_cache_t* cache = page->slab_cache;
        if (cache) {
            return cache->obj_size;
        }
        return 0;
    }

    uint32_t order = page->order;
    if (order > BUDDY_MAX_ORDER) order = BUDDY_MAX_ORDER;
    return (uint64_t)(PAGE_SIZE << order);
}
