#ifndef ARCH_X86_64_EXT3_H
#define ARCH_X86_64_EXT3_H

#include <arch/types.h>
#include <arch/ext2.h>
#include <arch/jbd2.h>
#include <arch/spinlock.h>
#include <arch/fs.h>

#define EXT3_MAGIC             0xEF53
#define EXT3_HAS_JOURNAL       0x0004

#define EXT3_JOURNAL_INODE     8
#define EXT3_JOURNAL_REVOKE    0x00000001
#define EXT3_JOURNAL_COMMIT    0x00000002

#define EXT3_FEATURE_COMPAT_HAS_JOURNAL      0x0004
#define EXT3_FEATURE_COMPAT_RESIZE_INODE     0x0010
#define EXT3_FEATURE_COMPAT_DIR_INDEX        0x0020

#define EXT3_FEATURE_INCOMPAT_COMPRESSION    0x0001
#define EXT3_FEATURE_INCOMPAT_FILETYPE       0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER        0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV    0x0008

#define EXT3_FEATURE_RO_COMPAT_SPARSE_SUPER  0x0001
#define EXT3_FEATURE_RO_COMPAT_LARGE_FILE    0x0002
#define EXT3_FEATURE_RO_COMPAT_BTREE_DIR     0x0004

#define EXT3_MAX_FS  16
#define EXT3_MAX_TX  64

typedef struct {
    uint32_t transaction_id;
    uint32_t num_blocks;
    uint32_t* block_numbers;
    void**   block_data;
    uint32_t flags;
} ext3_transaction_t;

typedef struct {
    ext2_superblock_t  superblock;
    ext2_group_desc_t* group_desc;
    uint32_t           block_size;
    uint32_t           inode_size;
    uint32_t           inodes_per_group;
    uint32_t           blocks_per_group;
    uint32_t           group_count;
    uint32_t           journal_inode;
    uint32_t           journal_dev;
    uint32_t           first_data_block;

    jbd2_journal_t*    journal;

    ext3_transaction_t transactions[EXT3_MAX_TX];
    uint32_t           active_tx;
    spinlock_t         tx_lock;

    void*              device;
    int                mounted;
    spinlock_t         lock;
} ext3_fs_t;

void ext3_init(void);
int  ext3_mount(vfs_node_t* mount_point, void* device);
int  ext3_unmount(ext3_fs_t* fs);
int  ext3_read_block(ext3_fs_t* fs, uint32_t block, void* buf);
int  ext3_write_block(ext3_fs_t* fs, uint32_t block, const void* buf);
int  ext3_journal_start(ext3_fs_t* fs);
int  ext3_journal_commit(ext3_fs_t* fs);
int  ext3_journal_abort(ext3_fs_t* fs);
int  ext3_recover(ext3_fs_t* fs);
ext2_inode_t* ext3_get_inode(ext3_fs_t* fs, uint32_t ino);
int  ext3_read_inode_data(ext3_fs_t* fs, ext2_inode_t* inode, uint64_t offset, void* buf, uint64_t size);
int  ext3_write_inode_data(ext3_fs_t* fs, ext2_inode_t* inode, uint64_t offset, const void* buf, uint64_t size);
uint32_t ext3_alloc_block(ext3_fs_t* fs);
void  ext3_free_block(ext3_fs_t* fs, uint32_t block);
uint32_t ext3_alloc_inode(ext3_fs_t* fs);
void  ext3_free_inode(ext3_fs_t* fs, uint32_t ino);

#endif