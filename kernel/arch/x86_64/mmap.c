

#include <arch/mmap.h>
#include <arch/memory.h>
#include <arch/pagefault.h>
#include <arch/pagecache.h>
#include <arch/buddy.h>
#include <arch/spinlock.h>
#include <string.h>

extern void printk(const char* fmt, ...);
extern void panic(const char* msg);

/* 修复: 原 extern 声明为 alloc_pages_virt, 但我们需要物理地址.
   改用 alloc_pages + page_to_pfn 模型. */
extern page_t* alloc_pages(uint32_t order);
extern void  free_pages(page_t* page);
extern uint64_t page_to_pfn(const page_t* page);

extern void*  pmap_get(void);
extern int    pmap_map_page(void* pml4, uint64_t vaddr, uint64_t paddr,
                              uint64_t flags);
extern void   pmap_unmap_page(void* pml4, uint64_t vaddr);
extern uint64_t pmap_get_physical(void* pml4, uint64_t vaddr);

extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* ptr);

/* 辅助: 分配一个物理页, 返回 page_t* (用于后续 free). */
static page_t* mmap_alloc_phys_page(void)
{
    return alloc_pages(0);
}

static void mmap_free_phys_page(uint64_t paddr)
{
    /* 通过物理地址反查 page 并释放 */
    buddy_zone_t* zone = buddy_get_main_zone();
    uint64_t pfn = paddr / PAGE_SIZE;
    page_t* page = pfn_to_page(zone, pfn);
    if (page) free_pages(page);
}

static inline uint64_t __read_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

#define PAGE_ALIGN_DOWN(addr)  ((addr) & ~((uint64_t)PAGE_SIZE - 1))

#define PAGE_ALIGN_UP(addr)   (((addr) + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1))

static inline uint64_t prot_to_pte_flags(uint32_t prot)
{
    uint64_t flags = PAGE_PRESENT | PAGE_USER;

    if (prot & PROT_WRITE) {
        flags |= PAGE_WRITABLE;
    }
    if (!(prot & PROT_EXEC)) {
        flags |= PAGE_NO_EXEC;
    }

    return flags;
}

mmap_area_t* mmap_alloc(void)
{
    mmap_area_t* area = (mmap_area_t*)memory_alloc(sizeof(mmap_area_t));
    if (area) {
        memset(area, 0, sizeof(mmap_area_t));
    }
    return area;
}

void mmap_free(mmap_area_t* area)
{
    if (area) {
        memory_free(area);
    }
}

void mmap_context_init(mmap_context_t* ctx)
{
    if (!ctx) return;
    memset(ctx->areas, 0, sizeof(ctx->areas));
    ctx->area_count = 0;
    spin_init(&ctx->lock);
}

void mmap_context_destroy(mmap_context_t* ctx)
{
    if (!ctx) return;

    spin_lock(&ctx->lock);

    for (int i = 0; i < MMAP_MAX_AREAS; i++) {
        mmap_area_t* area = &ctx->areas[i];
        if (!area->active) {
            continue;
        }

        uint64_t npages = area->length / PAGE_SIZE;
        void* pml4 = (void*)(uint64_t)__read_cr3();

        if ((area->flags & MAP_SHARED) && (area->flags & MMAP_FLAG_DIRTY)
            && area->file) {
            for (uint64_t j = 0; j < npages; j++) {
                uint64_t vaddr = area->start + j * PAGE_SIZE;
                pagecache_entry_t* pce = pagecache_find_page(area->file,
                                                              area->offset + j * PAGE_SIZE);
                if (pce) {
                    pagecache_set_dirty(pce);
                    pagecache_put_page(pce);
                }
            }

            pagecache_sync_node(area->file);
        }

        for (uint64_t j = 0; j < npages; j++) {
            uint64_t vaddr = area->start + j * PAGE_SIZE;
            pmap_unmap_page(pml4, vaddr);
        }

        if ((area->flags & MAP_ANONYMOUS) || (area->flags & MAP_PRIVATE)) {
            for (uint64_t j = 0; j < npages; j++) {
                uint64_t vaddr = area->start + j * PAGE_SIZE;
                uint64_t paddr = pmap_get_physical(pml4, vaddr);
                if (paddr) {
                    mmap_free_phys_page(paddr);
                }
            }
        }

        memset(area, 0, sizeof(mmap_area_t));
    }

    ctx->area_count = 0;
    spin_unlock(&ctx->lock);
}

