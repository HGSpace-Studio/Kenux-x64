#include <arch/memory.h>
#include <arch/buddy.h>
#include <arch/slab.h>
#include <string.h>
#include <arch/spinlock.h>

extern char kernel_end[];

static uint64_t heap_start = 0;
static int initialized = 0;
static vma_t kernel_vma;
static spinlock_t mem_lock;

typedef struct mem_block_header {
    size_t size;
    struct mem_block_header* next;
    uint32_t flags;
#define MEM_FLAG_USED     0x01
#define MEM_FLAG_KMALLOC  0x02
#define MEM_FLAG_SLAB     0x04
#define MEM_FLAG_LARGE    0x08
} mem_block_header_t;

typedef struct {
    size_t total_allocs;
    size_t total_frees;
    size_t current_usage;
    size_t peak_usage;
    size_t failed_allocs;
    size_t slab_hits;
    size_t slab_misses;
    size_t buddy_allocs;
    size_t large_allocs;
    size_t realloc_count;
    size_t zeroed_allocs;
    size_t aligned_allocs;
    double fragmentation_ratio;
    uint64_t last_compact_time;
    uint64_t alloc_timestamp[1024];
    void* alloc_ptrs[1024];
    size_t alloc_sizes[1024];
    int alloc_index;
} mem_stats_t;

static mem_stats_t mem_stats;

#define SLAB_SIZE_CLASSES 16
static const size_t slab_class_sizes[SLAB_SIZE_CLASSES] = {
    8, 16, 32, 48, 64, 96, 128, 192,
    256, 384, 512, 768, 1024, 1536, 2048, 3072
};

typedef struct slab_cache {
    size_t obj_size;
    size_t objects_per_slab;
    void* free_list;
    struct slab_cache* next;
    int active_slabs;
    int free_objects;
    int total_objects;
    spinlock_t lock;
    char name[32];
} slab_cache_t;

static slab_cache_t* slab_caches[SLAB_SIZE_CLASSES];

