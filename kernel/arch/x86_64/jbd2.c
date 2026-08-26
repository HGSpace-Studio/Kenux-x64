

#include <arch/jbd2.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/slab.h>
#include <arch/kernel.h>
#include <arch/ahci.h>
#include <wait.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define JBD2_VERSION          2
#define JBD2_DEFAULT_BLOCK_SIZE 4096
#define JBD2_CHECKPOINT_THRESHOLD 75
#define JBD2_MAX_RECOVERY_SCAN  65536

static void jbd2_trans_add_to_list(jbd2_transaction_t* trans);
static void jbd2_trans_remove_from_list(jbd2_transaction_t* trans);
static int  jbd2_write_superblock(void);
static int  jbd2_read_superblock(jbd2_superblock_t* sb);
static int  jbd2_write_block_to_journal(uint64_t journal_block_no,
                                         const void* data, uint32_t size);
static int  jbd2_read_block_from_journal(uint64_t journal_block_no,
                                         void* buf, uint32_t size);
static int  jbd2_write_block_to_device(uint64_t block_no,
                                         const void* data, uint32_t size);
static int  jbd2_read_block_from_device(uint64_t block_no,
                                         void* buf, uint32_t size);
static void jbd2_free_transaction_blocks(jbd2_transaction_t* trans);
static void jbd2_free_transaction(jbd2_transaction_t* trans);
static uint64_t jbd2_alloc_journal_block(void);
static void jbd2_free_journal_block(uint64_t journal_block_no);

jbd2_journal_t jbd2_journal = {
    .magic = 0,
    .initialized = 0,
    .journal_lock = SPINLOCK_INIT,
    .commit_lock = SPINLOCK_INIT,
};

static int jbd2_read_block_from_device(uint64_t block_no, void* buf, uint32_t size)
{
    if (buf == NULL || size == 0) {
        return -1;
    }

    uint32_t sector_size = 512;
    uint32_t sectors_per_block = jbd2_journal.dev_block_size / sector_size;
    uint64_t lba = block_no * sectors_per_block;
    uint8_t* dst = (uint8_t*)buf;
    uint32_t total_sectors = (size + sector_size - 1) / sector_size;

    for (uint32_t i = 0; i < total_sectors; i++) {
        if (ahci_read_sector(0, lba + i, dst + i * sector_size) != 0) {
            return -1;
        }
    }

    return 0;
}

static int jbd2_write_block_to_device(uint64_t block_no,
                                        const void* data, uint32_t size)
{
    if (data == NULL || size == 0) {
        return -1;
    }

    uint32_t sector_size = 512;
    uint32_t sectors_per_block = jbd2_journal.dev_block_size / sector_size;
    uint64_t lba = block_no * sectors_per_block;
    const uint8_t* src = (const uint8_t*)data;
    uint32_t total_sectors = (size + sector_size - 1) / sector_size;

    for (uint32_t i = 0; i < total_sectors; i++) {
        if (ahci_write_sector(0, lba + i, src + i * sector_size) != 0) {
            return -1;
        }
    }

    __sync_synchronize();
    return 0;
}

static int jbd2_read_block_from_journal(uint64_t journal_block_no,
                                         void* buf, uint32_t size)
{

    uint64_t abs_block = jbd2_journal.journal_start + journal_block_no;
    return jbd2_read_block_from_device(abs_block, buf, size);
}

static int jbd2_write_block_to_journal(uint64_t journal_block_no,
                                         const void* data, uint32_t size)
{
    uint64_t abs_block = jbd2_journal.journal_start + journal_block_no;
    int ret = jbd2_write_block_to_device(abs_block, data, size);

    if (ret == 0) {

        __sync_synchronize();
    }
    return ret;
}

