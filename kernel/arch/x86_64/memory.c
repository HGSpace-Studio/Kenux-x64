#include <arch/memory.h>
#include <arch/buddy.h>
#include <arch/slab.h>
#include <string.h>

extern char kernel_end[];

static uint64_t heap_start = 0;
static int initialized = 0;
static vma_t kernel_vma;

void memory_init(void)
{
    if (initialized) return;

    uint64_t kend = (uint64_t)(uintptr_t)kernel_end;
    heap_start = (kend + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    heap_start += PAGE_SIZE * 16;

    uint64_t mem_size = 128 * 1024 * 1024;
    buddy_global_init(heap_start, mem_size);
    slab_init();

    vma_init(&kernel_vma);
    initialized = 1;
}

void* kmalloc(size_t size)
{
    if (!initialized || size == 0) return NULL;

    if (size <= PAGE_SIZE * 4) {
        void* ptr = slab_alloc(size);
        if (ptr) return ptr;
    }

    uint32_t order = 0;
    size_t aligned = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    while ((PAGE_SIZE << order) < aligned && order < BUDDY_MAX_ORDER) {
        order++;
    }

    page_t* page = alloc_pages(order);
    return page ? page_to_virt(page) : NULL;
}

void kfree(void* ptr)
{
    if (!ptr || !initialized) return;

    buddy_zone_t* zone = buddy_get_main_zone();
    page_t* page = virt_to_page(zone, ptr);

    if (page && (page->flags & PAGE_FLAG_SLAB)) {
        slab_free(ptr);
    } else if (page) {
        free_pages(page);
    }
}

void* kzalloc(size_t size)
{
    void* ptr = kmalloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void* krealloc(void* ptr, uint64_t new_size)
{
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    void* new_ptr = kmalloc(new_size);
    if (new_ptr && ptr) {
        memcpy(new_ptr, ptr, new_size);
        kfree(ptr);
    }
    return new_ptr;
}

uint64_t memory_alloc_physical(uint64_t count)
{
    page_t* page = alloc_pages(0);
    return page ? page->pfn * PAGE_SIZE : 0;
}

void memory_free_physical(uint64_t addr, uint64_t count)
{
    (void)count;
    buddy_zone_t* zone = buddy_get_main_zone();
    page_t* page = pfn_to_page(zone, addr / PAGE_SIZE);
    if (page) free_pages(page);
}

uint64_t memory_get_free(void)
{
    buddy_zone_t* zone = buddy_get_main_zone();
    uint64_t total, free_mem, used;
    buddy_get_stats(zone, &total, &free_mem, &used);
    return free_mem * PAGE_SIZE;
}

uint64_t memory_get_total(void)
{
    buddy_zone_t* zone = buddy_get_main_zone();
    uint64_t total, free_mem, used;
    buddy_get_stats(zone, &total, &free_mem, &used);
    return total * PAGE_SIZE;
}

void* memory_alloc(uint64_t size)
{
    return kmalloc(size);
}

void memory_free(void* ptr)
{
    kfree(ptr);
}

void* memory_zalloc(uint64_t size)
{
    return kzalloc(size);
}

void* memory_realloc(void* ptr, uint64_t old_size, uint64_t new_size)
{
    (void)old_size;
    return krealloc(ptr, new_size);
}

void* memory_alloc_aligned(uint64_t size, uint64_t alignment)
{
    if (alignment <= PAGE_SIZE) return kmalloc(size);

    uint64_t extra = alignment + size;
    uint8_t* raw = (uint8_t*)kmalloc(extra);
    if (!raw) return NULL;

    uint64_t addr = (uint64_t)(raw + alignment - 1) & ~((uint64_t)alignment - 1);
    return (void*)addr;
}

void vma_init(vma_t* vma)
{
    if (!vma) return;
    memset(vma, 0, sizeof(vma_t));
    vma->heap_start = KERNEL_VMA + 0x100000;
    vma->heap_end = vma->heap_start;
    vma->stack_top = KERNEL_VMA_END;
}

int vma_map(vma_t* vma, uint64_t start, uint64_t end, uint64_t flags, const char* name)
{
    if (!vma || vma->count >= VMA_MAX_REGIONS) return -1;
    if (start >= end) return -1;

    vma_region_t* r = &vma->regions[vma->count];
    r->start = start;
    r->end = end;
    r->flags = flags;
    r->type = 0;
    if (name) {
        strncpy(r->name, name, sizeof(r->name) - 1);
        r->name[sizeof(r->name) - 1] = '\0';
    } else {
        r->name[0] = '\0';
    }
    vma->count++;
    return 0;
}

int vma_unmap(vma_t* vma, uint64_t start, uint64_t end)
{
    if (!vma) return -1;

    for (int i = 0; i < vma->count; i++) {
        if (vma->regions[i].start == start && vma->regions[i].end == end) {
            if (i < vma->count - 1) {
                vma->regions[i] = vma->regions[vma->count - 1];
            }
            vma->count--;
            return 0;
        }
    }
    return -1;
}

vma_region_t* vma_find(vma_t* vma, uint64_t addr)
{
    if (!vma) return NULL;
    for (int i = 0; i < vma->count; i++) {
        if (addr >= vma->regions[i].start && addr < vma->regions[i].end) {
            return &vma->regions[i];
        }
    }
    return NULL;
}

uint64_t vma_alloc_range(vma_t* vma, uint64_t size, uint64_t align)
{
    if (!vma || vma->count >= VMA_MAX_REGIONS) return 0;

    uint64_t addr = vma->heap_end;
    if (align > PAGE_SIZE) {
        addr = (addr + align - 1) & ~(align - 1);
    }

    vma->heap_end = addr + size;
    return addr;
}

static void* current_pml4 = NULL;

static uint64_t* __pmap_get_or_alloc_table(uint64_t* entry)
{
    if (*entry & PAGE_PRESENT) {
        return (uint64_t*)PTE_ADDR(*entry);
    }

    void* table = memory_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (!table) return NULL;
    memset(table, 0, PAGE_SIZE);
    *entry = PTE_FLAGS((uint64_t)(uintptr_t)table, PAGE_PRESENT | PAGE_WRITABLE);
    return (uint64_t*)table;
}

void pmap_init(void)
{
    current_pml4 = pmap_get();
}

void* pmap_create(void)
{
    void* pml4 = memory_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (pml4) {
        memset(pml4, 0, PAGE_SIZE);
        if (current_pml4) {
            memcpy(pml4, current_pml4, PAGE_SIZE);
        }
    }
    return pml4;
}

void pmap_switch(void* pml4)
{
    if (!pml4) return;
    current_pml4 = pml4;

    __asm__ volatile (
        "mov %0, %%cr3\n"
        :
        : "r"(pml4)
        : "memory"
    );
}

void* pmap_get(void)
{
    void* cr3;
    __asm__ volatile (
        "mov %%cr3, %0\n"
        : "=r"(cr3)
    );
    return cr3;
}

int pmap_map_page(void* pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    if (!pml4) return -1;
    uint64_t* pml4e = (uint64_t*)pml4;
    uint64_t idx_pml4 = (vaddr >> 39) & 0x1FF;
    uint64_t idx_pdpt = (vaddr >> 30) & 0x1FF;
    uint64_t idx_pd   = (vaddr >> 21) & 0x1FF;
    uint64_t idx_pt   = (vaddr >> 12) & 0x1FF;

    uint64_t* pdpt = __pmap_get_or_alloc_table(&pml4e[idx_pml4]);
    if (!pdpt) return -1;
    uint64_t* pd = __pmap_get_or_alloc_table(&pdpt[idx_pdpt]);
    if (!pd) return -1;
    uint64_t* pt = __pmap_get_or_alloc_table(&pd[idx_pd]);
    if (!pt) return -1;

    pt[idx_pt] = PTE_FLAGS(paddr, flags | PAGE_PRESENT);
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
    return 0;
}

void pmap_unmap_page(void* pml4, uint64_t vaddr)
{
    if (!pml4) return;
    uint64_t* pml4e = (uint64_t*)pml4;
    uint64_t idx_pml4 = (vaddr >> 39) & 0x1FF;
    if (!(pml4e[idx_pml4] & PAGE_PRESENT)) return;
    uint64_t* pdpt = (uint64_t*)PTE_ADDR(pml4e[idx_pml4]);
    uint64_t idx_pdpt = (vaddr >> 30) & 0x1FF;
    if (!(pdpt[idx_pdpt] & PAGE_PRESENT)) return;
    uint64_t* pd = (uint64_t*)PTE_ADDR(pdpt[idx_pdpt]);
    uint64_t idx_pd = (vaddr >> 21) & 0x1FF;
    if (!(pd[idx_pd] & PAGE_PRESENT)) return;
    uint64_t* pt = (uint64_t*)PTE_ADDR(pd[idx_pd]);
    uint64_t idx_pt = (vaddr >> 12) & 0x1FF;
    pt[idx_pt] = 0;
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
}

uint64_t pmap_get_physical(void* pml4, uint64_t vaddr)
{
    if (!pml4) return 0;
    uint64_t* pml4e = (uint64_t*)pml4;
    uint64_t idx_pml4 = (vaddr >> 39) & 0x1FF;
    if (!(pml4e[idx_pml4] & PAGE_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)PTE_ADDR(pml4e[idx_pml4]);
    uint64_t idx_pdpt = (vaddr >> 30) & 0x1FF;
    if (!(pdpt[idx_pdpt] & PAGE_PRESENT)) return 0;
    uint64_t* pd = (uint64_t*)PTE_ADDR(pdpt[idx_pdpt]);
    uint64_t idx_pd = (vaddr >> 21) & 0x1FF;
    if (!(pd[idx_pd] & PAGE_PRESENT)) return 0;
    if (pd[idx_pd] & PAGE_SIZE_2M) {
        return PTE_ADDR(pd[idx_pd]) + (vaddr & 0x1FFFFF);
    }
    uint64_t* pt = (uint64_t*)PTE_ADDR(pd[idx_pd]);
    uint64_t idx_pt = (vaddr >> 12) & 0x1FF;
    if (!(pt[idx_pt] & PAGE_PRESENT)) return 0;
    return PTE_ADDR(pt[idx_pt]) + (vaddr & 0xFFF);
}

void memory_get_stats(uint64_t* total, uint64_t* free, uint64_t* ac, uint64_t* fc)
{
    if (total) *total = memory_get_total();
    if (free) *free = memory_get_free();
    if (ac) *ac = 0;
    if (fc) *fc = 0;
}