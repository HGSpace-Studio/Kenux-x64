#ifndef ARCH_X86_64_FAT_H
#define ARCH_X86_64_FAT_H

#include <arch/types.h>
#include <arch/fs.h>

#define FAT12       12
#define FAT16       16
#define FAT32       32

#define FAT_SECTOR_SIZE     512
#define FAT_MAX_ROOT_ENTRIES 512
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F

#define FAT_CLUSTER_FREE        0x0000
#define FAT_CLUSTER_EOC12       0x0FF8
#define FAT_CLUSTER_EOC16       0xFFF8
#define FAT_CLUSTER_EOC32       0x0FFFFFF8
#define FAT_CLUSTER_BAD12       0x0FF7
#define FAT_CLUSTER_BAD16       0xFFF7
#define FAT_CLUSTER_BAD32       0x0FFFFFF7

typedef struct {
    uint8_t  jmp_boot[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    union {
        struct {
            uint8_t  drive_number;
            uint8_t  reserved1;
            uint8_t  boot_sig;
            uint32_t volume_id;
            char     volume_label[11];
            char     fs_type[8];
        } fat16;
        struct {
            uint32_t sectors_per_fat_32;
            uint16_t ext_flags;
            uint16_t fs_version;
            uint32_t root_cluster;
            uint16_t fs_info_sector;
            uint16_t backup_boot_sector;
            uint8_t  reserved[12];
            uint8_t  drive_number;
            uint8_t  reserved1;
            uint8_t  boot_sig;
            uint32_t volume_id;
            char     volume_label[11];
            char     fs_type[8];
        } fat32;
    } ext;
} __attribute__((packed)) fat_bpb_t;

typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat_dir_entry_t;

typedef struct {
    int type;
    fat_bpb_t bpb;
    uint32_t total_sectors;
    uint32_t sectors_per_fat;
    uint32_t root_dir_sectors;
    uint32_t first_data_sector;
    uint32_t total_clusters;
    uint32_t root_cluster;
    uint32_t bytes_per_cluster;
    uint32_t* fat_cache;
    uint32_t fat_cache_size;
    void*    device;
    spinlock_t lock;
} fat_fs_t;

int fat_mount(vfs_node_t* mount_point, void* device);
int fat_unmount(fat_fs_t* fs);
int fat_read_cluster(fat_fs_t* fs, uint32_t cluster, void* buf);
int fat_write_cluster(fat_fs_t* fs, uint32_t cluster, const void* buf);
uint32_t fat_get_next_cluster(fat_fs_t* fs, uint32_t cluster);
int fat_set_next_cluster(fat_fs_t* fs, uint32_t cluster, uint32_t next);
uint32_t fat_alloc_cluster(fat_fs_t* fs);
void fat_free_cluster(fat_fs_t* fs, uint32_t cluster);
int fat_read_dir(fat_fs_t* fs, uint32_t dir_cluster, fat_dir_entry_t* entries, int max);
int fat_create_entry(fat_fs_t* fs, uint32_t dir_cluster, const char* name,
                     uint8_t attr, uint32_t* out_cluster);
int fat_delete_entry(fat_fs_t* fs, uint32_t dir_cluster, const char* name);
int fat_read_file(fat_fs_t* fs, uint32_t start_cluster, void* buf, uint32_t offset, uint32_t size);
int fat_write_file(fat_fs_t* fs, uint32_t start_cluster, const void* buf, uint32_t offset, uint32_t size);
uint32_t fat_cluster_to_sector(fat_fs_t* fs, uint32_t cluster);
uint32_t fat_sector_to_cluster(fat_fs_t* fs, uint32_t sector);
int fat_format(void* device, int type, const char* label, uint32_t total_sectors);

#endif