uint64_t mmap_find_free_area(mmap_context_t* ctx, uint64_t length)
{
    if (!ctx || length == 0) {
        return 0;
    }

    uint64_t aligned_len = PAGE_ALIGN_UP(length);

    spin_lock(&ctx->lock);

    uint64_t candidate = MMAP_SEARCH_START;

    while (candidate >= MMAP_MIN_ADDR + aligned_len) {
        uint64_t try_start = candidate - aligned_len;
        int conflict = 0;

        for (int i = 0; i < MMAP_MAX_AREAS; i++) {
            if (!ctx->areas[i].active) {
                continue;
            }
            mmap_area_t* area = &ctx->areas[i];

            uint64_t area_start = area->start;
            uint64_t area_end   = area->start + area->length;

            if (try_start < area_end && (try_start + aligned_len) > area_start) {

                candidate = area_start;
                conflict = 1;
                break;
            }
        }

        if (!conflict) {
            spin_unlock(&ctx->lock);
            return try_start;
        }
    }

    spin_unlock(&ctx->lock);
    return 0;
}

static int mmap_check_overlap(mmap_context_t* ctx, uint64_t addr, uint64_t length)
{
    for (int i = 0; i < MMAP_MAX_AREAS; i++) {
        if (!ctx->areas[i].active) {
            continue;
        }
        mmap_area_t* area = &ctx->areas[i];
        uint64_t area_end = area->start + area->length;

        if (addr < area_end && (addr + length) > area->start) {
            return 1;
        }
    }
    return 0;
}

static int mmap_find_covering_area(mmap_context_t* ctx, uint64_t addr,
                                     uint64_t length)
{
    for (int i = 0; i < MMAP_MAX_AREAS; i++) {
        if (!ctx->areas[i].active) {
            continue;
        }
        mmap_area_t* area = &ctx->areas[i];
        uint64_t area_end = area->start + area->length;

        if (addr < area_end && (addr + length) > area->start) {
            return i;
        }
    }
    return -1;
}

static int mmap_find_area_at(mmap_context_t* ctx, uint64_t vaddr)
{
    for (int i = 0; i < MMAP_MAX_AREAS; i++) {
        if (!ctx->areas[i].active) {
            continue;
        }
        mmap_area_t* area = &ctx->areas[i];
        if (vaddr >= area->start && vaddr < (area->start + area->length)) {
            return i;
        }
    }
    return -1;
}

static int mmap_alloc_slot(mmap_context_t* ctx)
{
    for (int i = 0; i < MMAP_MAX_AREAS; i++) {
        if (!ctx->areas[i].active) {
            return i;
        }
    }
    return -1;
}