static int jbd2_write_superblock(void)
{
    jbd2_superblock_t sb;
    memset(&sb, 0, sizeof(sb));

    sb.magic           = JBD2_MAGIC;
    sb.version         = JBD2_VERSION;
    sb.block_size      = jbd2_journal.dev_block_size;
    sb.journal_start   = jbd2_journal.journal_start;
    sb.journal_blocks  = jbd2_journal.journal_blocks;
    sb.next_tid        = jbd2_journal.next_tid;
    sb.last_commit_id  = jbd2_journal.last_checkpoint_tid;
    sb.first_commit_id = jbd2_journal.last_checkpoint_tid + 1;
    sb.free_blocks     = jbd2_journal.journal_blocks - jbd2_journal.used_blocks;

    memset(sb.uuid, 0, 16);
    uint64_t id_val = jbd2_journal.next_tid ^ jbd2_journal.journal_start;
    memcpy(sb.uuid, &id_val, sizeof(uint64_t));

    return jbd2_write_block_to_journal(0, &sb, sizeof(sb));
}

static int jbd2_read_superblock(jbd2_superblock_t* sb)
{
    if (sb == NULL) {
        return -1;
    }

    memset(sb, 0, sizeof(jbd2_superblock_t));
    return jbd2_read_block_from_journal(0, sb, sizeof(jbd2_superblock_t));
}

static uint64_t jbd2_alloc_journal_block(void)
{
    if (jbd2_journal.used_blocks >= jbd2_journal.journal_blocks) {

        return (uint64_t)-1;
    }

    uint64_t data_blocks = jbd2_journal.journal_blocks - 1;
    uint64_t block_no = (jbd2_journal.used_blocks % data_blocks) + 1;

    jbd2_journal.used_blocks++;
    return block_no;
}

static void jbd2_free_journal_block(uint64_t journal_block_no)
{
    (void)journal_block_no;
    if (jbd2_journal.used_blocks > 0) {
        jbd2_journal.used_blocks--;
    }
}

static void jbd2_trans_add_to_list(jbd2_transaction_t* trans)
{
    trans->prev = jbd2_journal.trans_list_tail;
    trans->next = NULL;

    if (jbd2_journal.trans_list_tail != NULL) {
        jbd2_journal.trans_list_tail->next = trans;
    } else {

        jbd2_journal.trans_list_head = trans;
    }
    jbd2_journal.trans_list_tail = trans;
}

static void jbd2_trans_remove_from_list(jbd2_transaction_t* trans)
{
    if (trans->prev != NULL) {
        trans->prev->next = trans->next;
    } else {
        jbd2_journal.trans_list_head = trans->next;
    }

    if (trans->next != NULL) {
        trans->next->prev = trans->prev;
    } else {
        jbd2_journal.trans_list_tail = trans->prev;
    }

    trans->prev = NULL;
    trans->next = NULL;
}

static void jbd2_free_transaction_blocks(jbd2_transaction_t* trans)
{
    if (trans == NULL) return;

    jbd2_block_t* blk = trans->block_list_head;
    while (blk != NULL) {
        jbd2_block_t* next = blk->next;

        if (blk->data != NULL) {
            kfree(blk->data);
            blk->data = NULL;
        }

        if (blk->flags & JBD2_BLOCK_IN_LOG) {
            jbd2_free_journal_block(blk->journal_block_no);
        }

        kfree(blk);
        blk = next;
    }

    trans->block_list_head = NULL;
    trans->block_list_tail = NULL;
    trans->block_count = 0;
    trans->data_block_count = 0;
}

static void jbd2_free_transaction(jbd2_transaction_t* trans)
{
    if (trans == NULL) return;

    jbd2_free_transaction_blocks(trans);
    kfree(trans);
}

