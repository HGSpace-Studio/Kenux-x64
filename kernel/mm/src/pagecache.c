#include "pagecache.h"
#include <arch/memory.h>
#include <string.h>

static uint32_t pagecache_hash(uint64_t fs_id, uint64_t inode_id, uint64_t page_index)
{
    uint64_t h = fs_id * 2654435761ULL + inode_id * 2246822519ULL + page_index * 3266489917ULL;
    return (uint32_t)(h ^ (h >> 32)) % PAGECACHE_HASH_BUCKETS;
}

static void lru_remove(pagecache_t* pc, pagecache_page_t* page)
{
    if (page->lru_prev) page->lru_prev->lru_next = page->lru_next;
    else pc->lru_head = page->lru_next;
    if (page->lru_next) page->lru_next->lru_prev = page->lru_prev;
    else pc->lru_tail = page->lru_prev;
    page->lru_prev = NULL;
    page->lru_next = NULL;
}

static void lru_insert_head(pagecache_t* pc, pagecache_page_t* page)
{
    page->lru_prev = NULL;
    page->lru_next = pc->lru_head;
    if (pc->lru_head) pc->lru_head->lru_prev = page;
    else pc->lru_tail = page;
    pc->lru_head = page;
}

static void lru_move_head(pagecache_t* pc, pagecache_page_t* page)
{
    lru_remove(pc, page);
    lru_insert_head(pc, page);
}

static pagecache_page_t* lru_evict(pagecache_t* pc)
{
    pagecache_page_t* victim = pc->lru_tail;
    while (victim) {
        if (victim->ref_count == 0) {
            if (victim->dirty) {
                if (pc->ops.write_page) {
                    pc->ops.write_page(victim->fs_id, victim->inode_id,
                                       victim->page_index, victim->data);
                }
                victim->dirty = 0;
                pc->dirty_pages--;
            }
            lru_remove(pc, victim);
            uint32_t bucket = pagecache_hash(victim->fs_id, victim->inode_id, victim->page_index);
            pagecache_page_t** pp = &pc->hash_table[bucket];
            while (*pp) {
                if (*pp == victim) { *pp = victim->hash_next; break; }
                pp = &(*pp)->hash_next;
            }
            return victim;
        }
        victim = victim->lru_prev;
    }
    return NULL;
}

void pagecache_init(pagecache_t* pc, int max_pages, pagecache_ops_t* ops)
{
    if (!pc) return;
    memset(pc, 0, sizeof(pagecache_t));
    spin_init(&pc->lock);

    if (max_pages > PAGECACHE_MAX_PAGES) max_pages = PAGECACHE_MAX_PAGES;
    pc->total_pages = max_pages;
    pc->used_pages = 0;
    pc->dirty_pages = 0;
    pc->hits = 0;
    pc->misses = 0;

    if (ops) pc->ops = *ops;

    pc->page_pool = (pagecache_page_t*)memory_alloc(sizeof(pagecache_page_t) * max_pages);
    if (!pc->page_pool) return;

    for (int i = 0; i < max_pages; i++) {
        pc->page_pool[i].data = memory_alloc(PAGE_SIZE);
        pc->page_pool[i].hash_next = NULL;
        pc->page_pool[i].lru_prev = NULL;
        pc->page_pool[i].lru_next = NULL;
        pc->page_pool[i].dirty = 0;
        pc->page_pool[i].ref_count = 0;
        pc->page_pool[i].valid = 0;
        pc->page_pool[i].page_index = 0;
        pc->page_pool[i].fs_id = 0;
        pc->page_pool[i].inode_id = 0;
    }
}

