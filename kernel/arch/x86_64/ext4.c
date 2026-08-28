#include <arch/ext4.h>
#include <arch/ext2.h>
#include <arch/memory.h>
#include <string.h>

static uint32_t ext4_crc32c_table[256];
static int ext4_crc32c_initialized = 0;

static void ext4_crc32c_init(void)
{
    if (ext4_crc32c_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0x82F63B78;
            else crc >>= 1;
        }
        ext4_crc32c_table[i] = crc;
    }
    ext4_crc32c_initialized = 1;
}

uint32_t ext4_crc32c(uint32_t crc, const void* buf, uint32_t size)
{
    ext4_crc32c_init();
    const uint8_t* p = (const uint8_t*)buf;
    crc = ~crc;
    for (uint32_t i = 0; i < size; i++) {
        crc = ext4_crc32c_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

uint16_t ext4_group_desc_csum(ext4_fs_t* fs, uint32_t group)
{
    if (!fs) return 0;
    if (fs->has_metadata_csum) {
        uint32_t crc = ext4_crc32c(fs->csum_seed, &group, sizeof(group));
        return (uint16_t)(crc & 0xFFFF);
    }
    return 0;
}

int ext4_mount(vfs_node_t* mount_point, void* device)
{
    if (!mount_point || !device) return -1;

    ext4_fs_t* fs = (ext4_fs_t*)memory_alloc(sizeof(ext4_fs_t));
    if (!fs) return -2;
    memset(fs, 0, sizeof(ext4_fs_t));

    int result = ext3_mount(mount_point, device);
    if (result != 0) {
        memory_free(fs);
        return -3;
    }

    ext2_superblock_t* sb = &fs->base.superblock;

    fs->feature_compat = sb->feature_compat;
    fs->feature_ro_compat = sb->feature_ro_compat;
    fs->feature_incompat = sb->feature_incompat;

    fs->has_extents = (fs->feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) ? 1 : 0;
    fs->has_64bit = (fs->feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) ? 1 : 0;
    fs->has_flex_bg = (fs->feature_incompat & EXT4_FEATURE_INCOMPAT_FLEX_BG) ? 1 : 0;
    fs->has_huge_file = (fs->feature_ro_compat & EXT4_FEATURE_RO_COMPAT_HUGE_FILE) ? 1 : 0;
    fs->has_metadata_csum = (fs->feature_ro_compat & EXT4_FEATURE_RO_COMPAT_METADATA_CSUM) ? 1 : 0;

    fs->inode_size = sb->inode_size;
    fs->first_data_block = sb->first_data_block;

    if (fs->has_64bit) {
        fs->desc_size = 64;
    } else {
        fs->desc_size = 32;
    }

    if (fs->has_metadata_csum) {
        fs->csum_seed = 0;
    }

    fs->flex_bg_size = 1;

    fs->s_reserved_gdt_blocks = 0;

    return 0;
}

int ext4_umount(ext4_fs_t* fs)
{
    if (!fs) return -1;
    return 0;
}

int ext4_read_inode(ext4_fs_t* fs, uint32_t ino, void* buf)
{
    if (!fs || ino == 0 || !buf) return -1;
    ext2_inode_t* inode = ext3_get_inode(&fs->base, ino);
    if (!inode) return -1;
    memcpy(buf, inode, sizeof(ext2_inode_t));
    return 0;
}

int ext4_write_inode(ext4_fs_t* fs, uint32_t ino, const void* buf)
{
    if (!fs || ino == 0 || !buf) return -1;
    ext2_inode_t* inode = ext3_get_inode(&fs->base, ino);
    if (!inode) return -1;
    memcpy(inode, buf, sizeof(ext2_inode_t));
    return 0;
}

int ext4_extent_map(ext4_fs_t* fs, uint32_t ino, uint64_t logical_block,
                     uint64_t* physical_block, uint32_t* length)
{
    if (!fs || ino == 0 || !physical_block || !length) return -1;

    if (!fs->has_extents) {
        *physical_block = logical_block;
        *length = 1;
        return 0;
    }

    uint8_t inode_buf[256];
    int result = ext4_read_inode(fs, ino, inode_buf);
    if (result != 0) return result;

    ext4_extent_header_t* header = (ext4_extent_header_t*)(inode_buf + 0x28);
    if (header->eh_magic != EXT4_EXT_MAGIC) {
        *physical_block = logical_block;
        *length = 1;
        return 0;
    }

    if (header->eh_depth == 0) {
        ext4_extent_t* ext = (ext4_extent_t*)(header + 1);
        for (uint16_t i = 0; i < header->eh_entries; i++) {
            uint32_t ext_len = ext[i].ee_len;
            if (ext_len > EXT4_EXTENT_INIT_MAX_LEN) ext_len -= EXT4_EXTENT_INIT_MAX_LEN;
            if (logical_block >= ext[i].ee_block &&
                logical_block < ext[i].ee_block + ext_len) {
                uint64_t start = ((uint64_t)ext[i].ee_start_hi << 32) | ext[i].ee_start_lo;
                *physical_block = start + (logical_block - ext[i].ee_block);
                *length = ext_len - (uint32_t)(logical_block - ext[i].ee_block);
                return 0;
            }
        }
    } else {
        ext4_extent_idx_t* idx = (ext4_extent_idx_t*)(header + 1);
        for (uint16_t i = 0; i < header->eh_entries; i++) {
            if (logical_block < idx[i].ei_block) {
                if (i == 0) return -2;
                uint64_t child_block = ((uint64_t)idx[i-1].ei_start_hi << 32) | idx[i-1].ei_start_lo;
                uint8_t block_buf[4096];
                ext3_read_block(&fs->base, (uint32_t)child_block, block_buf);
                ext4_extent_header_t* child_hdr = (ext4_extent_header_t*)block_buf;
                if (child_hdr->eh_magic != EXT4_EXT_MAGIC) return -3;
                ext4_extent_t* child_ext = (ext4_extent_t*)(child_hdr + 1);
                for (uint16_t j = 0; j < child_hdr->eh_entries; j++) {
                    uint32_t ext_len = child_ext[j].ee_len;
                    if (ext_len > EXT4_EXTENT_INIT_MAX_LEN) ext_len -= EXT4_EXTENT_INIT_MAX_LEN;
                    if (logical_block >= child_ext[j].ee_block &&
                        logical_block < child_ext[j].ee_block + ext_len) {
                        uint64_t start = ((uint64_t)child_ext[j].ee_start_hi << 32) | child_ext[j].ee_start_lo;
                        *physical_block = start + (logical_block - child_ext[j].ee_block);
                        *length = ext_len - (uint32_t)(logical_block - child_ext[j].ee_block);
                        return 0;
                    }
                }
                return -4;
            }
        }
        if (header->eh_entries > 0) {
            uint16_t last = header->eh_entries - 1;
            uint64_t child_block = ((uint64_t)idx[last].ei_start_hi << 32) | idx[last].ei_start_lo;
            uint8_t block_buf[4096];
            ext3_read_block(&fs->base, (uint32_t)child_block, block_buf);
            ext4_extent_header_t* child_hdr = (ext4_extent_header_t*)block_buf;
            if (child_hdr->eh_magic != EXT4_EXT_MAGIC) return -3;
            ext4_extent_t* child_ext = (ext4_extent_t*)(child_hdr + 1);
            for (uint16_t j = 0; j < child_hdr->eh_entries; j++) {
                uint32_t ext_len = child_ext[j].ee_len;
                if (ext_len > EXT4_EXTENT_INIT_MAX_LEN) ext_len -= EXT4_EXTENT_INIT_MAX_LEN;
                if (logical_block >= child_ext[j].ee_block &&
                    logical_block < child_ext[j].ee_block + ext_len) {
                    uint64_t start = ((uint64_t)child_ext[j].ee_start_hi << 32) | child_ext[j].ee_start_lo;
                    *physical_block = start + (logical_block - child_ext[j].ee_block);
                    *length = ext_len - (uint32_t)(logical_block - child_ext[j].ee_block);
                    return 0;
                }
            }
        }
    }

    return -5;
}

int ext4_read_file(ext4_fs_t* fs, uint32_t ino, void* buf, uint64_t offset, uint64_t size)
{
    if (!fs || ino == 0 || !buf) return -1;

    if (fs->has_extents) {
        uint32_t block_size = fs->base.block_size;
        uint8_t* dst = (uint8_t*)buf;
        uint64_t remaining = size;
        uint64_t file_offset = offset;

        while (remaining > 0) {
            uint64_t logical_block = file_offset / block_size;
            uint32_t block_offset = (uint32_t)(file_offset % block_size);
            uint64_t physical_block;
            uint32_t extent_length;

            int result = ext4_extent_map(fs, ino, logical_block, &physical_block, &extent_length);
            if (result != 0) return result;

            uint8_t block_buf[4096];
            ext3_read_block(&fs->base, (uint32_t)physical_block, block_buf);

            uint32_t to_copy = block_size - block_offset;
            if (to_copy > remaining) to_copy = (uint32_t)remaining;

            memcpy(dst, block_buf + block_offset, to_copy);
            dst += to_copy;
            file_offset += to_copy;
            remaining -= to_copy;
        }

        return (int)size;
    }

    ext2_inode_t* inode = ext3_get_inode(&fs->base, ino);
    if (!inode) return -1;
    return ext3_read_inode_data(&fs->base, inode, offset, buf, size);
}

int ext4_write_file(ext4_fs_t* fs, uint32_t ino, const void* buf, uint64_t offset, uint64_t size)
{
    if (!fs || ino == 0 || !buf) return -1;
    ext2_inode_t* inode = ext3_get_inode(&fs->base, ino);
    if (!inode) return -1;
    return ext3_write_inode_data(&fs->base, inode, offset, buf, size);
}

int ext4_truncate(ext4_fs_t* fs, uint32_t ino, uint64_t new_size)
{
    if (!fs || ino == 0) return -1;
    (void)new_size;
    return 0;
}

int ext4_lookup(ext4_fs_t* fs, uint32_t dir_ino, const char* name, uint32_t* out_ino)
{
    if (!fs || dir_ino == 0 || !name || !out_ino) return -1;
    (void)fs; (void)dir_ino; (void)name; (void)out_ino;
    return -1;
}

int ext4_create(ext4_fs_t* fs, uint32_t dir_ino, const char* name, uint32_t mode, uint32_t* out_ino)
{
    if (!fs || dir_ino == 0 || !name || !out_ino) return -1;
    (void)mode;
    *out_ino = ext3_alloc_inode(&fs->base);
    return *out_ino ? 0 : -1;
}

int ext4_mkdir(ext4_fs_t* fs, uint32_t dir_ino, const char* name, uint32_t mode)
{
    if (!fs || dir_ino == 0 || !name) return -1;
    (void)dir_ino; (void)name; (void)mode;
    return -1;
}

int ext4_unlink(ext4_fs_t* fs, uint32_t dir_ino, const char* name)
{
    if (!fs || dir_ino == 0 || !name) return -1;
    (void)dir_ino; (void)name;
    return -1;
}

int ext4_symlink(ext4_fs_t* fs, uint32_t dir_ino, const char* name, const char* target)
{
    if (!fs || dir_ino == 0 || !name || !target) return -1;
    (void)target;
    return -1;
}

int ext4_rename(ext4_fs_t* fs, uint32_t old_dir, const char* old_name,
                 uint32_t new_dir, const char* new_name)
{
    if (!fs || old_dir == 0 || !old_name || new_dir == 0 || !new_name) return -1;
    (void)old_dir; (void)old_name; (void)new_dir; (void)new_name;
    return -1;
}

uint32_t ext4_alloc_inode(ext4_fs_t* fs, int is_dir)
{
    if (!fs) return 0;
    (void)is_dir;
    return ext3_alloc_inode(&fs->base);
}

void ext4_free_inode(ext4_fs_t* fs, uint32_t ino)
{
    if (!fs || ino == 0) return;
    ext3_free_inode(&fs->base, ino);
}

uint64_t ext4_alloc_block(ext4_fs_t* fs)
{
    if (!fs) return 0;
    return (uint64_t)ext3_alloc_block(&fs->base);
}

void ext4_free_block(ext4_fs_t* fs, uint64_t block)
{
    if (!fs || block == 0) return;
    ext3_free_block(&fs->base, (uint32_t)block);
}

int ext4_extent_insert(ext4_fs_t* fs, uint32_t ino, uint64_t logical_block,
                         uint64_t physical_block, uint32_t length)
{
    if (!fs || ino == 0) return -1;
    (void)logical_block; (void)physical_block; (void)length;
    return 0;
}