int jbd2_journal_init(uint32_t dev_block_size, uint64_t journal_start,
                      uint64_t journal_blocks)
{

    if (dev_block_size == 0 || journal_blocks < 16) {
        return -1;
    }

    spin_init(&jbd2_journal.journal_lock);
    spin_init(&jbd2_journal.commit_lock);

    jbd2_journal.dev_block_size  = dev_block_size;
    jbd2_journal.journal_start  = journal_start;
    jbd2_journal.journal_blocks = journal_blocks;
    jbd2_journal.version        = JBD2_VERSION;
    jbd2_journal.next_tid      = 1;
    jbd2_journal.current_trans = NULL;
    jbd2_journal.checkpoint_trans = NULL;
    jbd2_journal.trans_list_head = NULL;
    jbd2_journal.trans_list_tail = NULL;
    jbd2_journal.used_blocks    = 0;
    jbd2_journal.last_checkpoint_tid = 0;
    jbd2_journal.magic         = JBD2_MAGIC;
    jbd2_journal.initialized   = 0;

    memset(&jbd2_journal.stats, 0, sizeof(jbd2_stats_t));

    jbd2_superblock_t sb;
    int ret = jbd2_read_superblock(&sb);

    if (ret == 0 && sb.magic == JBD2_MAGIC) {

        jbd2_journal.next_tid = sb.next_tid > 0 ? sb.next_tid : 1;
        jbd2_journal.last_checkpoint_tid = sb.last_commit_id;

        if (sb.free_blocks < journal_blocks) {
            jbd2_journal.used_blocks = journal_blocks - sb.free_blocks;
        }

        jbd2_journal.version = sb.version;
    } else if (ret == 0 && sb.magic != JBD2_MAGIC) {

        jbd2_journal.used_blocks = 0;
        jbd2_write_superblock();
    } else {

        jbd2_journal.used_blocks = 0;
        jbd2_write_superblock();
    }

    jbd2_journal.initialized = 1;
    return 0;
}

int jbd2_journal_load(void)
{
    if (!jbd2_journal.initialized) {
        return -1;
    }

    int replayed = 0;

    uint64_t data_area_blocks = jbd2_journal.journal_blocks - 1;

    void* scan_buf = kmalloc(jbd2_journal.dev_block_size);
    if (scan_buf == NULL) {
        return -1;
    }

    uint64_t max_scanned = data_area_blocks < JBD2_MAX_RECOVERY_SCAN
                           ? data_area_blocks : JBD2_MAX_RECOVERY_SCAN;

    uint64_t* recovery_blk_nos = kmalloc(sizeof(uint64_t) * max_scanned);
    uint64_t* recovery_tids    = kmalloc(sizeof(uint64_t) * max_scanned);
    uint32_t* recovery_types   = kmalloc(sizeof(uint32_t) * max_scanned);
    uint64_t  recovery_count   = 0;

    if (recovery_blk_nos == NULL || recovery_tids == NULL || recovery_types == NULL) {
        kfree(scan_buf);
        if (recovery_blk_nos) kfree(recovery_blk_nos);
        if (recovery_tids)    kfree(recovery_tids);
        if (recovery_types)   kfree(recovery_types);
        return -1;
    }

    for (uint64_t i = 0; i < max_scanned; i++) {
        uint64_t jblk = i + 1;
        if (jbd2_read_block_from_journal(jblk, scan_buf,
                                          jbd2_journal.dev_block_size) != 0) {
            continue;
        }

        jbd2_block_header_t* hdr = (jbd2_block_header_t*)scan_buf;

        if (hdr->magic != JBD2_MAGIC) {
            continue;
        }

        if (hdr->block_type != JBD2_DESCRIPTOR &&
            hdr->block_type != JBD2_COMMIT_BLOCK &&
            hdr->block_type != JBD2_REVOKED) {
            continue;
        }

        recovery_blk_nos[recovery_count] = jblk;
        recovery_tids[recovery_count]    = hdr->transaction_id;
        recovery_types[recovery_count]  = hdr->block_type;
        recovery_count++;
    }

    if (recovery_count == 0) {

        kfree(scan_buf);
        kfree(recovery_blk_nos);
        kfree(recovery_tids);
        kfree(recovery_types);
        return 0;
    }

    uint64_t max_tid = 0;
    for (uint64_t i = 0; i < recovery_count; i++) {
        if (recovery_tids[i] > max_tid) {
            max_tid = recovery_tids[i];
        }
    }
    if (max_tid == 0) {
        kfree(scan_buf);
        kfree(recovery_blk_nos);
        kfree(recovery_tids);
        kfree(recovery_types);
        return 0;
    }

    for (uint64_t tid = 1; tid <= max_tid; tid++) {
        int has_commit = 0;
        int has_blocks = 0;

        for (uint64_t i = 0; i < recovery_count; i++) {
            if (recovery_tids[i] == tid) {
                has_blocks = 1;
                if (recovery_types[i] == JBD2_COMMIT_BLOCK) {
                    has_commit = 1;
                    break;
                }
            }
        }

        if (!has_commit || !has_blocks) {
            continue;
        }

        for (uint64_t i = 0; i < recovery_count; i++) {
            if (recovery_tids[i] != tid) {
                continue;
            }

            if (recovery_types[i] != JBD2_DESCRIPTOR) {
                continue;
            }

            jbd2_block_header_t* desc_hdr;
            if (jbd2_read_block_from_journal(recovery_blk_nos[i], scan_buf,
                                              jbd2_journal.dev_block_size) != 0) {
                continue;
            }
            desc_hdr = (jbd2_block_header_t*)scan_buf;

            uint32_t num_data = desc_hdr->block_count;
            uint64_t* block_nos = (uint64_t*)(scan_buf + sizeof(jbd2_block_header_t));

            for (uint32_t b = 0; b < num_data; b++) {
                uint64_t target_block = block_nos[b];

                uint64_t data_journal_blk = recovery_blk_nos[i] + 1 + b;
                if (data_journal_blk >= jbd2_journal.journal_blocks) {
                    break;
                }

                void* data_buf = kmalloc(jbd2_journal.dev_block_size);
                if (data_buf == NULL) {
                    continue;
                }

                if (jbd2_read_block_from_journal(data_journal_blk, data_buf,
                                                  jbd2_journal.dev_block_size) == 0) {

                    jbd2_write_block_to_device(target_block, data_buf,
                                                jbd2_journal.dev_block_size);
                }

                kfree(data_buf);
            }
        }

        replayed++;

        if (tid > jbd2_journal.last_checkpoint_tid) {
            jbd2_journal.last_checkpoint_tid = tid;
        }
    }

    jbd2_journal.used_blocks = 0;

    jbd2_write_superblock();

    kfree(scan_buf);
    kfree(recovery_blk_nos);
    kfree(recovery_tids);
    kfree(recovery_types);

    return replayed;
}

