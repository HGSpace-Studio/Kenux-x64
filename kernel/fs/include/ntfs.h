#ifndef KERNEL_FS_NTFS_H
#define KERNEL_FS_NTFS_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define NTFS_SECTOR_SIZE       512
#define NTFS_MAGIC             0x4E544653
#define NTFS_MFT_RECORD_SIZE  1024
#define NTFS_IDX_RECORD_SIZE  4096

#define NTFS_FILE_MFT          0
#define NTFS_FILE_MFTMIRR      1
#define NTFS_FILE_LOGFILE      2
#define NTFS_FILE_VOLUME       3
#define NTFS_FILE_ATTRDEF      4
#define NTFS_FILE_ROOT         5
#define NTFS_FILE_BITMAP       6
#define NTFS_FILE_BOOT         7
#define NTFS_FILE_BADCLUS      8
#define NTFS_FILE_QUOTA        9
#define NTFS_FILE_SECURE       10
#define NTFS_FILE_UPCASE       11
#define NTFS_FILE_EXTEND       12

#define NTFS_ATTR_STANDARD_INFORMATION  0x10
#define NTFS_ATTR_ATTRIBUTE_LIST        0x20
#define NTFS_ATTR_FILE_NAME            0x30
#define NTFS_ATTR_OBJECT_ID            0x40
#define NTFS_ATTR_SECURITY_DESCRIPTOR  0x50
#define NTFS_ATTR_VOLUME_NAME          0x60
#define NTFS_ATTR_VOLUME_INFORMATION   0x70
#define NTFS_ATTR_DATA                 0x80
#define NTFS_ATTR_INDEX_ROOT           0x90
#define NTFS_ATTR_INDEX_ALLOCATION     0xA0
#define NTFS_ATTR_BITMAP               0xB0
#define NTFS_ATTR_REPARSE_POINT        0xC0
#define NTFS_ATTR_EA_INFORMATION       0xD0
#define NTFS_ATTR_EA                   0xE0
#define NTFS_ATTR_LOGGED_UTILITY_STREAM 0x100

#define NTFS_ATTR_FLAG_COMPRESSED   0x0001
#define NTFS_ATTR_FLAG_RESIDENT     0x0000
#define NTFS_ATTR_FLAG_NON_RESIDENT 0x0001
#define NTFS_ATTR_FLAG_ENCRYPTED    0x4000
#define NTFS_ATTR_FLAG_SPARSE       0x8000

#define NTFS_FILE_FLAG_READONLY     0x0001
#define NTFS_FILE_FLAG_HIDDEN       0x0002
#define NTFS_FILE_FLAG_SYSTEM       0x0004
#define NTFS_FILE_FLAG_ARCHIVE      0x0020
#define NTFS_FILE_FLAG_DEVICE       0x0040
#define NTFS_FILE_FLAG_TEMPORARY    0x0100
#define NTFS_FILE_FLAG_SPARSE       0x0200
#define NTFS_FILE_FLAG_REPARSE      0x0400
#define NTFS_FILE_FLAG_COMPRESSED   0x0800
#define NTFS_FILE_FLAG_OFFLINE      0x1000
#define NTFS_FILE_FLAG_DIRECTORY    0x10000000
#define NTFS_FILE_FLAG_INDEX_VIEW   0x20000000

typedef struct {
    uint8_t  jump[3];
    uint8_t  oem_id[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t unused[5];
    uint64_t mft_cluster;
    uint64_t mftmirr_cluster;
    int8_t   clusters_per_mft_record;
    int8_t   clusters_per_idx_record;
    uint64_t volume_serial;
    uint32_t checksum;
} __attribute__((packed)) ntfs_boot_sector_t;

typedef struct {
    uint32_t type;
    uint32_t length;
    uint8_t  non_resident;
    uint8_t  name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attribute_number;
} __attribute__((packed)) ntfs_attr_header_t;

typedef struct {
    uint32_t attr_type;
    uint32_t attr_length;
    uint8_t  name_length;
    uint16_t name_offset;
    uint32_t data_length;
    uint16_t data_offset;
    uint8_t  indexed;
    uint8_t  padding;
} __attribute__((packed)) ntfs_attr_list_entry_t;

typedef struct {
    uint64_t start_vcn;
    uint64_t start_lcn;
    uint64_t length;
} ntfs_run_t;

typedef struct {
    uint32_t magic;
    uint16_t usa_offset;
    uint16_t usa_count;
    uint64_t lsn;
    uint16_t sequence_number;
    uint16_t link_count;
    uint16_t attr_offset;
    uint16_t flags;
    uint32_t used_size;
    uint32_t allocated_size;
} __attribute__((packed)) ntfs_mft_record_header_t;

typedef struct {
    uint64_t parent_directory;
    uint64_t creation_time;
    uint64_t modification_time;
    uint64_t data_modification_time;
    uint64_t access_time;
    uint64_t allocated_size;
    uint64_t data_size;
    uint32_t file_attributes;
    uint32_t reparse_point_tag;
    uint8_t  filename_length;
    uint8_t  filename_type;
    uint16_t filename[1];
} __attribute__((packed)) ntfs_filename_attr_t;

typedef struct {
    void*         device;
    ntfs_boot_sector_t boot;
    uint64_t      mft_start;
    uint32_t      mft_record_size;
    uint32_t      idx_record_size;
    uint32_t      cluster_size;
    uint64_t      volume_size;
    uint64_t      mft_size;
    uint8_t*      mft_cache;
    uint32_t      mft_cache_size;
    spinlock_t    lock;
} ntfs_fs_t;

int      ntfs_mount(ntfs_fs_t* fs, void* device);
int      ntfs_umount(ntfs_fs_t* fs);
int      ntfs_read_mft_record(ntfs_fs_t* fs, uint64_t mft_index, void* buf);
int      ntfs_find_attr(ntfs_fs_t* fs, const void* mft_record, uint32_t attr_type,
                         const void** attr_data, uint32_t* attr_size);
int      ntfs_decode_runs(const uint8_t* run_data, ntfs_run_t* runs, int max_runs);
int      ntfs_read_attr(ntfs_fs_t* fs, const void* mft_record, uint32_t attr_type,
                         uint64_t offset, void* buf, uint32_t size);
int      ntfs_lookup(ntfs_fs_t* fs, uint64_t dir_mft, const char* name, uint64_t* out_mft);
int      ntfs_read_dir(ntfs_fs_t* fs, uint64_t dir_mft, int (*callback)(const char* name, uint64_t mft, void* ctx), void* ctx);
int      ntfs_create(ntfs_fs_t* fs, uint64_t dir_mft, const char* name, uint32_t flags);
int      ntfs_unlink(ntfs_fs_t* fs, uint64_t dir_mft, const char* name);
int      ntfs_read_file(ntfs_fs_t* fs, uint64_t mft_index, uint64_t offset, void* buf, uint32_t size);
int      ntfs_write_file(ntfs_fs_t* fs, uint64_t mft_index, uint64_t offset, const void* buf, uint32_t size);
uint64_t ntfs_get_file_size(ntfs_fs_t* fs, uint64_t mft_index);
uint32_t ntfs_get_file_attrs(ntfs_fs_t* fs, uint64_t mft_index);
uint64_t ntfs_alloc_cluster(ntfs_fs_t* fs);
void     ntfs_free_cluster(ntfs_fs_t* fs, uint64_t cluster);

#endif