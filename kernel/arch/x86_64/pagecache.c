

#include <arch/pagecache.h>
#include <arch/memory.h>
#include <string.h>

static pagecache_entry_t* hash_table[PAGE_CACHE_HASH_SIZE];
static pagecache_entry_t* lru_head = NULL;
static pagecache_entry_t* lru_tail = NULL;
static spinlock_t pc_lock = SPINLOCK_INIT;
static uint64_t pc_total = 0;
static uint64_t pc_dirty = 0;

static uint32_t __hash(vfs_node_t* node, uint64_t offset)
{
    uint64_t key = (uint64_t)node ^ offset;
    return (uint32_t)(key & (PAGE_CACHE_HASH_SIZE - 1));
}

static void __lru_add(pagecache_entry_t* entry)
{
    entry->lru_next = lru_head;
    entry->lru_prev = NULL;
    if (lru_head) lru_head->lru_prev = entry;
    lru_head = entry;
    if (!lru_tail) lru_tail = entry;
}

static void __lru_remove(pagecache_entry_t* entry)
{
    if (entry->lru_prev) entry->lru_prev->lru_next = entry->lru_next;
    else lru_head = entry->lru_next;
    if (entry->lru_next) entry->lru_next->lru_prev = entry->lru_prev;
    else lru_tail = entry->lru_prev;
    entry->lru_prev = entry->lru_next = NULL;
}

static void __lru_move_to_head(pagecache_entry_t* entry)
{
    __lru_remove(entry);
    __lru_add(entry);
}

void pagecache_init(void)
{
    memset(hash_table, 0, sizeof(hash_table));
    lru_head = lru_tail = NULL;
    spin_init(&pc_lock);
    pc_total = 0;
    pc_dirty = 0;
}

pagecache_entry_t* pagecache_find_page(vfs_node_t* node, uint64_t offset)
{
    uint32_t h = __hash(node, offset);
    spin_lock(&pc_lock);

    pagecache_entry_t* entry = hash_table[h];
    while (entry) {
        if (entry->node == node && entry->offset == offset) {
            entry->ref_count++;
            __lru_move_to_head(entry);
            spin_unlock(&pc_lock);
            return entry;
        }
        entry = entry->hash_next;
    }

    spin_unlock(&pc_lock);
    return NULL;
}

pagecache_entry_t* pagecache_get_page(vfs_node_t* node, uint64_t offset)
{
    pagecache_entry_t* entry = pagecache_find_page(node, offset);
    if (entry) return entry;

    void* page = memory_alloc(PAGE_SIZE);
    if (!page) return NULL;

    entry = (pagecache_entry_t*)memory_alloc(sizeof(pagecache_entry_t));
    if (!entry) {
        memory_free(page);
        return NULL;
    }

    memset(entry, 0, sizeof(pagecache_entry_t));
    entry->node = node;
    entry->offset = offset;
    entry->page = page;
    entry->ref_count = 1;

    uint32_t h = __hash(node, offset);
    spin_lock(&pc_lock);
    entry->hash_next = hash_table[h];
    hash_table[h] = entry;
    __lru_add(entry);
    pc_total++;
    spin_unlock(&pc_lock);

    if (node && node->read) {
        node->read(node, offset, page, PAGE_SIZE);
        entry->flags |= PCACHE_UPTODATE;
    }

    return entry;
}

void pagecache_put_page(pagecache_entry_t* entry)
{
    if (!entry) return;
    spin_lock(&pc_lock);
    if (entry->ref_count > 0) entry->ref_count--;
    spin_unlock(&pc_lock);
}

void pagecache_set_dirty(pagecache_entry_t* entry)
{
    if (!entry) return;
    spin_lock(&pc_lock);
    if (!(entry->flags & PCACHE_DIRTY)) {
        entry->flags |= PCACHE_DIRTY;
        pc_dirty++;
    }
    spin_unlock(&pc_lock);
}

void pagecache_clear_dirty(pagecache_entry_t* entry)
{
    if (!entry) return;
    spin_lock(&pc_lock);
    if (entry->flags & PCACHE_DIRTY) {
        entry->flags &= ~PCACHE_DIRTY;
        pc_dirty--;
    }
    spin_unlock(&pc_lock);
}

int pagecache_read(pagecache_entry_t* entry, void* buf, uint64_t offset_in_page, uint64_t len)
{
    if (!entry || !buf || !entry->page) return -1;
    if (offset_in_page >= PAGE_SIZE) return -1;
    if (offset_in_page + len > PAGE_SIZE) len = PAGE_SIZE - offset_in_page;

    memcpy(buf, (uint8_t*)entry->page + offset_in_page, len);
    return (int)len;
}

int pagecache_write(pagecache_entry_t* entry, const void* buf, uint64_t offset_in_page, uint64_t len)
{
    if (!entry || !buf || !entry->page) return -1;
    if (offset_in_page >= PAGE_SIZE) return -1;
    if (offset_in_page + len > PAGE_SIZE) len = PAGE_SIZE - offset_in_page;

    memcpy((uint8_t*)entry->page + offset_in_page, buf, len);
    pagecache_set_dirty(entry);
    return (int)len;
}

int pagecache_sync_node(vfs_node_t* node)
{
    if (!node) return -1;
    int count = 0;

    spin_lock(&pc_lock);
    for (uint32_t h = 0; h < PAGE_CACHE_HASH_SIZE; h++) {
        pagecache_entry_t* entry = hash_table[h];
        while (entry) {
            if (entry->node == node && (entry->flags & PCACHE_DIRTY)) {
                entry->flags |= PCACHE_WRITEBACK;
                spin_unlock(&pc_lock);

                if (node->write) {
                    node->write(node, entry->offset, entry->page, PAGE_SIZE);
                }

                spin_lock(&pc_lock);
                entry->flags &= ~(PCACHE_DIRTY | PCACHE_WRITEBACK);
                pc_dirty--;
                count++;
            }
            entry = entry->hash_next;
        }
    }
    spin_unlock(&pc_lock);
    return count;
}

int pagecache_sync_all(void)
{
    int count = 0;
    spin_lock(&pc_lock);
    for (uint32_t h = 0; h < PAGE_CACHE_HASH_SIZE; h++) {
        pagecache_entry_t* entry = hash_table[h];
        while (entry) {
            if (entry->flags & PCACHE_DIRTY) {
                vfs_node_t* node = entry->node;
                entry->flags |= PCACHE_WRITEBACK;
                spin_unlock(&pc_lock);

                if (node && node->write) {
                    node->write(node, entry->offset, entry->page, PAGE_SIZE);
                }

                spin_lock(&pc_lock);
                entry->flags &= ~(PCACHE_DIRTY | PCACHE_WRITEBACK);
                pc_dirty--;
                count++;
            }
            entry = entry->hash_next;
        }
    }
    spin_unlock(&pc_lock);
    return count;
}

void pagecache_get_stats(uint64_t* total, uint64_t* dirty)
{
    if (total) *total = pc_total;
    if (dirty) *dirty = pc_dirty;
}
