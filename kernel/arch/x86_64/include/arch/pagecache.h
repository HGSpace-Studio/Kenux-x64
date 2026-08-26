

#ifndef ARCH_PAGECACHE_H
#define ARCH_PAGECACHE_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/fs.h>

#define PAGE_CACHE_HASH_BITS    10
#define PAGE_CACHE_HASH_SIZE    (1 << PAGE_CACHE_HASH_BITS)
#define PAGE_CACHE_MAX_PAGES    4096

#define PCACHE_UPTODATE     0x01
#define PCACHE_DIRTY        0x02
#define PCACHE_LOCKED       0x04
#define PCACHE_WRITEBACK    0x08

typedef struct pagecache_entry {
    struct pagecache_entry* hash_next;
    struct pagecache_entry* lru_prev;
    struct pagecache_entry* lru_next;

    vfs_node_t* node;
    uint64_t    offset;
    void*       page;
    uint32_t    flags;
    uint32_t    ref_count;
} pagecache_entry_t;

void pagecache_init(void);

pagecache_entry_t* pagecache_get_page(vfs_node_t* node, uint64_t offset);
pagecache_entry_t* pagecache_find_page(vfs_node_t* node, uint64_t offset);

void pagecache_put_page(pagecache_entry_t* entry);

void pagecache_set_dirty(pagecache_entry_t* entry);
void pagecache_clear_dirty(pagecache_entry_t* entry);

int pagecache_sync_node(vfs_node_t* node);
int pagecache_sync_all(void);

int pagecache_read(pagecache_entry_t* entry, void* buf, uint64_t offset_in_page, uint64_t len);
int pagecache_write(pagecache_entry_t* entry, const void* buf, uint64_t offset_in_page, uint64_t len);

void pagecache_get_stats(uint64_t* total, uint64_t* dirty);

#endif
