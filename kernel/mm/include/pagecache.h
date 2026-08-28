#ifndef KERNEL_MM_PAGECACHE_H
#define KERNEL_MM_PAGECACHE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define PAGECACHE_HASH_BUCKETS  4096
#define PAGECACHE_MAX_PAGES     65536
#define PAGECACHE_READAHEAD     16
#define PAGE_SIZE               4096

typedef struct pagecache_page {
    uint64_t                    page_index;
    void*                       data;
    struct pagecache_page*      hash_next;
    struct pagecache_page*      lru_prev;
    struct pagecache_page*      lru_next;
    int                         dirty;
    int                         ref_count;
    int                         valid;
    uint64_t                    fs_id;
    uint64_t                    inode_id;
} pagecache_page_t;

typedef struct {
    int (*read_page)(uint64_t fs_id, uint64_t inode_id, uint64_t page_index, void* buf);
    int (*write_page)(uint64_t fs_id, uint64_t inode_id, uint64_t page_index, const void* buf);
} pagecache_ops_t;

typedef struct {
    pagecache_page_t*           hash_table[PAGECACHE_HASH_BUCKETS];
    pagecache_page_t*           lru_head;
    pagecache_page_t*           lru_tail;
    pagecache_page_t*           page_pool;
    int                         total_pages;
    int                         used_pages;
    int                         dirty_pages;
    uint64_t                    hits;
    uint64_t                    misses;
    pagecache_ops_t             ops;
    spinlock_t                  lock;
} pagecache_t;

void           pagecache_init(pagecache_t* pc, int max_pages, pagecache_ops_t* ops);
pagecache_page_t* pagecache_get(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                                uint64_t page_index);
void           pagecache_release(pagecache_t* pc, pagecache_page_t* page);
int            pagecache_read(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                              uint64_t offset, void* buf, uint32_t size);
int            pagecache_write(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                               uint64_t offset, const void* buf, uint32_t size);
int            pagecache_mark_dirty(pagecache_t* pc, pagecache_page_t* page);
int            pagecache_flush_page(pagecache_t* pc, pagecache_page_t* page);
int            pagecache_flush_inode(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id);
int            pagecache_flush_all(pagecache_t* pc);
void           pagecache_invalidate(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id);
void           pagecache_invalidate_range(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                                           uint64_t start, uint64_t end);
void           pagecache_readahead(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                                    uint64_t page_index, int count);
void           pagecache_truncate(pagecache_t* pc, uint64_t fs_id, uint64_t inode_id,
                                   uint64_t new_size);
uint64_t       pagecache_get_hits(pagecache_t* pc);
uint64_t       pagecache_get_misses(pagecache_t* pc);

#endif