#ifndef ARCH_X86_64_EXFAT_H
#define ARCH_X86_64_EXFAT_H

#include <arch/types.h>
#include <arch/fs.h>

#define EXFAT_SIGNATURE       0xAA550000
#define EXFAT_VOLUME_FLAGS    0x0001
#define EXFAT_MEDIA_TYPE      0xF8

#define EXFAT_ENTRY_END              0x00
#define EXFAT_ENTRY_BITMAP           0x81
#define EXFAT_ENTRY_UPCASE           0x82
#define EXFAT_ENTRY_VOLUME_LABEL     0x83
#define EXFAT_ENTRY_FILE             0x85
#define EXFAT_ENTRY_STREAM           0xC0
#define EXFAT_ENTRY_FILE_NAME        0xC1

#define EXFAT_FILE_ATTR_READONLY     0x0001
#define EXFAT_FILE_ATTR_HIDDEN       0x0002
#define EXFAT_FILE_ATTR_SYSTEM       0x0004
#define EXFAT_FILE_ATTR_VOLUME       0x0008
#define EXFAT_FILE_ATTR_DIRECTORY    0x0010
#define EXFAT_FILE_ATTR_ARCHIVE      0x0020

#define EXFAT_MAX_NAME_LEN          255
#define EXFAT_MAX_DENTRY_NAME_LEN   15

#define EXFAT_CLUSTER_FREE           0x00000000
#define EXFAT_CLUSTER_END            0xFFFFFFFF

typedef struct {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint8_t  reserved0[53];
    uint64_t partition_offset;
    uint64_t volume_length;
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t root_directory;
    uint32_t volume_serial;
    uint16_t fs_revision;
    uint16_t volume_flags;
    uint8_t  bytes_per_sector_shift;
    uint8_t  sectors_per_cluster_shift;
    uint8_t  number_of_fats;
    uint8_t  drive_select;
    uint8_t  percent_in_use;
    uint8_t  reserved1[7];
    uint8_t  boot_code[390];
    uint16_t signature;
} __attribute__((packed)) exfat_bpb_t;

typedef struct {
    uint8_t  type;
    uint8_t  secondary_count;
    uint16_t checksum;
    uint16_t attributes;
    uint8_t  reserved1[2];
    uint32_t create_time;
    uint32_t modify_time;
    uint32_t access_time;
    uint8_t  create_time_cs;
    uint8_t  modify_time_cs;
    uint8_t  access_time_cs;
    uint8_t  reserved2[9];
} __attribute__((packed)) exfat_file_entry_t;

typedef struct {
    uint8_t  type;
    uint8_t  flags;
    uint8_t  reserved1;
    uint8_t  name_length;
    uint16_t name_hash;
    uint16_t reserved2;
    uint32_t valid_data_length;
    uint32_t reserved3;
    uint64_t data_length;
    uint32_t start_cluster;
    uint32_t reserved4;
} __attribute__((packed)) exfat_stream_entry_t;

typedef struct {
    uint8_t  type;
    uint8_t  flags;
    uint16_t name[EXFAT_MAX_DENTRY_NAME_LEN];
} __attribute__((packed)) exfat_name_entry_t;

typedef struct {
    uint8_t  type;
    uint8_t  bitmap_flags;
    uint8_t  reserved[18];
    uint32_t start_cluster;
    uint64_t size;
} __attribute__((packed)) exfat_bitmap_entry_t;

typedef struct {
    uint8_t  type;
    uint8_t  reserved1[3];
    uint32_t checksum;
    uint8_t  reserved2[12];
    uint32_t start_cluster;
    uint64_t size;
} __attribute__((packed)) exfat_upcase_entry_t;

typedef struct {
    exfat_bpb_t bpb;
    uint32_t    bytes_per_sector;
    uint32_t    sectors_per_cluster;
    uint32_t    bytes_per_cluster;
    uint32_t    fat_start_sector;
    uint32_t    fat_sectors;
    uint32_t    cluster_heap_start;
    uint32_t    total_clusters;
    uint32_t    root_dir_cluster;
    uint32_t*   fat_table;
    uint64_t    fat_table_size;
    void*       device;
    spinlock_t  lock;
    int         mounted;
} exfat_fs_t;

typedef struct {
    uint16_t attributes;
    uint32_t create_time;
    uint32_t modify_time;
    uint32_t access_time;
    uint32_t start_cluster;
    uint64_t data_length;
    uint16_t name_hash;
    uint8_t  name_len;
    uint16_t name[EXFAT_MAX_NAME_LEN + 1];
} exfat_dir_info_t;

void exfat_init(void);
int  exfat_mount(vfs_node_t* mount_point, void* device);
int  exfat_unmount(exfat_fs_t* fs);
uint32_t exfat_cluster_to_sector(exfat_fs_t* fs, uint32_t cluster);
uint32_t exfat_get_next_cluster(exfat_fs_t* fs, uint32_t cluster);
int  exfat_set_next_cluster(exfat_fs_t* fs, uint32_t cluster, uint32_t next);
uint32_t exfat_alloc_cluster(exfat_fs_t* fs);
void  exfat_free_cluster(exfat_fs_t* fs, uint32_t cluster);
int  exfat_read_cluster(exfat_fs_t* fs, uint32_t cluster, void* buf);
int  exfat_write_cluster(exfat_fs_t* fs, uint32_t cluster, const void* buf);
int  exfat_read_dir(exfat_fs_t* fs, uint32_t dir_cluster, exfat_dir_info_t* entries, int max);
int  exfat_create_entry(exfat_fs_t* fs, uint32_t dir_cluster, const uint16_t* name, uint8_t name_len, uint16_t attr, uint32_t* out_cluster);
int  exfat_delete_entry(exfat_fs_t* fs, uint32_t dir_cluster, const uint16_t* name, uint8_t name_len);
int  exfat_read_file(exfat_fs_t* fs, uint32_t start_cluster, void* buf, uint64_t offset, uint64_t size);
int  exfat_write_file(exfat_fs_t* fs, uint32_t start_cluster, const void* buf, uint64_t offset, uint64_t size);
uint16_t exfat_name_hash(const uint16_t* name, uint8_t len);

#endif