uint64_t mmap_do_mmap(mmap_context_t* ctx, uint64_t addr, uint64_t length,
                       uint32_t prot, uint32_t flags, int fd, uint64_t offset)
{

    if (!ctx || length == 0) {
        return (uint64_t)-1;
    }

    if (length > 0x10000000000ULL) {
        return (uint64_t)-1;
    }

    if ((prot & PROT_READ) == 0 && (prot & PROT_WRITE) == 0 &&
        (prot & PROT_EXEC) == 0 && prot != PROT_NONE) {
        return (uint64_t)-1;
    }

    if ((flags & MAP_SHARED) && (flags & MAP_PRIVATE)) {
        return (uint64_t)-1;
    }

    if (!(flags & MAP_ANONYMOUS) && (offset & (PAGE_SIZE - 1)) != 0) {
        return (uint64_t)-1;
    }

    uint64_t aligned_len = PAGE_ALIGN_UP(length);

    if (addr != 0 && (addr & (PAGE_SIZE - 1)) != 0) {
        return (uint64_t)-1;
    }

    uint64_t map_addr;

    if (addr != 0) {

        if (flags & MAP_FIXED) {

            mmap_do_munmap(ctx, addr, aligned_len);
            map_addr = addr;
        } else {

            if (mmap_check_overlap(ctx, addr, aligned_len)) {

                map_addr = mmap_find_free_area(ctx, aligned_len);
                if (map_addr == 0) {
                    return (uint64_t)-1;
                }
            } else {
                map_addr = addr;
            }
        }
    } else {

        map_addr = mmap_find_free_area(ctx, aligned_len);
        if (map_addr == 0) {
            return (uint64_t)-1;
        }
    }

    spin_lock(&ctx->lock);

    int slot = mmap_alloc_slot(ctx);
    if (slot < 0) {
        spin_unlock(&ctx->lock);
        return (uint64_t)-1;
    }

    mmap_area_t* area = &ctx->areas[slot];
    area->start      = map_addr;
    area->length     = aligned_len;
    area->prot       = prot;
    area->flags      = flags;
    area->offset     = offset;
    area->ref_count  = 1;
    area->active     = 1;

    if (flags & MAP_ANONYMOUS) {
        area->file = NULL;
    } else {

        area->file = NULL;
        (void)fd;
    }

    ctx->area_count++;
    spin_unlock(&ctx->lock);

    void* pml4 = (void*)(uint64_t)__read_cr3();
    uint64_t npages = aligned_len / PAGE_SIZE;
    uint64_t pte_flags = prot_to_pte_flags(prot);

    if (flags & MAP_ANONYMOUS) {

        for (uint64_t i = 0; i < npages; i++) {
            uint64_t vaddr = map_addr + i * PAGE_SIZE;
            page_t* pg = mmap_alloc_phys_page();
            if (!pg) {

                for (uint64_t j = 0; j < i; j++) {
                    uint64_t va = map_addr + j * PAGE_SIZE;
                    uint64_t pa = pmap_get_physical(pml4, va);
                    pmap_unmap_page(pml4, va);
                    if (pa) {
                        mmap_free_phys_page(pa);
                    }
                }

                spin_lock(&ctx->lock);
                area->active = 0;
                ctx->area_count--;
                spin_unlock(&ctx->lock);
                return (uint64_t)-1;
            }
            uint64_t paddr = page_to_pfn(pg) * PAGE_SIZE;
            memset((void*)paddr, 0, PAGE_SIZE);

            if (pmap_map_page(pml4, vaddr, paddr, pte_flags) != 0) {
                free_pages(pg);

                for (uint64_t j = 0; j < i; j++) {
                    uint64_t va = map_addr + j * PAGE_SIZE;
                    uint64_t pa = pmap_get_physical(pml4, va);
                    pmap_unmap_page(pml4, va);
                    if (pa) {
                        mmap_free_phys_page(pa);
                    }
                }
                spin_lock(&ctx->lock);
                area->active = 0;
                ctx->area_count--;
                spin_unlock(&ctx->lock);
                return (uint64_t)-1;
            }
        }
        area->flags |= MMAP_FLAG_POPULATED;
    }
    else if (flags & MAP_PRIVATE) {

        uint64_t cow_pte_flags = pte_flags & ~(uint64_t)PAGE_WRITABLE;
        cow_pte_flags |= PAGE_COW_FLAG;

        for (uint64_t i = 0; i < npages; i++) {

        }

        area->flags |= MMAP_FLAG_COW;
    }
    else if (flags & MAP_SHARED) {

        if (area->file) {

            for (uint64_t i = 0; i < npages; i++) {
                uint64_t file_offset = offset + i * PAGE_SIZE;
                pagecache_entry_t* pce = pagecache_find_page(area->file,
                                                              file_offset);
                if (pce) {

                    uint64_t vaddr = map_addr + i * PAGE_SIZE;
                    uint64_t paddr = (uint64_t)pce->page;
                    if (pmap_map_page(pml4, vaddr, paddr, pte_flags) == 0) {

                    }
                    pagecache_put_page(pce);
                }

            }
        } else {

            for (uint64_t i = 0; i < npages; i++) {
                uint64_t vaddr = map_addr + i * PAGE_SIZE;
                page_t* pg = mmap_alloc_phys_page();
                if (!pg) {

                    for (uint64_t j = 0; j < i; j++) {
                        uint64_t va = map_addr + j * PAGE_SIZE;
                        uint64_t pa = pmap_get_physical(pml4, va);
                        pmap_unmap_page(pml4, va);
                        if (pa) mmap_free_phys_page(pa);
                    }
                    spin_lock(&ctx->lock);
                    area->active = 0;
                    ctx->area_count--;
                    spin_unlock(&ctx->lock);
                    return (uint64_t)-1;
                }
                uint64_t paddr = page_to_pfn(pg) * PAGE_SIZE;
                memset((void*)paddr, 0, PAGE_SIZE);
                pmap_map_page(pml4, vaddr, paddr, pte_flags);
            }
            area->flags |= MMAP_FLAG_POPULATED;
        }
    }

    return map_addr;
}

