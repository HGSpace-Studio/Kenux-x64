#ifndef ARCH_EXT4_H
#define ARCH_EXT4_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/fs.h>
#include <arch/jbd2.h>

#define EXT4_SUPER_MAGIC     0xEF53
#define EXT4_NAME_LEN        255
#define EXT4_BLOCK_SIZE(s)   (1024 << (s)->s_log_block_size)
#define EXT4_INODE_SIZE(s)   ((s)->s_inode_size)
#define EXT4_INODES_PER_GROUP(s) ((s)->s_inodes_per_group)
#define EXT4_BLOCKS_PER_GROUP(s) ((s)->s_blocks_per_group)

#define EXT4_NDIR_BLOCKS     12
#define EXT4_IND_BLOCK       12
#define EXT4_DIND_BLOCK      13
#define EXT4_TIND_BLOCK      14
#define EXT4_N_BLOCKS        15

#define EXT4_FT_UNKNOWN      0
#define EXT4_FT_REG_FILE     1
#define EXT4_FT_DIR          2
#define EXT4_FT_CHRDEV       3
#define EXT4_FT_BLKDEV       4
#define EXT4_FT_FIFO         5
#define EXT4_FT_SOCK         6
#define EXT4_FT_SYMLINK      7

#define EXT4_SB_OFFSET       1024
#define EXT4_SB_BLOCK        1
#define EXT4_MIN_BLOCK_SIZE  1024
#define EXT4_MAX_BLOCK_SIZE  65536

#define EXT4_FEATURE_COMPAT_HAS_JOURNAL      0x0004
#define EXT4_FEATURE_INCOMPAT_EXTENTS          0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT            0x0080
#define EXT4_FEATURE_INCOMPAT_FLEX_BG          0x0200
#define EXT4_FEATURE_INCOMPAT_DIRDATA         0x1000

#define EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER   0x0001
#define EXT4_FEATURE_RO_COMPAT_LARGE_FILE     0x0002
#define EXT4_FEATURE_RO_COMPAT_BTREE_DIR      0x0004
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE      0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM       0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK      0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE    0x0040
#define EXT4_FEATURE_RO_COMPAT_QUOTA          0x0100
#define EXT4_FEATURE_RO_COMPAT_BIGALLOC       0x0200
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM  0x0400

#define EXT4_EXT_MAGIC       0xF30A
#define EXT4_EXTENT_TAIL     0xF30C
#define EXT4_MAX_DEPTH       4
#define EXT4_EXT_ZERO_LEN    0x8000

#define EXT4_HTREE_MAGIC     0xEEEE

#define EXT4_SECRM_FL        0x00000001
#define EXT4_UNRM_FL         0x00000002
#define EXT4_COMPR_FL        0x00000004
#define EXT4_SYNC_FL         0x00000008
#define EXT4_IMMUTABLE_FL    0x00000010
#define EXT4_APPEND_FL       0x00000020
#define EXT4_NODUMP_FL       0x00000040
#define EXT4_NOATIME_FL      0x00000080
#define EXT4_DIRTY_FL        0x00000100
#define EXT4_COMPRBLK_FL     0x00000200
#define EXT4_NOCOMPR_FL      0x00000400
#define EXT4_ECOMPR_FL       0x00000800
#define EXT4_INDEX_FL        0x00001000
#define EXT4_IMAGIC_FL       0x00002000
#define EXT4_JOURNAL_DATA_FL 0x00004000
#define EXT4_NOTAIL_FL       0x00008000
#define EXT4_DIRSYNC_FL      0x00010000
#define EXT4_TOPDIR_FL       0x00020000
#define EXT4_HUGE_FILE_FL    0x00040000
#define EXT4_EXTENTS_FL      0x00080000
#define EXT4_EA_INODE_FL     0x00200000
#define EXT4_EOFBLOCKS_FL    0x00400000

typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count_lo;
    uint32_t s_r_blocks_count_lo;
    uint32_t s_free_blocks_count_lo;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_cluster_size;
    uint32_t s_blocks_per_group;
    uint32_t s_clusters_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_count_hi;
    uint16_t s_min_extra_isize;
    uint16_t s_want_extra_isize;
    uint32_t s_flags;
    uint16_t s_raid_stride;
    uint16_t s_mmp_interval;
    uint64_t s_mmp_block;
    uint32_t s_raid_stripe_width;
    uint8_t  s_log_groups_per_flex;
    uint8_t  s_checksum_type;
    uint16_t s_reserved_pad;
    uint64_t s_kbytes_written;
    uint32_t s_snapshot_inum;
    uint32_t s_snapshot_id;
    uint64_t s_snapshot_r_blocks_count;
    uint32_t s_snapshot_list;
    uint32_t s_error_count;
    uint32_t s_first_error_time;
    uint32_t s_first_error_ino;
    uint64_t s_first_error_block;
    uint8_t  s_first_error_func[32];
    uint32_t s_first_error_line;
    uint32_t s_last_error_time;
    uint32_t s_last_error_ino;
    uint64_t s_last_error_block;
    uint8_t  s_last_error_func[32];
    uint32_t s_last_error_line;
    uint8_t  s_mount_opts[64];
    uint32_t s_usr_quota_inum;
    uint32_t s_grp_quota_inum;
    uint32_t s_overhead_blocks;
    uint32_t s_backup_bgs[2];
    uint8_t  s_encrypt_algos[4];
    uint8_t  s_encrypt_pw_salt[16];
    uint32_t s_lpf_ino;
    uint32_t s_prj_quota_inum;
    uint32_t s_checksum_seed;
    uint8_t  s_reserved[98];
    uint32_t s_checksum;
} __attribute__((packed)) ext4_superblock_t;

typedef struct {
    uint32_t bg_block_bitmap_lo;
    uint32_t bg_inode_bitmap_lo;
    uint32_t bg_inode_table_lo;
    uint16_t bg_free_blocks_count_lo;
    uint16_t bg_free_inodes_count_lo;
    uint16_t bg_used_dirs_count_lo;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap_lo;
    uint16_t bg_block_bitmap_csum_lo;
    uint16_t bg_inode_bitmap_csum_lo;
    uint16_t bg_itable_unused_lo;
    uint16_t bg_checksum;
    uint32_t bg_block_bitmap_hi;
    uint32_t bg_inode_bitmap_hi;
    uint32_t bg_inode_table_hi;
    uint16_t bg_free_blocks_count_hi;
    uint16_t bg_free_inodes_count_hi;
    uint16_t bg_used_dirs_count_hi;
    uint16_t bg_itable_unused_hi;
    uint32_t bg_exclude_bitmap_hi;
    uint16_t bg_block_bitmap_csum_hi;
    uint16_t bg_inode_bitmap_csum_hi;
    uint32_t bg_reserved;
} __attribute__((packed)) ext4_group_desc_t;

typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size_lo;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[EXT4_N_BLOCKS];
    uint32_t i_generation;
    uint32_t i_file_acl_lo;
    uint32_t i_size_high;
    uint32_t i_obso_faddr;
    uint16_t i_blocks_high;
    uint16_t i_file_acl_high;
    uint16_t i_uid_high;
    uint16_t i_gid_high;
    uint16_t i_checksum_lo;
    uint16_t i_reserved;
} __attribute__((packed)) ext4_inode_t;

typedef struct {
    uint16_t eh_magic;
    uint16_t eh_entries;
    uint16_t eh_max;
    uint16_t eh_depth;
    uint32_t eh_generation;
} __attribute__((packed)) ext4_extent_header_t;

typedef struct {
    uint32_t ei_block;
    uint32_t ei_leaf_lo;
    uint16_t ei_leaf_hi;
    uint16_t ei_unused;
} __attribute__((packed)) ext4_extent_idx_t;

typedef struct {
    uint32_t ee_block;
    uint16_t ee_len;
    uint16_t ee_start_hi;
    uint32_t ee_start_lo;
} __attribute__((packed)) ext4_extent_t;

typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[EXT4_NAME_LEN];
} __attribute__((packed)) ext4_dir_entry_t;

typedef struct {
    uint32_t fake_dirent_inode;
    uint16_t fake_dirent_rec_len;
    uint8_t  fake_dirent_name_len;
    uint8_t  fake_dirent_file_type;
    uint32_t dx_root_hash_version;
    uint16_t dx_root_info_len;
    uint8_t  dx_root_indirect_levels;
    uint8_t  dx_root_unused_flags;
    uint32_t dx_root_block_limit;
    uint16_t dx_root_count;
    uint16_t dx_root_block;
    uint32_t dx_root_parent;
    struct {
        uint32_t hash;
        uint32_t block;
    } dx_entry[0];
} __attribute__((packed)) ext4_dx_root_t;

typedef struct {
    uint32_t fake_dirent_inode;
    uint16_t fake_dirent_rec_len;
    uint8_t  fake_dirent_name_len;
    uint8_t  fake_dirent_file_type;
    uint16_t dx_entry_limit;
    uint16_t dx_entry_count;
    uint32_t dx_entry_block;
    struct {
        uint32_t hash;
        uint32_t block;
    } dx_entry[0];
} __attribute__((packed)) ext4_dx_node_t;

typedef struct {
    int (*read)(struct vfs_node* node, uint64_t offset, void* buf, uint64_t size);
    int (*write)(struct vfs_node* node, uint64_t offset, const void* buf, uint64_t size);
    int (*open)(struct vfs_node* node, int flags);
    int (*close)(struct vfs_node* node);
    struct vfs_node* (*finddir)(struct vfs_node* node, const char* name);
    int (*readdir)(struct vfs_node* node, int index, char* name, uint64_t max_len);
    int (*mkdir)(struct vfs_node* parent, const char* name, uint64_t mode);
    int (*unlink)(struct vfs_node* parent, const char* name);
} ext4_file_operations_t;

typedef struct {
    vfs_node_t* root;
    ext4_superblock_t sb;
    ext4_group_desc_t* gdt;
    uint8_t* block_bitmap;
    uint8_t* inode_bitmap;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t inodes_per_group;
    uint32_t blocks_per_group;
    uint32_t group_count;
    uint32_t desc_size;
    uint64_t blocks_count;
    uint32_t first_data_block;
    uint32_t feature_incompat;
    uint32_t feature_compat;
    uint32_t feature_ro_compat;
    jbd2_journal_t* journal;
    ext4_file_operations_t fops;
    spinlock_t lock;
    int mounted;
    char device[32];
} ext4_fs_t;

void ext4_init(void);
int ext4_mount(const char* device, vfs_node_t** root_out);
int ext4_unmount(ext4_fs_t* fs);
int ext4_read_inode(ext4_fs_t* fs, uint32_t ino, ext4_inode_t* inode);
int ext4_read_block(ext4_fs_t* fs, uint64_t block, void* buf);
int ext4_write_block(ext4_fs_t* fs, uint64_t block, const void* buf);
int ext4_read_extent_block(ext4_fs_t* fs, ext4_inode_t* inode, uint64_t lblock,
                           uint64_t* pblock, uint32_t* max_blocks);
int ext4_read_file(ext4_fs_t* fs, ext4_inode_t* inode, void* buf, uint64_t offset, uint64_t len);
int ext4_dir_iterate(ext4_fs_t* fs, ext4_inode_t* inode,
                     int (*cb)(ext4_dir_entry_t* entry, void* arg), void* arg);
int ext4_lookup(ext4_fs_t* fs, uint32_t parent_ino, const char* name, uint32_t* out_ino);
int ext4_replay_journal(ext4_fs_t* fs);

static inline uint64_t ext4_inode_size64(const ext4_inode_t* inode)
{
    return ((uint64_t)inode->i_size_high << 32) | inode->i_size_lo;
}

static inline uint64_t ext4_blocks_count64(const ext4_superblock_t* sb)
{
    return ((uint64_t)sb->s_blocks_count_hi << 32) | sb->s_blocks_count_lo;
}

#endif