void memory_init(void)
{
    if (initialized) return;

    spin_init(&mem_lock);

    uint64_t kend = (uint64_t)(uintptr_t)kernel_end;
    heap_start = (kend + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    heap_start += PAGE_SIZE * 16;

    uint64_t mem_size = detect_memory_size();
    if (mem_size == 0) {
        mem_size = 128 * 1024 * 1024;
    }

    buddy_global_init(heap_start, mem_size);
    slab_system_init();

    vma_init(&kernel_vma);
    memset(&mem_stats, 0, sizeof(mem_stats));
    initialized = 1;
}

uint64_t detect_memory_size(void)
{
    uint64_t size = 0;

    if (efi_memory_map && efi_descriptor_size > 0) {
        uint64_t count = efi_memory_map_size / efi_descriptor_size;
        for (uint64_t i = 0; i < count; i++) {
            efi_memory_descriptor_t* desc =
                (efi_memory_descriptor_t*)((uint8_t*)efi_memory_map + i * efi_descriptor_size);
            if (desc->type == EFI_CONVENTIONAL_MEMORY) {
                size += desc->number_of_pages * PAGE_SIZE;
            }
        }
    }

    if (size == 0) {
        size = 128 * 1024 * 1024;
    }

    return size;
}

void* kmalloc(uint64_t size)
{
    if (!initialized || size == 0) return NULL;

    spin_lock(&mem_lock);

    if (size <= PAGE_SIZE * 4) {
        for (int i = 0; i < SLAB_SIZE_CLASSES; i++) {
            if (size <= slab_class_sizes[i]) {
                void* ptr = slab_alloc_from_cache(slab_caches[i]);
                if (ptr) {
                    mem_stats.slab_hits++;
                    mem_stats.total_allocs++;
                    mem_stats.current_usage += size;
                    if (mem_stats.current_usage > mem_stats.peak_usage) {
                        mem_stats.peak_usage = mem_stats.current_usage;
                    }
                    record_allocation(ptr, size);
                    spin_unlock(&mem_lock);
                    return ptr;
                }
                break;
            }
        }
        mem_stats.slab_misses++;
    }

    uint32_t order = 0;
    uint64_t aligned = (size + sizeof(mem_block_header_t) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    while ((PAGE_SIZE << order) < aligned && order < BUDDY_MAX_ORDER) {
        order++;
    }

    page_t* page = alloc_pages(order);
    if (page) {
        mem_block_header_t* header = (mem_block_header_t*)page_to_virt(page);
        header->size = size;
        header->flags = MEM_FLAG_USED | MEM_FLAG_LARGE | MEM_FLAG_KMALLOC;
        header->next = NULL;

        void* ptr = (void*)(header + 1);
        memset(ptr, 0, size);

        mem_stats.buddy_allocs++;
        mem_stats.large_allocs++;
        mem_stats.total_allocs++;
        mem_stats.current_usage += size;
        if (mem_stats.current_usage > mem_stats.peak_usage) {
            mem_stats.peak_usage = mem_stats.current_usage;
        }
        record_allocation(ptr, size);
        spin_unlock(&mem_lock);
        return ptr;
    }

    mem_stats.failed_allocs++;
    spin_unlock(&mem_lock);
    return NULL;
}

void kfree(void* ptr)
{
    if (!ptr || !initialized) return;

    spin_lock(&mem_lock);

    buddy_zone_t* zone = buddy_get_main_zone();
    page_t* page = virt_to_page(zone, ptr);

    if (page && (page->flags & PAGE_FLAG_SLAB)) {
        slab_free(ptr);
        mem_stats.total_frees++;
    } else if (page) {
        mem_block_header_t* header = ((mem_block_header_t*)ptr) - 1;
        if (header->flags & MEM_FLAG_KMALLOC) {
            mem_stats.current_usage -= header->size;
            record_free(ptr);
        }
        free_pages(page);
        mem_stats.total_frees++;
    }

    spin_unlock(&mem_lock);
}

void* kzalloc(uint64_t size)
{
    void* ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
        mem_stats.zeroed_allocs++;
    }
    return ptr;
}

void* krealloc(void* ptr, uint64_t new_size)
{
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    spin_lock(&mem_lock);

    mem_block_header_t* old_header = ((mem_block_header_t*)ptr) - 1;
    size_t old_size = old_header->size;

    void* new_ptr = kmalloc(new_size);
    if (new_ptr && ptr) {
        memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
        kfree(ptr);
        mem_stats.realloc_count++;
    }

    spin_unlock(&mem_lock);
    return new_ptr;
}

void* kmalloc_aligned(uint64_t size, uint64_t alignment)
{
    if (!initialized || size == 0 || alignment == 0) return NULL;

    spin_lock(&mem_lock);

    uint64_t extra = alignment + size + sizeof(mem_block_header_t);
    uint8_t* raw = (uint8_t*)kmalloc(extra);
    if (!raw) {
        spin_unlock(&mem_lock);
        return NULL;
    }

    uint64_t addr = (uint64_t)(raw + sizeof(mem_block_header_t) + alignment - 1) & ~((uint64_t)alignment - 1);
    mem_block_header_t* header = (mem_block_header_t*)(addr - sizeof(mem_block_header_t));
    header->size = size;
    header->flags = MEM_FLAG_USED | MEM_FLAG_KMALLOC;

    mem_stats.aligned_allocs++;
    spin_unlock(&mem_lock);
    return (void*)addr;
}

char* kstrdup(const char* str)
{
    if (!str) return NULL;

    size_t len = strlen(str) + 1;
    char* new_str = (char*)kmalloc(len);
    if (new_str) {
        memcpy(new_str, str, len);
    }
    return new_str;
}

char* kstrndup(const char* str, size_t n)
{
    if (!str) return NULL;

    size_t len = strnlen(str, n) + 1;
    char* new_str = (char*)kmalloc(len);
    if (new_str) {
        memcpy(new_str, str, len - 1);
        new_str[len - 1] = '\0';
    }
    return new_str;
}

int memory_get_stats(mem_stats_t* stats)
{
    if (!stats || !initialized) return -1;

    spin_lock(&mem_lock);
    memcpy(stats, &mem_stats, sizeof(mem_stats_t));

    buddy_zone_t* zone = buddy_get_main_zone();
    uint64_t total, free_mem, used;
    buddy_get_stats(zone, &total, &free_mem, &used);

    if (total > 0) {
        stats->fragmentation_ratio = (double)(used - stats->current_usage) / (double)used;
    } else {
        stats->fragmentation_ratio = 0.0;
    }
    spin_unlock(&mem_lock);

    return 0;
}

void memory_compact(void)
{
    if (!initialized) return;

    spin_lock(&mem_lock);

    slab_compact_all();

    buddy_zone_t* zone = buddy_get_main_zone();
    buddy_compact(zone);

    mem_stats.last_compact_time = get_current_time_ns();

    spin_unlock(&mem_lock);
}

void memory_defrag(void)
{
    if (!initialized) return;

    spin_lock(&mem_lock);

    for (int i = 0; i < SLAB_SIZE_CLASSES; i++) {
        if (slab_caches[i]) {
            slab_defrag(slab_caches[i]);
        }
    }

    buddy_zone_t* zone = buddy_get_main_zone();
    buddy_merge_free_pages(zone);

    spin_unlock(&mem_lock);
}

int memory_pressure_level(void)
{
    if (!initialized) return 0;

    buddy_zone_t* zone = buddy_get_main_zone();
    uint64_t total, free_mem, used;
    buddy_get_stats(zone, &total, &free_mem, &used);

    if (total == 0) return 0;

    double usage_percent = (double)(total - free_mem) / (double)total * 100.0;

    if (usage_percent > 90.0) return 3;
    if (usage_percent > 75.0) return 2;
    if (usage_percent > 50.0) return 1;
    return 0;
}

bool memory_available(size_t size)
{
    if (!initialized || size == 0) return false;

    buddy_zone_t* zone = buddy_get_main_zone();
    uint64_t total, free_mem, used;
    buddy_get_stats(zone, &total, &free_mem, &used);

    return free_mem >= size;
}

static void record_allocation(void* ptr, size_t size)
{
    int idx = mem_stats.alloc_index++ % 1024;
    mem_stats.alloc_timestamp[idx] = get_current_time_ns();
    mem_stats.alloc_ptrs[idx] = ptr;
    mem_stats.alloc_sizes[idx] = size;
}

static void record_free(void* ptr)
{
    for (int i = 0; i < 1024; i++) {
        if (mem_stats.alloc_ptrs[i] == ptr) {
            mem_stats.alloc_ptrs[i] = NULL;
            mem_stats.alloc_sizes[i] = 0;
            break;
        }
    }
}

void slab_system_init(void)
{
    const char* cache_names[SLAB_SIZE_CLASSES] = {
        "slab-8", "slab-16", "slab-32", "slab-48",
        "slab-64", "slab-96", "slab-128", "slab-192",
        "slab-256", "slab-384", "slab-512", "slab-768",
        "slab-1k", "slab-1.5k", "slab-2k", "slab-3k"
    };

    for (int i = 0; i < SLAB_SIZE_CLASSES; i++) {
        slab_caches[i] = slab_create(cache_names[i], slab_class_sizes[i], 0);
        if (slab_caches[i]) {
            spin_init(&slab_caches[i]->lock);
        }
    }
}

void* slab_alloc_from_cache(slab_cache_t* cache)
{
    if (!cache) return NULL;

    spin_lock(&cache->lock);

    if (cache->free_list) {
        void* obj = cache->free_list;
        cache->free_list = *(void**)obj;
        cache->free_objects--;
        spin_unlock(&cache->lock);
        return obj;
    }

    spin_unlock(&cache->lock);
    return NULL;
}

void slab_free_to_cache(slab_cache_t* cache, void* obj)
{
    if (!cache || !obj) return;

    spin_lock(&cache->lock);

    *(void**)obj = cache->free_list;
    cache->free_list = obj;
    cache->free_objects++;

    spin_unlock(&cache->lock);
}

void slab_compact_all(void)
{
    for (int i = 0; i < SLAB_SIZE_CLASSES; i++) {
        if (slab_caches[i]) {
            slab_compact(slab_caches[i]);
        }
    }
}

void slab_destroy_cache(slab_cache_t* cache)
{
    if (!cache) return;

    spin_lock(&cache->lock);

    while (cache->free_list) {
        void* obj = cache->free_list;
        cache->free_list = *(void**)obj;
        page_t* page = virt_to_page(buddy_get_main_zone(), obj);
        if (page) {
            free_pages(page);
        }
    }

    spin_unlock(&cache->lock);
    kfree(cache);
}

uint64_t memory_get_total(void)
{
    if (!initialized) return 0;

    buddy_zone_t* zone = buddy_get_main_zone();
    uint64_t total, free_mem, used;
    buddy_get_stats(zone, &total, &free_mem, &used);
    return total;
}

uint64_t memory_get_free(void)
{
    if (!initialized) return 0;

    buddy_zone_t* zone = buddy_get_main_zone();
    uint64_t total, free_mem, used;
    buddy_get_stats(zone, &total, &free_mem, &used);
    return free_mem;
}

uint64_t memory_get_used(void)
{
    if (!initialized) return 0;

    buddy_zone_t* zone = buddy_get_main_zone();
    uint64_t total, free_mem, used;
    buddy_get_stats(zone, &total, &free_mem, &used);
    return used;
}

double memory_get_usage_percent(void)
{
    if (!initialized) return 0.0;

    uint64_t total = memory_get_total();
    if (total == 0) return 0.0;

    uint64_t used = memory_get_used();
    return (double)used / (double)total * 100.0;
}

void* memory_alloc_physical_aligned(uint64_t count, uint64_t alignment)
{
    if (!initialized || count == 0) return NULL;

    uint64_t size = count * PAGE_SIZE;
    if (alignment > PAGE_SIZE) {
        size += alignment;
    }

    page_t* page = alloc_pages(0);
    if (!page) return NULL;

    uint64_t addr = page->pfn * PAGE_SIZE;
    if (alignment > PAGE_SIZE) {
        uint64_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
        if (aligned_addr != addr) {
            free_pages(page);
            page = alloc_pages(0);
            if (!page) return NULL;
        }
    }

    return (void*)(page->pfn * PAGE_SIZE);
}

int memory_set_readonly(void* addr, size_t size)
{
    if (!initialized || !addr || size == 0) return -1;

    uint64_t start = (uint64_t)addr;
    uint64_t end = start + size;

    start &= ~(PAGE_SIZE - 1);
    end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint64_t p = start; p < end; p += PAGE_SIZE) {
        set_page_flags(p, PAGE_PRESENT | PAGE_READONLY);
    }

    return 0;
}