pagecache_page_t* pagecache_get(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                                uint64_t page_index)
{
    if (!pc) return NULL;

    uint32_t bucket = pagecache_hash(fs_id, inode_id, page_index);

    spinlock_acquire(&pc->lock);

    pagecache_page_t* page = pc->hash_table[bucket];
    while (page) {
        if (page->fs_id == fs_id && page->inode_id == inode_id &&
            page->page_index == page_index && page->valid) {
            page->ref_count++;
            lru_move_head(pc, page);
            pc->hits++;
            spinlock_release(&pc->lock);
            return page;
        }
        page = page->hash_next;
    }

    pc->misses++;

    pagecache_page_t* new_page = NULL;
    if (pc->used_pages < pc->total_pages) {
        new_page = &pc->page_pool[pc->used_pages++];
    } else {
        new_page = lru_evict(pc);
    }

    if (!new_page) {
        spinlock_release(&pc->lock);
        return NULL;
    }

    new_page->fs_id = fs_id;
    new_page->inode_id = inode_id;
    new_page->page_index = page_index;
    new_page->ref_count = 1;
    new_page->dirty = 0;
    new_page->valid = 1;

    if (pc->ops.read_page) {
        int result = pc->ops.read_page(fs_id, inode_id, page_index, new_page->data);
        if (result != 0) {
            new_page->valid = 0;
            new_page->ref_count = 0;
            spinlock_release(&pc->lock);
            return NULL;
        }
    }

    new_page->hash_next = pc->hash_table[bucket];
    pc->hash_table[bucket] = new_page;
    lru_insert_head(pc, new_page);

    spinlock_release(&pc->lock);
    return new_page;
}

void pagecache_release(pagecache_t* pc, pagecache_page_t* page)
{
    if (!pc || !page) return;
    spinlock_acquire(&pc->lock);
    if (page->ref_count > 0) page->ref_count--;
    spinlock_release(&pc->lock);
}

int pagecache_read(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                   uint64_t offset, void* buf, uint32_t size)
{
    if (!pc || !buf || size == 0) return -1;

    uint8_t* dst = (uint8_t*)buf;
    uint32_t remaining = size;
    uint64_t pos = offset;

    while (remaining > 0) {
        uint64_t page_index = pos / PAGE_SIZE;
        uint32_t page_offset = (uint32_t)(pos % PAGE_SIZE);
        uint32_t to_copy = PAGE_SIZE - page_offset;
        if (to_copy > remaining) to_copy = remaining;

        pagecache_page_t* page = pagecache_get(pc, fs_id, inode_id, page_index);
        if (!page) return -2;

        memcpy(dst, (uint8_t*)page->data + page_offset, to_copy);
        pagecache_release(pc, page);

        dst += to_copy;
        pos += to_copy;
        remaining -= to_copy;
    }

    return (int)size;
}

int pagecache_write(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                    uint64_t offset, const void* buf, uint32_t size)
{
    if (!pc || !buf || size == 0) return -1;

    const uint8_t* src = (const uint8_t*)buf;
    uint32_t remaining = size;
    uint64_t pos = offset;

    while (remaining > 0) {
        uint64_t page_index = pos / PAGE_SIZE;
        uint32_t page_offset = (uint32_t)(pos % PAGE_SIZE);
        uint32_t to_copy = PAGE_SIZE - page_offset;
        if (to_copy > remaining) to_copy = remaining;

        pagecache_page_t* page = pagecache_get(pc, fs_id, inode_id, page_index);
        if (!page) return -2;

        memcpy((uint8_t*)page->data + page_offset, src, to_copy);
        page->dirty = 1;
        pc->dirty_pages++;
        pagecache_release(pc, page);

        src += to_copy;
        pos += to_copy;
        remaining -= to_copy;
    }

    return (int)size;
}

int pagecache_mark_dirty(pagecache_t* pc, pagecache_page_t* page)
{
    if (!pc || !page) return -1;
    spinlock_acquire(&pc->lock);
    if (!page->dirty) {
        page->dirty = 1;
        pc->dirty_pages++;
    }
    spinlock_release(&pc->lock);
    return 0;
}

int pagecache_flush_page(pagecache_t* pc, pagecache_page_t* page)
{
    if (!pc || !page) return -1;
    spinlock_acquire(&pc->lock);
    if (page->dirty && pc->ops.write_page) {
        int result = pc->ops.write_page(page->fs_id, page->inode_id,
                                        page->page_index, page->data);
        if (result == 0) {
            page->dirty = 0;
            pc->dirty_pages--;
        }
        spinlock_release(&pc->lock);
        return result;
    }
    spinlock_release(&pc->lock);
    return 0;
}