void jbd2_journal_destroy(void)
{
    if (!jbd2_journal.initialized) {
        return;
    }

    spin_lock(&jbd2_journal.journal_lock);

    if (jbd2_journal.current_trans != NULL) {
        jbd2_trans_abort(jbd2_journal.current_trans);
        jbd2_journal.current_trans = NULL;
    }

    jbd2_transaction_t* trans = jbd2_journal.trans_list_head;
    while (trans != NULL) {
        jbd2_transaction_t* next = trans->next;
        jbd2_free_transaction(trans);
        trans = next;
    }

    jbd2_journal.trans_list_head = NULL;
    jbd2_journal.trans_list_tail = NULL;
    jbd2_journal.checkpoint_trans = NULL;

    jbd2_journal.used_blocks = 0;
    jbd2_write_superblock();

    jbd2_journal.initialized = 0;

    spin_unlock(&jbd2_journal.journal_lock);
}

jbd2_transaction_t* jbd2_trans_start(void)
{
    if (!jbd2_journal.initialized) {
        return NULL;
    }

    jbd2_transaction_t* trans =
        (jbd2_transaction_t*)kzalloc(sizeof(jbd2_transaction_t));
    if (trans == NULL) {
        return NULL;
    }

    spin_lock(&jbd2_journal.journal_lock);

    trans->tid               = jbd2_journal.next_tid++;
    trans->state             = JBD2_RUNNING;
    trans->block_list_head   = NULL;
    trans->block_list_tail   = NULL;
    trans->block_count       = 0;
    trans->data_block_count  = 0;
    trans->commit_timestamp  = 0;
    trans->next              = NULL;
    trans->prev              = NULL;

    wait_queue_init(&trans->sync_wait);

    memset(trans->data_blocks, 0, sizeof(trans->data_blocks));

    jbd2_trans_add_to_list(trans);

    jbd2_journal.current_trans = trans;

    jbd2_journal.stats.total_transactions++;

    spin_unlock(&jbd2_journal.journal_lock);

    return trans;
}