int mmap_do_munmap(mmap_context_t* ctx, uint64_t addr, uint64_t length)
{
    if (!ctx || length == 0) {
        return -1;
    }

    uint64_t aligned_len = PAGE_ALIGN_UP(length);
    void* pml4 = (void*)(uint64_t)__read_cr3();

    spin_lock(&ctx->lock);

    int found_any = 0;

    for (;;) {
        int idx = mmap_find_covering_area(ctx, addr, aligned_len);
        if (idx < 0) {
            break;
        }

        mmap_area_t* area = &ctx->areas[idx];
        found_any = 1;

        uint64_t unmap_start = (addr > area->start) ? addr : area->start;
        uint64_t unmap_end   = ((addr + aligned_len) < (area->start + area->length))
                               ? (addr + aligned_len) : (area->start + area->length);

        if ((area->flags & MAP_SHARED) && (area->flags & MMAP_FLAG_DIRTY)
            && area->file) {
            uint64_t page_start = PAGE_ALIGN_DOWN(unmap_start);
            uint64_t page_end   = PAGE_ALIGN_UP(unmap_end);

            for (uint64_t v = page_start; v < page_end; v += PAGE_SIZE) {
                uint64_t file_off = area->offset + (v - area->start);
                pagecache_entry_t* pce = pagecache_find_page(area->file, file_off);
                if (pce) {
                    pagecache_set_dirty(pce);
                    pagecache_put_page(pce);
                }
            }
            pagecache_sync_node(area->file);
        }

        uint64_t page_start = PAGE_ALIGN_DOWN(unmap_start);
        uint64_t page_end   = PAGE_ALIGN_UP(unmap_end);

        for (uint64_t v = page_start; v < page_end; v += PAGE_SIZE) {
            pmap_unmap_page(pml4, v);
        }

        if (unmap_start == area->start && unmap_end == (area->start + area->length)) {

            memset(area, 0, sizeof(mmap_area_t));
            ctx->area_count--;
        } else if (unmap_start == area->start) {

            uint64_t removed = unmap_end - area->start;
            area->start  += removed;
            area->length -= removed;
            if (area->offset != 0) {
                area->offset += removed;
            }
        } else if (unmap_end == (area->start + area->length)) {

            uint64_t removed = unmap_end - unmap_start;
            area->length -= removed;
        } else {

            uint64_t tail_start = unmap_end;
            uint64_t tail_length = (area->start + area->length) - unmap_end;

            int new_slot = mmap_alloc_slot(ctx);
            if (new_slot >= 0) {
                mmap_area_t* tail = &ctx->areas[new_slot];
                tail->start     = tail_start;
                tail->length    = tail_length;
                tail->prot      = area->prot;
                tail->flags     = area->flags;
                tail->offset    = area->offset + (tail_start - area->start);
                tail->file      = area->file;
                tail->ref_count = 1;
                tail->active    = 1;
                ctx->area_count++;
            }

            area->length = unmap_start - area->start;
        }
    }

    spin_unlock(&ctx->lock);

    return found_any ? 0 : -1;
}

