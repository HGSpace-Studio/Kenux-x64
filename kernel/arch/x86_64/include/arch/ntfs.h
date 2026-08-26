#ifndef ARCH_X86_64_NTFS_H
#define ARCH_X86_64_NTFS_H

#include <arch/types.h>
#include <arch/fs.h>

#define NTFS_OEM_ID               "NTFS    "
#define NTFS_BOOT_SECTOR_SIZE     512
#define NTFS_FILE_RECORD_MAGIC    0x454C4946
#define NTFS_INDEX_ROOT_MAGIC     0x58444E49
#define NTFS_INDEX_ALLOC_MAGIC    0x494E4458

#define NTFS_ATTRIBUTE_STANDARD_INFORMATION   0x00000010
#define NTFS_ATTRIBUTE_ATTRIBUTE_LIST         0x00000020
#define NTFS_ATTRIBUTE_FILE_NAME              0x00000030
#define NTFS_ATTRIBUTE_VOLUME_NAME            0x00000060
#define NTFS_ATTRIBUTE_VOLUME_INFORMATION     0x00000070
#define NTFS_ATTRIBUTE_DATA                   0x00000080
#define NTFS_ATTRIBUTE_INDEX_ROOT             0x00000090
#define NTFS_ATTRIBUTE_INDEX_ALLOCATION       0x000000A0
#define NTFS_ATTRIBUTE_BITMAP                 0x000000B0
#define NTFS_ATTRIBUTE_END                    0xFFFFFFFF

#define NTFS_ATTR_NONRESIDENT     0x0001
#define NTFS_ATTR_COMPRESSED      0x00FF
#define NTFS_ATTR_ENCRYPTED       0x4000
#define NTFS_ATTR_SPARSE          0x8000

#define NTFS_FILE_ATTR_READONLY              0x00000001
#define NTFS_FILE_ATTR_HIDDEN                0x00000002
#define NTFS_FILE_ATTR_SYSTEM                0x00000004
#define NTFS_FILE_ATTR_ARCHIVE               0x00000020
#define NTFS_FILE_ATTR_DEVICE                0x00000040
#define NTFS_FILE_ATTR_NORMAL                0x00000080
#define NTFS_FILE_ATTR_TEMPORARY             0x00000100
#define NTFS_FILE_ATTR_SPARSE_FILE           0x00000200
#define NTFS_FILE_ATTR_REPARSE_POINT         0x00000400
#define NTFS_FILE_ATTR_COMPRESSED            0x00000800
#define NTFS_FILE_ATTR_OFFLINE               0x00001000
#define NTFS_FILE_ATTR_NOT_CONTENT_INDEXED   0x00002000
#define NTFS_FILE_ATTR_ENCRYPTED             0x00004000
#define NTFS_FILE_ATTR_DIRECTORY             0x10000000
#define NTFS_FILE_ATTR_INDEX_VIEW            0x20000000

#define NTFS_MFT_RECORD_IN_USE     0x0001
#define NTFS_MFT_RECORD_DIRECTORY  0x0002

#define NTFS_NAMESPACE_POSIX       0
#define NTFS_NAMESPACE_WIN32       1
#define NTFS_NAMESPACE_DOS         2
#define NTFS_NAMESPACE_WIN32_DOS   3

#define NTFS_INDEX_ENTRY_HAS_SUBNODE   0x0001
#define NTFS_INDEX_ENTRY_LAST          0x0002

#define NTFS_VOLUME_FLAG_DIRTY         0x0001
#define NTFS_VOLUME_FLAG_RESIZE_LOG    0x0002
#define NTFS_VOLUME_FLAG_UPGRADE_ON_MOUNT 0x0004
#define NTFS_VOLUME_FLAG_MOUNTED_NT4   0x0008
#define NTFS_VOLUME_FLAG_DELETE_USN_UNDERWAY 0x0010
#define NTFS_VOLUME_FLAG_REPAIR_OBJECT_ID 0x0020
#define NTFS_VOLUME_FLAG_MODIFIED_BY_CHKDSK 0x8000

#define NTFS_MAX_FILENAME_LEN      255
#define NTFS_MAX_PATH_LEN          32767

typedef struct {
    char      name[FS_MAX_NAME];
    uint64_t  inode;
    uint64_t  type;
    uint64_t  size;
} vfs_dirent_t;

