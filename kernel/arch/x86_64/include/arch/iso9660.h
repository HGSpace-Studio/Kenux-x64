#ifndef ARCH_X86_64_ISO9660_H
#define ARCH_X86_64_ISO9660_H

#include <arch/types.h>
#include <arch/fs.h>

#define ISO9660_PRIMARY_VOLUME   0x01
#define ISO9660_SUPPLEMENTARY    0x02
#define ISO9660_VOLUME_PARTITION 0x03
#define ISO9660_VOLUME_TERMINATOR 0xFF

#define ISO9660_STD_IDENTIFIER   "CD001"
#define ISO9660_SECTOR_SIZE      2048
#define ISO9660_MAX_NAME_LEN    37
#define ISO9660_MAX_PATH_DEPTH  8

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
    uint8_t  unused1;
    char     system_identifier[32];
    char     volume_identifier[32];
    uint8_t  unused2[8];
    uint32_t volume_space_size_le;
    uint32_t volume_space_size_be;
    uint8_t  unused3[32];
    uint16_t volume_set_size_le;
    uint16_t volume_set_size_be;
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint16_t logical_block_size_le;
    uint16_t logical_block_size_be;
    uint32_t path_table_size_le;
    uint32_t path_table_size_be;
    uint32_t location_of_type_l_path_table;
    uint32_t location_of_type_m_path_table;
    uint32_t location_of_type_l_path_table2;
    uint32_t location_of_type_m_path_table2;
    uint8_t  directory_record[34];
    uint8_t  directory_record2[34];
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
    uint8_t  unused4;
    uint8_t  application_use[512];
    uint8_t  unused5[653];
} __attribute__((packed)) iso9660_pvd_t;

typedef struct {
    uint8_t  length;
    uint8_t  extended_attribute_length;
    uint32_t extent_location_le;
    uint32_t extent_location_be;
    uint32_t data_length_le;
    uint32_t data_length_be;
    uint8_t  recording_date_time[7];
    uint8_t  file_flags;
    uint8_t  file_unit_size;
    uint8_t  interleave_gap_size;
    uint16_t volume_sequence_number_le;
    uint16_t volume_sequence_number_be;
    uint8_t  file_identifier_length;
    char     file_identifier[1];
} __attribute__((packed)) iso9660_dir_record_t;

typedef struct {
    iso9660_pvd_t pvd;
    uint32_t      root_extent;
    uint32_t      root_size;
    uint16_t      block_size;
    uint32_t      total_blocks;
    void*         device;
    int           mounted;
    spinlock_t    lock;
} iso9660_fs_t;

typedef struct {
    char     name[ISO9660_MAX_NAME_LEN + 1];
    uint32_t extent;
    uint32_t size;
    uint8_t  flags;
    uint8_t  name_len;
} iso9660_dirent_t;

void iso9660_init(void);
int  iso9660_mount(vfs_node_t* mount_point, void* device);
int  iso9660_unmount(iso9660_fs_t* fs);
int  iso9660_read_dir(iso9660_fs_t* fs, uint32_t extent, uint32_t size, iso9660_dirent_t* entries, int max);
int  iso9660_read_file(iso9660_fs_t* fs, uint32_t extent, void* buf, uint64_t offset, uint64_t size);
int  iso9660_find_entry(iso9660_fs_t* fs, uint32_t dir_extent, uint32_t dir_size, const char* name, iso9660_dirent_t* out);

#endif