int mmap_do_mprotect(mmap_context_t* ctx, uint64_t addr, uint64_t len,
                      uint32_t prot)
{
    if (!ctx || len == 0) {
        return -1;
    }

    uint64_t aligned_len = PAGE_ALIGN_UP(len);
    void* pml4 = (void*)(uint64_t)__read_cr3();
    int modified = 0;

    spin_lock(&ctx->lock);

    for (int i = 0; i < MMAP_MAX_AREAS; i++) {
        if (!ctx->areas[i].active) {
            continue;
        }
        mmap_area_t* area = &ctx->areas[i];
        uint64_t area_end = area->start + area->length;

        if (addr >= area_end || (addr + aligned_len) <= area->start) {
            continue;
        }

        area->prot = prot;

        uint64_t overlap_start = (addr > area->start) ? addr : area->start;
        uint64_t overlap_end   = ((addr + aligned_len) < area_end)
                                ? (addr + aligned_len) : area_end;
        uint64_t new_pte_flags = prot_to_pte_flags(prot);

        for (uint64_t vaddr = PAGE_ALIGN_DOWN(overlap_start);
             vaddr < PAGE_ALIGN_UP(overlap_end);
             vaddr += PAGE_SIZE) {
            uint64_t paddr = pmap_get_physical(pml4, vaddr);
            if (paddr) {

                pmap_unmap_page(pml4, vaddr);
                pmap_map_page(pml4, vaddr, paddr, new_pte_flags);
            }
        }

        modified = 1;
    }

    spin_unlock(&ctx->lock);

    if (!modified) {
        return -1;
    }

    __asm__ volatile (
        "mov %%cr3, %%rax\n"
        "mov %%rax, %%cr3\n"
        ::: "rax", "memory"
    );

    return 0;
}

int mmap_page_fault_handler(mmap_context_t* ctx, uint64_t vaddr,
                             uint64_t error_code)
{
    if (!ctx) {
        return -1;
    }

    spin_lock(&ctx->lock);

    int idx = mmap_find_area_at(ctx, vaddr);
    if (idx < 0) {
        spin_unlock(&ctx->lock);
        return -1;
    }

    mmap_area_t* area = &ctx->areas[idx];
    uint64_t page_offset = vaddr - area->start;
    uint64_t page_vaddr = vaddr & ~(uint64_t)(PAGE_SIZE - 1);

    void* pml4 = (void*)(uint64_t)__read_cr3();
    uint64_t pte_flags = prot_to_pte_flags(area->prot);

    if (area->flags & MAP_PRIVATE) {

        if (error_code & PF_WRITE) {

            page_t* new_pg = mmap_alloc_phys_page();
            if (!new_pg) {
                spin_unlock(&ctx->lock);
                return -1;
            }
            uint64_t new_paddr = page_to_pfn(new_pg) * PAGE_SIZE;
            memset((void*)new_paddr, 0, PAGE_SIZE);

            if (area->file) {
                uint64_t file_off = area->offset + page_offset;
                pagecache_entry_t* pce = pagecache_get_page(area->file, file_off);
                if (pce) {
                    memcpy((void*)new_paddr, pce->page, PAGE_SIZE);
                    pagecache_put_page(pce);
                }
            } else {

                uint64_t old_paddr = pmap_get_physical(pml4, page_vaddr);
                if (old_paddr) {
                    memcpy((void*)new_paddr, (void*)old_paddr, PAGE_SIZE);
                }
            }

            if (pmap_map_page(pml4, page_vaddr, new_paddr,
                              pte_flags | PAGE_WRITABLE) != 0) {
                free_pages(new_pg);
                spin_unlock(&ctx->lock);
                return -1;
            }

            area->flags |= MMAP_FLAG_DIRTY | MMAP_FLAG_POPULATED;

            __asm__ volatile ("invlpg (%0)" :: "r"(page_vaddr) : "memory");
            spin_unlock(&ctx->lock);
            return 0;
        }
        else {

            page_t* new_pg = mmap_alloc_phys_page();
            if (!new_pg) {
                spin_unlock(&ctx->lock);
                return -1;
            }
            uint64_t new_paddr = page_to_pfn(new_pg) * PAGE_SIZE;
            memset((void*)new_paddr, 0, PAGE_SIZE);

            if (area->file) {
                uint64_t file_off = area->offset + page_offset;
                pagecache_entry_t* pce = pagecache_get_page(area->file, file_off);
                if (pce) {
                    memcpy((void*)new_paddr, pce->page, PAGE_SIZE);
                    pagecache_put_page(pce);
                }
            }

            uint64_t cow_flags = pte_flags & ~(uint64_t)PAGE_WRITABLE;
            cow_flags |= PAGE_COW_FLAG;

            if (pmap_map_page(pml4, page_vaddr, new_paddr,
                              cow_flags) != 0) {
                free_pages(new_pg);
                spin_unlock(&ctx->lock);
                return -1;
            }

            area->flags |= MMAP_FLAG_POPULATED;

            __asm__ volatile ("invlpg (%0)" :: "r"(page_vaddr) : "memory");
            spin_unlock(&ctx->lock);
            return 0;
        }
    }

    if ((area->flags & MAP_SHARED) && area->file) {

        uint64_t file_off = area->offset + page_offset;

        pagecache_entry_t* pce = pagecache_get_page(area->file, file_off);
        if (!pce) {

            spin_unlock(&ctx->lock);
            return -1;
        }

        uint64_t paddr = (uint64_t)pce->page;
        if (pmap_map_page(pml4, page_vaddr, paddr, pte_flags) != 0) {
            pagecache_put_page(pce);
            spin_unlock(&ctx->lock);
            return -1;
        }

        pagecache_put_page(pce);
        area->flags |= MMAP_FLAG_POPULATED;

        __asm__ volatile ("invlpg (%0)" :: "r"(page_vaddr) : "memory");
        spin_unlock(&ctx->lock);
        return 0;
    }

    if (area->flags & MAP_ANONYMOUS) {
        page_t* new_pg = mmap_alloc_phys_page();
        if (!new_pg) {
            spin_unlock(&ctx->lock);
            return -1;
        }
        uint64_t new_paddr = page_to_pfn(new_pg) * PAGE_SIZE;
        memset((void*)new_paddr, 0, PAGE_SIZE);

        if (pmap_map_page(pml4, page_vaddr, new_paddr,
                          pte_flags) != 0) {
            free_pages(new_pg);
            spin_unlock(&ctx->lock);
            return -1;
        }

        area->flags |= MMAP_FLAG_POPULATED;

        __asm__ volatile ("invlpg (%0)" :: "r"(page_vaddr) : "memory");
        spin_unlock(&ctx->lock);
        return 0;
    }

    spin_unlock(&ctx->lock);
    return -1;
}

