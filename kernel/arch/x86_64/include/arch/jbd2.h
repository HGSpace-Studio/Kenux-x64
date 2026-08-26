

#ifndef ARCH_JBD2_H
#define ARCH_JBD2_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <wait.h>

#define JBD2_MAGIC             0xC03B3998

#define JBD2_RUNNING           0
#define JBD2_LOCKED            1
#define JBD2_FLUSHING          2
#define JBD2_COMMITTING        3
#define JBD2_FINISHED          4

#define JBD2_DESCRIPTOR        1
#define JBD2_COMMIT_BLOCK      2
#define JBD2_REVOKED           3

#define JBD2_BLOCK_DIRTY       0x01
#define JBD2_BLOCK_COMMITTED   0x02
#define JBD2_BLOCK_REVOKED     0x04
#define JBD2_BLOCK_IN_LOG      0x08
#define JBD2_BLOCK_WRITTEN     0x10

#define JBD2_SUPERBLOCK_SIZE   4096

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint64_t journal_start;
    uint64_t journal_blocks;
    uint64_t first_commit_id;
    uint64_t last_commit_id;
    uint64_t next_tid;
    uint64_t free_blocks;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t  uuid[16];
    uint8_t  padding[1024];
} __attribute__((packed)) jbd2_superblock_t;

typedef struct {
    uint32_t magic;
    uint32_t block_type;
    uint64_t transaction_id;
    uint32_t block_count;
    uint8_t  padding[4080];
} __attribute__((packed)) jbd2_block_header_t;

typedef struct {
    uint64_t total_transactions;
    uint64_t committed_transactions;
    uint64_t aborted_transactions;
    uint64_t checkpointed;
    uint64_t journal_used_blocks;
    uint64_t journal_free_blocks;
    uint32_t journal_usage_percent;
} jbd2_stats_t;

struct jbd2_journal;
struct jbd2_transaction;
struct jbd2_block;

typedef struct jbd2_block {
    uint64_t block_no;
    uint64_t journal_block_no;
    struct jbd2_transaction* trans;
    uint32_t flags;
    uint32_t data_size;
    void*    data;
    struct jbd2_block* next;
    struct jbd2_block* prev;
} jbd2_block_t;

#define JBD2_TRANS_MAX_BLOCKS   128

typedef struct jbd2_transaction {
    uint64_t tid;
    uint32_t state;
    jbd2_block_t* block_list_head;
    jbd2_block_t* block_list_tail;
    uint32_t block_count;
    jbd2_block_t* data_blocks[JBD2_TRANS_MAX_BLOCKS];
    uint32_t data_block_count;
    uint64_t commit_timestamp;
    wait_queue_t sync_wait;
    struct jbd2_transaction* next;
    struct jbd2_transaction* prev;
} jbd2_transaction_t;

typedef struct jbd2_journal {
    uint32_t magic;
    uint32_t version;
    uint64_t dev_id;
    uint32_t dev_block_size;
    uint64_t journal_start;
    uint64_t journal_blocks;
    uint64_t next_tid;
    jbd2_transaction_t* current_trans;
    jbd2_transaction_t* checkpoint_trans;
    jbd2_transaction_t* trans_list_head;
    jbd2_transaction_t* trans_list_tail;
    spinlock_t commit_lock;
    spinlock_t journal_lock;
    uint64_t used_blocks;
    uint64_t last_checkpoint_tid;
    jbd2_stats_t stats;
    uint32_t initialized;
} jbd2_journal_t;

extern jbd2_journal_t jbd2_journal;

int jbd2_journal_init(uint32_t dev_block_size, uint64_t journal_start,
                      uint64_t journal_blocks);

int jbd2_journal_load(void);

void jbd2_journal_destroy(void);

jbd2_transaction_t* jbd2_trans_start(void);

int jbd2_trans_log_block(jbd2_transaction_t* trans, uint64_t block_no,
                         const void* data, uint32_t size);

int jbd2_trans_commit(jbd2_transaction_t* trans);

void jbd2_trans_abort(jbd2_transaction_t* trans);

int jbd2_journal_flush(void);

int jbd2_journal_checkpoint(void);

void jbd2_get_stats(jbd2_stats_t* stats);

#endif