int memory_set_writable(void* addr, size_t size)
{
    if (!initialized || !addr || size == 0) return -1;

    uint64_t start = (uint64_t)addr;
    uint64_t end = start + size;

    start &= ~(PAGE_SIZE - 1);
    end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint64_t p = start; p < end; p += PAGE_SIZE) {
        set_page_flags(p, PAGE_PRESENT | PAGE_WRITABLE);
    }

    return 0;
}

int memory_set_nocache(void* addr, size_t size)
{
    if (!initialized || !addr || size == 0) return -1;

    uint64_t start = (uint64_t)addr;
    uint64_t end = start + size;

    start &= ~(PAGE_SIZE - 1);
    end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint64_t p = start; p < end; p += PAGE_SIZE) {
        set_page_flags(p, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);
    }

    return 0;
}

void memory_dump_info(void)
{
    if (!initialized) return;

    mem_stats_t stats;
    memory_get_stats(&stats);

    printk("Memory Statistics:\n");
    printk("  Total allocations: %zu\n", stats.total_allocs);
    printk("  Total frees:       %zu\n", stats.total_frees);
    printk("  Current usage:     %zu bytes (%.2f MB)\n",
           stats.current_usage, (double)stats.current_usage / (1024.0 * 1024.0));
    printk("  Peak usage:        %zu bytes (%.2f MB)\n",
           stats.peak_usage, (double)stats.peak_usage / (1024.0 * 1024.0));
    printk("  Failed allocs:     %zu\n", stats.failed_allocs);
    printk("  Slab hits/misses:  %zu/%zu\n", stats.slab_hits, stats.slab_misses);
    printk("  Buddy allocs:      %zu\n", stats.buddy_allocs);
    printk("  Large allocs:      %zu\n", stats.large_allocs);
    printk("  Realloc count:     %zu\n", stats.realloc_count);
    printk("  Zeroed allocs:     %zu\n", stats.zeroed_allocs);
    printk("  Aligned allocs:    %zu\n", stats.aligned_allocs);
    printk("  Fragmentation:     %.2f%%\n", stats.fragmentation_ratio * 100.0);

    uint64_t total = memory_get_total();
    uint64_t free_mem = memory_get_free();
    uint64_t used = memory_get_used();

    printk("\nBuddy System:\n");
    printk("  Total: %zu MB\n", total / (1024 * 1024));
    printk("  Free:  %zu MB (%.1f%%)\n", free_mem / (1024 * 1024),
           (double)free_mem / (double)total * 100.0);
    printk("  Used:  %zu MB (%.1f%%)\n", used / (1024 * 1024),
           (double)used / (double)total * 100.0);

    int pressure = memory_pressure_level();
    printk("\nMemory pressure level: %d (0=low, 1=medium, 2=high, 3=critical)\n", pressure);
}