int mmap_fork_inherit(mmap_context_t* parent, mmap_context_t* child)
{
    if (!parent || !child) {
        return -1;
    }

    spin_lock(&parent->lock);
    spin_lock(&child->lock);

    for (int i = 0; i < MMAP_MAX_AREAS; i++) {
        if (!parent->areas[i].active) {
            continue;
        }

        mmap_area_t* parea = &parent->areas[i];

        int cslot = mmap_alloc_slot(child);
        if (cslot < 0) {

            spin_unlock(&child->lock);
            spin_unlock(&parent->lock);
            return -1;
        }

        mmap_area_t* carea = &child->areas[cslot];

        if (parea->flags & MAP_SHARED) {

            *carea = *parea;
            carea->ref_count = parea->ref_count + 1;

            void* child_pml4 = (void*)(uint64_t)__read_cr3();
            void* parent_pml4 = (void*)pmap_get();

            uint64_t npages = parea->length / PAGE_SIZE;
            for (uint64_t j = 0; j < npages; j++) {
                uint64_t vaddr = parea->start + j * PAGE_SIZE;
                uint64_t paddr = pmap_get_physical(parent_pml4, vaddr);
                if (paddr) {
                    uint64_t pte_flags = prot_to_pte_flags(parea->prot);
                    pmap_map_page(child_pml4, vaddr, paddr, pte_flags);
                }
            }
        }
        else {

            *carea = *parea;
            carea->ref_count = 1;

            void* child_pml4 = (void*)(uint64_t)__read_cr3();

            uint64_t npages = parea->length / PAGE_SIZE;
            for (uint64_t j = 0; j < npages; j++) {
                uint64_t vaddr = parea->start + j * PAGE_SIZE;

                cow_mark_page((uint64_t)child_pml4, vaddr);
            }

            carea->flags |= MMAP_FLAG_COW;
        }

        child->area_count++;
    }

    spin_unlock(&child->lock);
    spin_unlock(&parent->lock);
    return 0;
}

