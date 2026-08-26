/*
 * Kenux OS - Filesystem Creation Tool
 * Header file for mkfs functionality
 */

#ifndef _MKFS_H
#define _MKFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

// Maximum path length
#define MAX_PATH_LEN 4096
#define MAX_FS_NAME 32
#define MAX_LABEL_LEN 16

// Filesystem types
typedef enum {
    FS_TYPE_EXT2,
    FS_TYPE_EXT3,
    FS_TYPE_EXT4,
    FS_TYPE_VFAT,
    FS_TYPE_NTFS,
    FS_TYPE_ISO9660,
    FS_TYPE_SWAP,
    FS_TYPE_UNKNOWN
} FileSystemType;

// Filesystem statistics
typedef struct {
    size_t total_blocks;
    size_t free_blocks;
    size_t total_inodes;
    size_t free_inodes;
    size_t block_size;
    size_t blocks_per_group;
    size_t inodes_per_group;
} FileSystemStats;

// Superblock structure (simplified)
typedef struct {
    uint32_t s_magic;
    uint32_t s_block_size;
    uint32_t s_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_data_blocks_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    char s_volume_name[MAX_LABEL_LEN];
} Superblock;

// Group descriptor structure (simplified)
typedef struct {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint32_t bg_pad;
    uint32_t bg_reserved[3];
} GroupDescriptor;

// mkfs state
typedef struct {
    int device_fd;
    char device_path[MAX_PATH_LEN];
    FileSystemType fs_type;
    int verbose;
    int force;
    char label[MAX_LABEL_LEN];
    int check_interval;
    int block_size;
    Superblock superblock;
    FileSystemStats stats;
} MkfsState;

// Function prototypes
void mkfs_init(MkfsState *state);
void mkfs_cleanup(MkfsState *state);
int parse_arguments(MkfsState *state, int argc, char **argv);
int validate_device(MkfsState *state);
int create_filesystem(MkfsState *state);
int init_superblock(MkfsState *state);
int write_superblock(MkfsState *state);
int create_bitmaps(MkfsState *state);
int create_inodes(MkfsState *state);
int create_root_directory(MkfsState *state);
int verify_filesystem(MkfsState *state);
void print_filesystem_info(MkfsState *state);
void print_usage(void);
const char *get_fs_type_name(FileSystemType type);
FileSystemType get_fs_type_from_name(const char *name);

#endif /* _MKFS_H */