int jbd2_trans_log_block(jbd2_transaction_t* trans, uint64_t block_no,
                         const void* data, uint32_t size)
{
    if (trans == NULL || data == NULL || size == 0) {
        return -1;
    }

    if (!jbd2_journal.initialized) {
        return -1;
    }

    if (trans->state != JBD2_RUNNING) {
        return -2;
    }

    if (trans->data_block_count >= JBD2_TRANS_MAX_BLOCKS) {
        return -3;
    }

    spin_lock(&jbd2_journal.journal_lock);

    if (jbd2_journal.used_blocks >= jbd2_journal.journal_blocks - 2) {

        spin_unlock(&jbd2_journal.journal_lock);
        return -4;
    }

    jbd2_block_t* blk =
        (jbd2_block_t*)kzalloc(sizeof(jbd2_block_t));
    if (blk == NULL) {
        spin_unlock(&jbd2_journal.journal_lock);
        return -5;
    }

    blk->data = kmalloc(size);
    if (blk->data == NULL) {
        kfree(blk);
        spin_unlock(&jbd2_journal.journal_lock);
        return -6;
    }
    memcpy(blk->data, data, size);

    blk->journal_block_no = jbd2_alloc_journal_block();
    if (blk->journal_block_no == (uint64_t)-1) {
        kfree(blk->data);
        kfree(blk);
        spin_unlock(&jbd2_journal.journal_lock);
        return -7;
    }

    blk->block_no   = block_no;
    blk->trans      = trans;
    blk->flags      = JBD2_BLOCK_DIRTY;
    blk->data_size  = size;
    blk->next       = NULL;
    blk->prev       = trans->block_list_tail;

    if (trans->block_list_tail != NULL) {
        trans->block_list_tail->next = blk;
    } else {
        trans->block_list_head = blk;
    }
    trans->block_list_tail = blk;
    trans->block_count++;

    trans->data_blocks[trans->data_block_count] = blk;
    trans->data_block_count++;

    spin_unlock(&jbd2_journal.journal_lock);

    return 0;
}

