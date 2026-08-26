#ifndef ARCH_X86_64_EXT2_H
#define ARCH_X86_64_EXT2_H

#include <arch/types.h>
#include <arch/ahci.h>

#define EXT2_MAGIC 0xEF53
#define EXT2_BLOCK_SIZE 1024
#define EXT2_MAX_FILENAME 256
#define EXT2_MAX_INODES 1024
#define EXT2_MAX_BLOCKS 65536

#define EXT2_INODE_REGULAR 0x8000
#define EXT2_INODE_DIRECTORY 0x4000

#define EXT2_FLAG_DIR_INODE 2
#define EXT2_FLAG_DIR_DATA 3

typedef struct {
    uint32_t inode_count;
    uint32_t block_count;
    uint32_t reserved_blocks;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_fragment_size;
    uint32_t blocks_per_group;
    uint32_t fragments_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mount_count;
    uint16_t max_mount_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t last_checked;
    uint32_t check_interval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t uid_reserved;
    uint16_t gid_reserved;
    uint32_t first_inode;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    char volume_name[16];
    char last_mounted[64];
    uint32_t algo_bitmap;
} __attribute__((packed)) ext2_superblock_t;

typedef struct {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks;
    uint16_t free_inodes;
    uint16_t used_dirs;
    uint16_t pad;
    uint32_t reserved[3];
} __attribute__((packed)) ext2_group_desc_t;

typedef struct {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t faddr;
    uint8_t osd2[12];
} __attribute__((packed)) ext2_inode_t;

typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[EXT2_MAX_FILENAME];
} __attribute__((packed)) ext2_dir_entry_t;

typedef struct {
    uint32_t block_size;
    ext2_superblock_t* superblock;
    ext2_group_desc_t* group_desc;
    uint32_t block_groups;
} ext2_fs_t;

void ext2_init(void);
int ext2_mount(const char* device, const char* mount_point);
int ext2_unmount(const char* mount_point);
int ext2_open(const char* path);
int ext2_close(int fd);
int ext2_read(int fd, void* buffer, uint64_t size);
int ext2_write(int fd, const void* buffer, uint64_t size);
int ext2_seek(int fd, uint64_t offset);
int ext2_mkdir(const char* path);
int ext2_rmdir(const char* path);
int ext2_unlink(const char* path);

#endif