int pagecache_flush_inode(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id)
{
    if (!pc) return -1;
    spinlock_acquire(&pc->lock);
    int count = 0;
    for (int i = 0; i < pc->total_pages; i++) {
        pagecache_page_t* page = &pc->page_pool[i];
        if (page->valid && page->dirty && page->fs_id == fs_id &&
            page->inode_id == inode_id && pc->ops.write_page) {
            int result = pc->ops.write_page(page->fs_id, page->inode_id,
                                            page->page_index, page->data);
            if (result == 0) {
                page->dirty = 0;
                pc->dirty_pages--;
                count++;
            }
        }
    }
    spinlock_release(&pc->lock);
    return count;
}

int pagecache_flush_all(pagecache_t* pc)
{
    if (!pc) return -1;
    spinlock_acquire(&pc->lock);
    int count = 0;
    for (int i = 0; i < pc->total_pages; i++) {
        pagecache_page_t* page = &pc->page_pool[i];
        if (page->valid && page->dirty && pc->ops.write_page) {
            int result = pc->ops.write_page(page->fs_id, page->inode_id,
                                            page->page_index, page->data);
            if (result == 0) {
                page->dirty = 0;
                pc->dirty_pages--;
                count++;
            }
        }
    }
    spinlock_release(&pc->lock);
    return count;
}

void pagecache_invalidate(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id)
{
    if (!pc) return;
    spinlock_acquire(&pc->lock);
    for (int i = 0; i < pc->total_pages; i++) {
        pagecache_page_t* page = &pc->page_pool[i];
        if (page->valid && page->fs_id == fs_id && page->inode_id == inode_id) {
            if (page->ref_count == 0) {
                lru_remove(pc, page);
                uint32_t bucket = pagecache_hash(page->fs_id, page->inode_id, page->page_index);
                pagecache_page_t** pp = &pc->hash_table[bucket];
                while (*pp) {
                    if (*pp == page) { *pp = page->hash_next; break; }
                    pp = &(*pp)->hash_next;
                }
                page->valid = 0;
                page->dirty = 0;
                page->hash_next = NULL;
            }
        }
    }
    spinlock_release(&pc->lock);
}

void pagecache_invalidate_range(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                                uint64_t start, uint64_t end)
{
    if (!pc) return;
    uint64_t start_page = start / PAGE_SIZE;
    uint64_t end_page = (end + PAGE_SIZE - 1) / PAGE_SIZE;

    spinlock_acquire(&pc->lock);
    for (int i = 0; i < pc->total_pages; i++) {
        pagecache_page_t* page = &pc->page_pool[i];
        if (page->valid && page->fs_id == fs_id && page->inode_id == inode_id &&
            page->page_index >= start_page && page->page_index < end_page &&
            page->ref_count == 0) {
            lru_remove(pc, page);
            uint32_t bucket = pagecache_hash(page->fs_id, page->inode_id, page->page_index);
            pagecache_page_t** pp = &pc->hash_table[bucket];
            while (*pp) {
                if (*pp == page) { *pp = page->hash_next; break; }
                pp = &(*pp)->hash_next;
            }
            page->valid = 0;
            page->dirty = 0;
            page->hash_next = NULL;
        }
    }
    spinlock_release(&pc->lock);
}

void pagecache_readahead(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                          uint64_t page_index, int count)
{
    if (!pc || count <= 0) return;
    for (int i = 0; i < count; i++) {
        pagecache_page_t* page = pagecache_get(pc, fs_id, inode_id, page_index + i);
        if (page) pagecache_release(pc, page);
        else break;
    }
}

void pagecache_truncate(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                         uint64_t new_size)
{
    if (!pc) return;
    uint64_t last_page = (new_size + PAGE_SIZE - 1) / PAGE_SIZE;
    pagecache_invalidate_range(pc, fs_id, inode_id, last_page * PAGE_SIZE, UINT64_MAX);
    (void)inode_id;
}

uint64_t pagecache_get_hits(pagecache_t* pc)
{
    if (!pc) return 0;
    return pc->hits;
}

uint64_t pagecache_get_misses(pagecache_t* pc)
{
    if (!pc) return 0;
    return pc->misses;
}