int jbd2_trans_commit(jbd2_transaction_t* trans)
{
    if (trans == NULL) {
        return -1;
    }

    if (!jbd2_journal.initialized) {
        return -1;
    }

    if (trans->state == JBD2_FINISHED || trans->state == JBD2_COMMITTING) {
        return 0;
    }

    spin_lock(&jbd2_journal.commit_lock);

    trans->state = JBD2_LOCKED;

    if (jbd2_journal.current_trans == trans) {
        jbd2_journal.current_trans = NULL;
    }

    trans->state = JBD2_FLUSHING;

    if (trans->block_count > 0) {

        void* desc_buf = kmalloc(jbd2_journal.dev_block_size);
        if (desc_buf == NULL) {
            trans->state = JBD2_LOCKED;
            spin_unlock(&jbd2_journal.commit_lock);
            return -2;
        }
        memset(desc_buf, 0, jbd2_journal.dev_block_size);

        jbd2_block_header_t* desc_hdr = (jbd2_block_header_t*)desc_buf;
        desc_hdr->magic          = JBD2_MAGIC;
        desc_hdr->block_type     = JBD2_DESCRIPTOR;
        desc_hdr->transaction_id = trans->tid;
        desc_hdr->block_count    = trans->block_count;

        uint64_t* block_nos = (uint64_t*)(desc_buf + sizeof(jbd2_block_header_t));
        jbd2_block_t* blk = trans->block_list_head;
        uint32_t idx = 0;
        while (blk != NULL && idx < trans->block_count) {
            block_nos[idx] = blk->block_no;
            idx++;
            blk = blk->next;
        }

        uint64_t desc_journal_blk = jbd2_alloc_journal_block();
        if (desc_journal_blk == (uint64_t)-1) {
            kfree(desc_buf);
            trans->state = JBD2_LOCKED;
            spin_unlock(&jbd2_journal.commit_lock);
            return -3;
        }

        if (jbd2_write_block_to_journal(desc_journal_blk, desc_buf,
                                          jbd2_journal.dev_block_size) != 0) {
            jbd2_free_journal_block(desc_journal_blk);
            kfree(desc_buf);
            trans->state = JBD2_LOCKED;
            spin_unlock(&jbd2_journal.commit_lock);
            return -4;
        }
        kfree(desc_buf);

        blk = trans->block_list_head;
        while (blk != NULL) {
            void* data_buf = kmalloc(jbd2_journal.dev_block_size);
            if (data_buf == NULL) {

                trans->state = JBD2_LOCKED;
                spin_unlock(&jbd2_journal.commit_lock);
                return -5;
            }
            memset(data_buf, 0, jbd2_journal.dev_block_size);

            jbd2_block_header_t* data_hdr = (jbd2_block_header_t*)data_buf;
            data_hdr->magic          = JBD2_MAGIC;
            data_hdr->block_type     = JBD2_DESCRIPTOR;
            data_hdr->transaction_id = trans->tid;
            data_hdr->block_count    = 1;

            uint32_t data_offset = sizeof(jbd2_block_header_t);
            uint32_t copy_size = blk->data_size;
            if (data_offset + copy_size > jbd2_journal.dev_block_size) {
                copy_size = jbd2_journal.dev_block_size - data_offset;
            }
            memcpy((uint8_t*)data_buf + data_offset, blk->data, copy_size);

            int ret = jbd2_write_block_to_journal(blk->journal_block_no,
                                                    data_buf,
                                                    jbd2_journal.dev_block_size);
            kfree(data_buf);

            if (ret != 0) {
                trans->state = JBD2_LOCKED;
                spin_unlock(&jbd2_journal.commit_lock);
                return -6;
            }

            blk->flags |= JBD2_BLOCK_COMMITTED | JBD2_BLOCK_IN_LOG;

            blk = blk->next;
        }
    }

    trans->state = JBD2_COMMITTING;

    void* commit_buf = kmalloc(jbd2_journal.dev_block_size);
    if (commit_buf == NULL) {
        trans->state = JBD2_LOCKED;
        spin_unlock(&jbd2_journal.commit_lock);
        return -7;
    }
    memset(commit_buf, 0, jbd2_journal.dev_block_size);

    jbd2_block_header_t* commit_hdr = (jbd2_block_header_t*)commit_buf;
    commit_hdr->magic          = JBD2_MAGIC;
    commit_hdr->block_type     = JBD2_COMMIT_BLOCK;
    commit_hdr->transaction_id = trans->tid;
    commit_hdr->block_count    = trans->block_count;

    uint64_t commit_journal_blk = jbd2_alloc_journal_block();
    if (commit_journal_blk == (uint64_t)-1) {
        kfree(commit_buf);
        trans->state = JBD2_LOCKED;
        spin_unlock(&jbd2_journal.commit_lock);
        return -8;
    }

    if (jbd2_write_block_to_journal(commit_journal_blk, commit_buf,
                                      jbd2_journal.dev_block_size) != 0) {
        jbd2_free_journal_block(commit_journal_blk);
        kfree(commit_buf);
        trans->state = JBD2_LOCKED;
        spin_unlock(&jbd2_journal.commit_lock);
        return -9;
    }
    kfree(commit_buf);

    __sync_synchronize();

    trans->state = JBD2_FINISHED;

    trans->commit_timestamp = jbd2_journal.next_tid;

    jbd2_journal.stats.committed_transactions++;

    jbd2_journal.checkpoint_trans = trans;

    wait_queue_wake_all(&trans->sync_wait);

    jbd2_write_superblock();

    spin_unlock(&jbd2_journal.commit_lock);

    return 0;
}

void jbd2_trans_abort(jbd2_transaction_t* trans)
{
    if (trans == NULL) {
        return;
    }

    spin_lock(&jbd2_journal.journal_lock);

    jbd2_block_t* blk = trans->block_list_head;
    while (blk != NULL) {
        blk->flags |= JBD2_BLOCK_REVOKED;
        blk->flags &= ~JBD2_BLOCK_DIRTY;

        if (blk->flags & JBD2_BLOCK_IN_LOG) {
            jbd2_free_journal_block(blk->journal_block_no);
        }

        blk = blk->next;
    }

    jbd2_journal.stats.aborted_transactions++;

    wait_queue_wake_all(&trans->sync_wait);

    if (jbd2_journal.current_trans == trans) {
        jbd2_journal.current_trans = NULL;
    }

    jbd2_trans_remove_from_list(trans);

    jbd2_write_superblock();

    spin_unlock(&jbd2_journal.journal_lock);

    jbd2_free_transaction(trans);
}