typedef struct {
    uint8_t   jump_boot[3];
    uint8_t   oem_id[8];
    uint16_t  bytes_per_sector;
    uint8_t   sectors_per_cluster;
    uint16_t  reserved_sectors;
    uint8_t   unused1[5];
    uint8_t   media_descriptor;
    uint16_t  unused2;
    uint16_t  sectors_per_track;
    uint16_t  number_of_heads;
    uint32_t  hidden_sectors;
    uint32_t  unused3[2];
    uint64_t  total_sectors;
    uint64_t  mft_cluster;
    uint64_t  mft_mirror_cluster;
    int8_t    bytes_per_file_record;
    uint8_t   unused4[3];
    int8_t    clusters_per_index_buffer;
    uint8_t   unused5[3];
    uint64_t  volume_serial_number;
    uint32_t  checksum;
    uint8_t   boot_strap[426];
    uint16_t  signature;
} __attribute__((packed)) NTFS_BOOT_SECTOR;

typedef struct {
    uint32_t  magic;
    uint16_t  update_sequence_offset;
    uint16_t  update_sequence_size;
    uint64_t  log_file_sequence_number;
    uint16_t  sequence_number;
    uint16_t  hard_link_count;
    uint16_t  attribute_offset;
    uint16_t  flags;
    uint32_t  bytes_in_use;
    uint32_t  bytes_allocated;
    uint64_t  base_record_reference;
    uint16_t  next_attribute_id;
    uint16_t  align_to_8_byte_boundary;
    uint32_t  mft_record_number;
} __attribute__((packed)) NTFS_FILE_RECORD_HEADER;

typedef struct {
    uint32_t  type;
    uint32_t  length;
    uint8_t   non_resident;
    uint8_t   name_length;
    uint16_t  name_offset;
    uint16_t  flags;
    uint16_t  attribute_id;
    union {
        struct {
            uint32_t  value_length;
            uint16_t  value_offset;
            uint8_t   resident_flags;
            uint8_t   reserved;
        } resident;
        struct {
            uint64_t  low_vcn;
            uint64_t  high_vcn;
            uint16_t  data_run_offset;
            uint16_t  compression_unit;
            uint32_t  reserved;
            uint64_t  allocated_size;
            uint64_t  data_size;
            uint64_t  initialized_size;
            uint64_t  compressed_size;
        } non_resident;
    } data;
} __attribute__((packed)) NTFS_ATTRIBUTE_HEADER;

typedef struct {
    uint64_t  creation_time;
    uint64_t  last_modification_time;
    uint64_t  last_mft_modification_time;
    uint64_t  last_access_time;
    uint32_t  file_attributes;
    uint32_t  max_versions;
    uint32_t  version_number;
    uint32_t  class_id;
    uint32_t  owner_id;
    uint32_t  security_id;
    uint64_t  quota_charged;
    uint64_t  usn;
} __attribute__((packed)) NTFS_STANDARD_INFORMATION;

typedef struct {
    uint64_t  parent_directory_reference;
    uint64_t  creation_time;
    uint64_t  last_modification_time;
    uint64_t  last_mft_modification_time;
    uint64_t  last_access_time;
    uint64_t  allocated_size;
    uint64_t  data_size;
    uint32_t  file_attributes;
    uint32_t  reparse_point_tag;
    uint8_t   file_name_length;
    uint8_t   file_namespace;
    uint16_t  file_name[NTFS_MAX_FILENAME_LEN];
} __attribute__((packed)) NTFS_FILE_NAME;

typedef struct {
    uint32_t  entries_offset;
    uint32_t  index_entries_size;
    uint32_t  allocated_size;
    uint8_t   leaf_flag;
    uint8_t   reserved[3];
} __attribute__((packed)) NTFS_INDEX_HEADER;

typedef struct {
    uint64_t  mft_record_reference;
    uint16_t  index_entry_length;
    uint16_t  filename_length;
    uint16_t  flags;
    uint8_t   data[];
} __attribute__((packed)) NTFS_INDEX_ENTRY;

typedef struct {
    uint64_t  reserved;
    uint8_t   major_version;
    uint8_t   minor_version;
    uint16_t  flags;
    uint32_t  volume_id;
} __attribute__((packed)) NTFS_VOLUME_INFORMATION;

typedef struct {
    NTFS_BOOT_SECTOR* boot_sector;
    uint64_t  cluster_size;
    uint64_t  mft_offset;
    uint64_t  mft_record_size;
    uint64_t  index_buffer_size;
    uint64_t  total_clusters;
    uint8_t*  mft_buffer;
    uint32_t  block_size;
    vfs_node_t* root_node;
    int       mounted;
    char      device_name[64];
} ntfs_fs_t;

int ntfs_init(void);
int ntfs_mount(const char* device);
vfs_node_t* ntfs_lookup(vfs_node_t* parent, const char* name);
int ntfs_readdir(vfs_node_t* dir, vfs_dirent_t* out, uint32_t count);
int ntfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, uint8_t* buf);

#endif
