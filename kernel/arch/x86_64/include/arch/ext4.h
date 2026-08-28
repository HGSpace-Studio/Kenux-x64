#ifndef ARCH_X86_64_EXT4_H
#define ARCH_X86_64_EXT4_H

#include <arch/types.h>
#include <arch/ext3.h>

#define EXT4_FEATURE_COMPAT_EXT_ATTR      0x0008
#define EXT4_FEATURE_COMPAT_RESIZE_INODE  0x0010
#define EXT4_FEATURE_COMPAT_DIR_INDEX     0x0020
#define EXT4_FEATURE_COMPAT_SPARSE_SUPER2 0x0200

#define EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER  0x0001
#define EXT4_FEATURE_RO_COMPAT_LARGE_FILE    0x0002
#define EXT4_FEATURE_RO_COMPAT_BTREE_DIR     0x0004
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE     0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM      0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK     0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE   0x0040
#define EXT4_FEATURE_RO_COMPAT_QUOTA         0x0100
#define EXT4_FEATURE_RO_COMPAT_BIGALLOC      0x0200
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM 0x0400
#define EXT4_FEATURE_RO_COMPAT_READONLY      0x1000
#define EXT4_FEATURE_RO_COMPAT_PROJECT       0x2000

#define EXT4_FEATURE_INCOMPAT_COMPRESSION    0x0001
#define EXT4_FEATURE_INCOMPAT_FILETYPE       0x0002
#define EXT4_FEATURE_INCOMPAT_RECOVER        0x0004
#define EXT4_FEATURE_INCOMPAT_JOURNAL_DEV    0x0008
#define EXT4_FEATURE_INCOMPAT_META_BG        0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS        0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT          0x0080
#define EXT4_FEATURE_INCOMPAT_MMP            0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG        0x0200
#define EXT4_FEATURE_INCOMPAT_EA_INODE       0x0400
#define EXT4_FEATURE_INCOMPAT_DIRDATA        0x1000
#define EXT4_FEATURE_INCOMPAT_CSUM_SEED      0x2000
#define EXT4_FEATURE_INCOMPAT_LARGEDIR       0x4000
#define EXT4_FEATURE_INCOMPAT_INLINE_DATA    0x8000
#define EXT4_FEATURE_INCOMPAT_ENCRYPT        0x10000

#define EXT4_EXTENT_FLAGS_UNINIT    0x0002
#define EXT4_EXTENT_INIT_MAX_LEN    0x8000

typedef struct {
    uint32_t ee_block;
    uint16_t ee_len;
    uint16_t ee_start_hi;
    uint32_t ee_start_lo;
} __attribute__((packed)) ext4_extent_t;

typedef struct {
    uint16_t eh_magic;
    uint16_t eh_entries;
    uint16_t eh_max;
    uint16_t eh_depth;
    uint32_t eh_generation;
} __attribute__((packed)) ext4_extent_header_t;

typedef struct {
    uint32_t ei_block;
    uint32_t ei_start_lo;
    uint16_t ei_start_hi;
    uint16_t ei_unused;
} __attribute__((packed)) ext4_extent_idx_t;

#define EXT4_EXT_MAGIC    0xA30A

typedef struct {
    ext3_fs_t base;
    uint32_t  feature_compat;
    uint32_t  feature_ro_compat;
    uint32_t  feature_incompat;
    uint32_t  first_data_block;
    uint32_t  inode_size;
    uint32_t  desc_size;
    uint64_t  desc_count;
    int       has_extents;
    int       has_64bit;
    int       has_flex_bg;
    int       has_huge_file;
    int       has_metadata_csum;
    uint32_t  flex_bg_size;
    uint32_t  csum_seed;
    uint32_t  s_desc_size;
    uint32_t  s_reserved_gdt_blocks;
    uint64_t* group_desc_cache;
} ext4_fs_t;

int  ext4_mount(vfs_node_t* mount_point, void* device);
int  ext4_umount(ext4_fs_t* fs);
int  ext4_read_inode(ext4_fs_t* fs, uint32_t ino, void* buf);
int  ext4_write_inode(ext4_fs_t* fs, uint32_t ino, const void* buf);
int  ext4_read_file(ext4_fs_t* fs, uint32_t ino, void* buf, uint64_t offset, uint64_t size);
int  ext4_write_file(ext4_fs_t* fs, uint32_t ino, const void* buf, uint64_t offset, uint64_t size);
int  ext4_truncate(ext4_fs_t* fs, uint32_t ino, uint64_t new_size);
int  ext4_lookup(ext4_fs_t* fs, uint32_t dir_ino, const char* name, uint32_t* out_ino);
int  ext4_create(ext4_fs_t* fs, uint32_t dir_ino, const char* name, uint32_t mode, uint32_t* out_ino);
int  ext4_mkdir(ext4_fs_t* fs, uint32_t dir_ino, const char* name, uint32_t mode);
int  ext4_unlink(ext4_fs_t* fs, uint32_t dir_ino, const char* name);
int  ext4_symlink(ext4_fs_t* fs, uint32_t dir_ino, const char* name, const char* target);
int  ext4_rename(ext4_fs_t* fs, uint32_t old_dir, const char* old_name,
                 uint32_t new_dir, const char* new_name);
uint32_t ext4_alloc_inode(ext4_fs_t* fs, int is_dir);
void     ext4_free_inode(ext4_fs_t* fs, uint32_t ino);
uint64_t ext4_alloc_block(ext4_fs_t* fs);
void     ext4_free_block(ext4_fs_t* fs, uint64_t block);
int  ext4_extent_map(ext4_fs_t* fs, uint32_t ino, uint64_t logical_block,
                      uint64_t* physical_block, uint32_t* length);
int  ext4_extent_insert(ext4_fs_t* fs, uint32_t ino, uint64_t logical_block,
                         uint64_t physical_block, uint32_t length);
uint32_t ext4_crc32c(uint32_t crc, const void* buf, uint32_t size);
uint16_t ext4_group_desc_csum(ext4_fs_t* fs, uint32_t group);

#endif