int jbd2_journal_flush(void)
{
    if (!jbd2_journal.initialized) {
        return -1;
    }

    spin_lock(&jbd2_journal.commit_lock);

    int ret = jbd2_journal_checkpoint();

    if (ret >= 0) {

        jbd2_transaction_t* trans = jbd2_journal.trans_list_head;
        while (trans != NULL) {
            jbd2_transaction_t* next = trans->next;

            if (trans->state == JBD2_FINISHED) {

                int all_written = 1;
                jbd2_block_t* blk = trans->block_list_head;
                while (blk != NULL) {
                    if (!(blk->flags & JBD2_BLOCK_WRITTEN)) {

                        if (blk->data != NULL && blk->data_size > 0) {
                            jbd2_write_block_to_device(blk->block_no,
                                                       blk->data,
                                                       blk->data_size);
                        }
                        blk->flags |= JBD2_BLOCK_WRITTEN;
                        all_written = 0;
                    }
                    blk = blk->next;
                }

                if (all_written) {

                    jbd2_trans_remove_from_list(trans);
                    jbd2_free_transaction(trans);
                }
            }

            trans = next;
        }

        jbd2_journal.used_blocks = 0;

        jbd2_write_superblock();
    }

    spin_unlock(&jbd2_journal.commit_lock);

    return ret;
}

int jbd2_journal_checkpoint(void)
{
    if (!jbd2_journal.initialized) {
        return -1;
    }

    int freed_blocks = 0;

    spin_lock(&jbd2_journal.commit_lock);

    jbd2_transaction_t* trans = jbd2_journal.trans_list_head;
    while (trans != NULL) {
        jbd2_transaction_t* next = trans->next;

        if (trans->state != JBD2_FINISHED) {
            trans = next;
            continue;
        }

        jbd2_block_t* blk = trans->block_list_head;
        while (blk != NULL) {
            if (blk->data != NULL && blk->data_size > 0) {

                int ret = jbd2_write_block_to_device(blk->block_no,
                                                      blk->data,
                                                      blk->data_size);
                if (ret == 0) {
                    blk->flags |= JBD2_BLOCK_WRITTEN;
                    blk->flags &= ~JBD2_BLOCK_DIRTY;
                }
            }

            if (blk->flags & JBD2_BLOCK_IN_LOG) {
                jbd2_free_journal_block(blk->journal_block_no);
                freed_blocks++;
            }
            blk->flags &= ~JBD2_BLOCK_IN_LOG;

            blk = blk->next;
        }

        if (trans->tid > jbd2_journal.last_checkpoint_tid) {
            jbd2_journal.last_checkpoint_tid = trans->tid;
        }

        jbd2_trans_remove_from_list(trans);

        jbd2_free_transaction(trans);

        trans = next;
    }

    jbd2_journal.stats.checkpointed = jbd2_journal.last_checkpoint_tid;

    jbd2_write_superblock();

    spin_unlock(&jbd2_journal.commit_lock);

    return freed_blocks;
}

void jbd2_get_stats(jbd2_stats_t* stats)
{
    if (stats == NULL || !jbd2_journal.initialized) {
        return;
    }

    spin_lock(&jbd2_journal.journal_lock);

    stats->total_transactions     = jbd2_journal.stats.total_transactions;
    stats->committed_transactions = jbd2_journal.stats.committed_transactions;
    stats->aborted_transactions    = jbd2_journal.stats.aborted_transactions;
    stats->checkpointed           = jbd2_journal.stats.checkpointed;
    stats->journal_used_blocks     = jbd2_journal.used_blocks;
    stats->journal_free_blocks     = jbd2_journal.journal_blocks - jbd2_journal.used_blocks;

    if (jbd2_journal.journal_blocks > 0) {
        stats->journal_usage_percent =
            (uint32_t)(jbd2_journal.used_blocks * 100 /
                        jbd2_journal.journal_blocks);
    } else {
        stats->journal_usage_percent = 0;
    }

    spin_unlock(&jbd2_journal.journal_lock);
}
