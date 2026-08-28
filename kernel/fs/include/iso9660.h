#ifndef KERNEL_FS_ISO9660_H
#define KERNEL_FS_ISO9660_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define ISO9660_SECTOR_SIZE     2048
#define ISO9660_VD_PRIMARY      1
#define ISO9660_VD_SUPPLEMENTARY 2
#define ISO9660_VD_TERMINATOR   255
#define ISO9660_VD_BOOT         0

#define ISO9660_FILE_FLAG_HIDDEN    0x01
#define ISO9660_FILE_FLAG_DIRECTORY 0x02
#define ISO9660_FILE_FLAG_ASSOCIATED 0x04
#define ISO9660_FILE_FLAG_EXTENDED  0x08
#define ISO9660_FILE_FLAG_PERMISSIONS 0x10
#define ISO9660_FILE_FLAG_MULTI_EXT 0x80

typedef struct {
    uint8_t  type;
    char     identifier[5];
    uint8_t  version;
    char     system_identifier[32];
    char     volume_identifier[32];
    uint8_t  unused1[8];
    uint32_t volume_space_size_le;
    uint32_t volume_space_size_be;
    uint8_t  unused2[32];
    uint16_t volume_set_size_le;
    uint16_t volume_set_size_be;
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint16_t logical_block_size_le;
    uint16_t logical_block_size_be;
    uint32_t path_table_size_le;
    uint32_t path_table_size_be;
    uint32_t l_path_table_loc_le;
    uint32_t l_path_table_loc_be;
    uint32_t m_path_table_loc_le;
    uint32_t m_path_table_loc_be;
    uint8_t  root_directory_record[34];
    char     volume_set_identifier[128];
    char     publisher_identifier[128];
    char     data_preparer_identifier[128];
    char     application_identifier[128];
    char     copyright_file_identifier[37];
    char     abstract_file_identifier[37];
    char     bibliographic_file_identifier[37];
    uint8_t  volume_creation_date_time[17];
    uint8_t  volume_modification_date_time[17];
    uint8_t  volume_expiration_date_time[17];
    uint8_t  volume_effective_date_time[17];
    uint8_t  file_structure_version;
    uint8_t  unused3;
    uint8_t  application_use[512];
    uint8_t  unused4[653];
} __attribute__((packed)) iso9660_pvd_t;

typedef struct {
    uint8_t  length;
    uint8_t  ext_attr_length;
    uint32_t extent_loc_le;
    uint32_t extent_loc_be;
    uint32_t data_length_le;
    uint32_t data_length_be;
    uint8_t  recording_date_time[7];
    uint8_t  file_flags;
    uint8_t  file_unit_size;
    uint8_t  interleave_gap_size;
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint8_t  identifier_length;
    char     identifier[1];
} __attribute__((packed)) iso9660_dir_record_t;

typedef struct {
    uint16_t depth;
    uint32_t extent;
    uint32_t parent_extent;
    char     name[256];
} iso9660_path_entry_t;

typedef struct {
    void*              device;
    iso9660_pvd_t      pvd;
    uint32_t           root_extent;
    uint32_t           root_size;
    uint32_t           block_size;
    uint32_t           total_blocks;
    int                has_joliet;
    int                has_rock_ridge;
    spinlock_t         lock;
} iso9660_fs_t;

int  iso9660_mount(iso9660_fs_t* fs, void* device);
int  iso9660_umount(iso9660_fs_t* fs);
int  iso9660_read_dir(iso9660_fs_t* fs, uint32_t extent, uint32_t size,
                       int (*callback)(const char* name, uint32_t extent, uint32_t size,
                                       uint8_t flags, void* ctx), void* ctx);
int  iso9660_lookup(iso9660_fs_t* fs, uint32_t dir_extent, uint32_t dir_size,
                     const char* name, uint32_t* out_extent, uint32_t* out_size, uint8_t* out_flags);
int  iso9660_read_file(iso9660_fs_t* fs, uint32_t extent, uint64_t offset,
                        void* buf, uint32_t size);
int  iso9660_read_sector(iso9660_fs_t* fs, uint32_t sector, void* buf);
int  iso9660_path_lookup(iso9660_fs_t* fs, const char* path,
                           uint32_t* out_extent, uint32_t* out_size, uint8_t* out_flags);

#endif