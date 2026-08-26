#include <arch/ext4.h>
#include <arch/memory.h>
#include <arch/slab.h>
#include <arch/ahci.h>
#include <string.h>

#define EXT4_MAX_FS 16

static ext4_fs_t ext4_fs_table[EXT4_MAX_FS];
static uint32_t ext4_fs_count = 0;
static spinlock_t ext4_global_lock = SPINLOCK_INIT;

static inline uint64_t ext4_get_blocks_count(const ext4_superblock_t* sb)
{
    return ((uint64_t)sb->s_blocks_count_hi << 32) | sb->s_blocks_count_lo;
}

static inline uint64_t ext4_inode_table_block(const ext4_fs_t* fs, uint32_t ino,
                                               uint32_t* offset_in_block)
{
    uint32_t group = (ino - 1) / fs->inodes_per_group;
    uint32_t idx = (ino - 1) % fs->inodes_per_group;
    if (group >= fs->group_count) return 0;
    ext4_group_desc_t* gd = &fs->gdt[group];
    uint64_t table = ((uint64_t)gd->bg_inode_table_hi << 32) | gd->bg_inode_table_lo;
    *offset_in_block = (idx * fs->inode_size) % fs->block_size;
    return table + (idx * fs->inode_size) / fs->block_size;
}

static inline uint64_t ext4_inode_i_blocks(const ext4_inode_t* inode)
{
    return ((uint64_t)inode->i_blocks_high << 32) | inode->i_blocks_lo;
}

static int ext4_read_disk_lba(uint64_t lba, void* buf, uint32_t count)
{
    if (!buf || count == 0) return -1;
    uint8_t* dst = (uint8_t*)buf;
    for (uint32_t i = 0; i < count; i++) {
        if (ahci_read_sector(0, lba + i, dst + i * 512) != 0) {
            return -1;
        }
    }
    return 0;
}

static int ext4_write_disk_lba(uint64_t lba, const void* buf, uint32_t count)
{
    if (!buf || count == 0) return -1;
    const uint8_t* src = (const uint8_t*)buf;
    for (uint32_t i = 0; i < count; i++) {
        if (ahci_write_sector(0, lba + i, src + i * 512) != 0) {
            return -1;
        }
    }
    return 0;
}

static int ext4_read_block_raw(ext4_fs_t* fs, uint64_t block, void* buf)
{
    uint64_t lba = block * (fs->block_size / 512);
    uint32_t sectors = fs->block_size / 512;
    return ext4_read_disk_lba(lba, buf, sectors);
}

static int ext4_write_block_raw(ext4_fs_t* fs, uint64_t block, const void* buf)
{
    uint64_t lba = block * (fs->block_size / 512);
    uint32_t sectors = fs->block_size / 512;
    return ext4_write_disk_lba(lba, buf, sectors);
}

void ext4_init(void)
{
    spin_init(&ext4_global_lock);
    ext4_fs_count = 0;
    for (uint32_t i = 0; i < EXT4_MAX_FS; i++) {
        ext4_fs_table[i].mounted = 0;
        ext4_fs_table[i].root = NULL;
        ext4_fs_table[i].gdt = NULL;
        ext4_fs_table[i].journal = NULL;
        ext4_fs_table[i].block_bitmap = NULL;
        ext4_fs_table[i].inode_bitmap = NULL;
        spin_init(&ext4_fs_table[i].lock);
    }
}

int ext4_mount(const char* device, vfs_node_t** root_out)
{
    if (!device) return -1;
    spin_lock(&ext4_global_lock);
    if (ext4_fs_count >= EXT4_MAX_FS) {
        spin_unlock(&ext4_global_lock);
        return -1;
    }
    ext4_fs_t* fs = NULL;
    for (uint32_t i = 0; i < EXT4_MAX_FS; i++) {
        if (!ext4_fs_table[i].mounted) {
            fs = &ext4_fs_table[i];
            break;
        }
    }
    if (!fs) {
        spin_unlock(&ext4_global_lock);
        return -1;
    }
    strncpy(fs->device, device, 31);
    fs->device[31] = 0;
    uint8_t sb_buf[1024];
    memset(sb_buf, 0, sizeof(sb_buf));
    if (ext4_read_disk_lba(2, sb_buf, 2) != 0) {
        spin_unlock(&ext4_global_lock);
        return -1;
    }
    memcpy(&fs->sb, sb_buf, sizeof(ext4_superblock_t));
    if (fs->sb.s_magic != EXT4_SUPER_MAGIC) {
        spin_unlock(&ext4_global_lock);
        return -1;
    }
    fs->block_size = EXT4_BLOCK_SIZE(&fs->sb);
    fs->inode_size = fs->sb.s_inode_size;
    fs->inodes_per_group = fs->sb.s_inodes_per_group;
    fs->blocks_per_group = fs->sb.s_blocks_per_group;
    fs->first_data_block = fs->sb.s_first_data_block;
    fs->feature_incompat = fs->sb.s_feature_incompat;
    fs->feature_compat = fs->sb.s_feature_compat;
    fs->feature_ro_compat = fs->sb.s_feature_ro_compat;
    fs->blocks_count = ext4_get_blocks_count(&fs->sb);
    if (fs->feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
        fs->desc_size = fs->sb.s_desc_size;
        if (fs->desc_size < sizeof(ext4_group_desc_t))
            fs->desc_size = sizeof(ext4_group_desc_t);
    } else {
        fs->desc_size = 32;
    }
    uint32_t gdt_block = fs->first_data_block + 1;
    uint32_t gdt_size_blocks = 1;
    uint64_t total_inodes = (uint64_t)fs->sb.s_inodes_count;
    uint32_t groups_needed = (uint32_t)((total_inodes + fs->inodes_per_group - 1) /
                                       fs->inodes_per_group);
    uint64_t gdt_bytes = (uint64_t)groups_needed * fs->desc_size;
    uint64_t blocks_per_group64 = (uint64_t)fs->blocks_per_group;
    if (blocks_per_group64 == 0) blocks_per_group64 = 1;
    uint64_t total_blocks64 = fs->blocks_count;
    uint64_t groups_by_blocks = (total_blocks64 + blocks_per_group64 - 1) / blocks_per_group64;
    uint32_t group_count = (uint32_t)(groups_by_blocks > groups_needed ?
                                      groups_by_blocks : groups_needed);
    fs->group_count = group_count;
    uint64_t gdt_total_bytes = (uint64_t)group_count * fs->desc_size;
    gdt_size_blocks = (uint32_t)((gdt_total_bytes + fs->block_size - 1) / fs->block_size);
    fs->gdt = (ext4_group_desc_t*)kmalloc(gdt_total_bytes);
    if (!fs->gdt) {
        spin_unlock(&ext4_global_lock);
        return -1;
    }
    uint8_t* gdt_buf = (uint8_t*)kmalloc(fs->block_size * gdt_size_blocks);
    if (!gdt_buf) {
        kfree(fs->gdt);
        fs->gdt = NULL;
        spin_unlock(&ext4_global_lock);
        return -1;
    }
    for (uint32_t b = 0; b < gdt_size_blocks; b++) {
        if (ext4_read_block_raw(fs, gdt_block + b, gdt_buf + b * fs->block_size) != 0) {
            kfree(gdt_buf);
            kfree(fs->gdt);
            fs->gdt = NULL;
            spin_unlock(&ext4_global_lock);
            return -1;
        }
    }
    for (uint32_t g = 0; g < group_count; g++) {
        uint64_t offset = (uint64_t)g * fs->desc_size;
        uint64_t block_offset = offset / fs->block_size;
        uint64_t in_block_offset = offset % fs->block_size;
        uint8_t* src = gdt_buf + block_offset * fs->block_size + in_block_offset;
        memcpy(&fs->gdt[g], src, fs->desc_size);
    }
    kfree(gdt_buf);
    if (fs->feature_compat & EXT4_FEATURE_COMPAT_HAS_JOURNAL) {
        if (fs->sb.s_journal_inum != 0) {
            ext4_inode_t journal_inode;
            if (ext4_read_inode(fs, fs->sb.s_journal_inum, &journal_inode) == 0) {
                uint64_t journal_start = ((uint64_t)journal_inode.i_block[0]);
                uint64_t journal_blocks = ext4_inode_size64(&journal_inode) / fs->block_size;
                fs->journal = (jbd2_journal_t*)kmalloc(sizeof(jbd2_journal_t));
                if (fs->journal) {
                    jbd2_journal_init(fs->block_size, journal_start, journal_blocks);
                }
            }
        }
    }
    fs->root = vfs_create_node("/", FS_TYPE_DIRECTORY);
    if (!fs->root) {
        kfree(fs->gdt);
        fs->gdt = NULL;
        spin_unlock(&ext4_global_lock);
        return -1;
    }
    fs->root->inode = 2;
    fs->root->impl_data = fs;
    fs->root->blksize = fs->block_size;
    fs->mounted = 1;
    ext4_fs_count++;
    spin_unlock(&ext4_global_lock);
    if (root_out) *root_out = fs->root;
    if (fs->feature_compat & EXT4_FEATURE_COMPAT_HAS_JOURNAL && fs->journal) {
        ext4_replay_journal(fs);
    }
    return 0;
}

int ext4_unmount(ext4_fs_t* fs)
{
    if (!fs || !fs->mounted) return -1;
    spin_lock(&ext4_global_lock);
    if (fs->journal) {
        jbd2_journal_flush();
        jbd2_journal_destroy();
        kfree(fs->journal);
        fs->journal = NULL;
    }
    if (fs->gdt) {
        kfree(fs->gdt);
        fs->gdt = NULL;
    }
    if (fs->block_bitmap) {
        kfree(fs->block_bitmap);
        fs->block_bitmap = NULL;
    }
    if (fs->inode_bitmap) {
        kfree(fs->inode_bitmap);
        fs->inode_bitmap = NULL;
    }
    fs->root = NULL;
    fs->mounted = 0;
    ext4_fs_count--;
    spin_unlock(&ext4_global_lock);
    return 0;
}

int ext4_read_inode(ext4_fs_t* fs, uint32_t ino, ext4_inode_t* inode)
{
    if (!fs || !inode || ino == 0 || !fs->mounted) return -1;
    uint32_t offset_in_block = 0;
    uint64_t table_block = ext4_inode_table_block(fs, ino, &offset_in_block);
    if (table_block == 0) return -1;
    uint8_t* block_buf = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buf) return -1;
    if (ext4_read_block_raw(fs, table_block, block_buf) != 0) {
        kfree(block_buf);
        return -1;
    }
    if (offset_in_block + fs->inode_size > fs->block_size) {
        memcpy(inode, block_buf + offset_in_block, fs->block_size - offset_in_block);
        uint64_t next_block = table_block + 1;
        if (ext4_read_block_raw(fs, next_block, block_buf) != 0) {
            kfree(block_buf);
            return -1;
        }
        memcpy((uint8_t*)inode + (fs->block_size - offset_in_block),
               block_buf, fs->inode_size - (fs->block_size - offset_in_block));
    } else {
        memcpy(inode, block_buf + offset_in_block, fs->inode_size);
    }
    kfree(block_buf);
    return 0;
}

int ext4_read_block(ext4_fs_t* fs, uint64_t block, void* buf)
{
    if (!fs || !buf || !fs->mounted) return -1;
    return ext4_read_block_raw(fs, block, buf);
}

int ext4_write_block(ext4_fs_t* fs, uint64_t block, const void* buf)
{
    if (!fs || !buf || !fs->mounted) return -1;
    return ext4_write_block_raw(fs, block, buf);
}

static int ext4_read_indirect_block(ext4_fs_t* fs, uint64_t ind_block,
                                     uint32_t index, uint64_t* phys_block)
{
    uint8_t* buf = (uint8_t*)kmalloc(fs->block_size);
    if (!buf) return -1;
    if (ext4_read_block_raw(fs, ind_block, buf) != 0) {
        kfree(buf);
        return -1;
    }
    uint32_t entries_per_block = fs->block_size / 4;
    if (index < entries_per_block) {
        uint32_t blk = ((uint32_t*)buf)[index];
        if (blk != 0) {
            *phys_block = blk;
            kfree(buf);
            return 0;
        }
    }
    kfree(buf);
    return -1;
}

static int ext4_map_block_indirect(ext4_fs_t* fs, ext4_inode_t* inode,
                                    uint64_t lblock, uint64_t* phys_block)
{
    uint32_t block_per_indirect = fs->block_size / 4;
    uint32_t total_direct = EXT4_NDIR_BLOCKS;
    if (lblock < total_direct) {
        *phys_block = inode->i_block[lblock];
        return (*phys_block != 0) ? 0 : -1;
    }
    uint32_t rel = (uint32_t)(lblock - total_direct);
    if (rel < block_per_indirect) {
        uint64_t ind_block = inode->i_block[EXT4_IND_BLOCK];
        if (ind_block == 0) return -1;
        return ext4_read_indirect_block(fs, ind_block, rel, phys_block);
    }
    uint32_t rel2 = rel - block_per_indirect;
    uint64_t dind_entries = (uint64_t)block_per_indirect * block_per_indirect;
    if (rel2 < dind_entries) {
        uint64_t dind_block = inode->i_block[EXT4_DIND_BLOCK];
        if (dind_block == 0) return -1;
        uint32_t dind_idx = (uint32_t)(rel2 / block_per_indirect);
        uint64_t ind_block_num;
        if (ext4_read_indirect_block(fs, dind_block, dind_idx, &ind_block_num) != 0)
            return -1;
        uint32_t ind_idx = (uint32_t)(rel2 % block_per_indirect);
        return ext4_read_indirect_block(fs, ind_block_num, ind_idx, phys_block);
    }
    uint32_t rel3 = rel2 - (uint32_t)dind_entries;
    uint64_t tind_entries = (uint64_t)block_per_indirect * dind_entries;
    if (rel3 < tind_entries) {
        uint64_t tind_block = inode->i_block[EXT4_TIND_BLOCK];
        if (tind_block == 0) return -1;
        uint32_t tind_idx = (uint32_t)(rel3 / dind_entries);
        uint64_t dind_block_num;
        if (ext4_read_indirect_block(fs, tind_block, tind_idx, &dind_block_num) != 0)
            return -1;
        uint32_t remaining = (uint32_t)(rel3 % dind_entries);
        uint32_t dind_idx2 = (uint32_t)(remaining / block_per_indirect);
        uint64_t ind_block_num2;
        if (ext4_read_indirect_block(fs, dind_block_num, dind_idx2, &ind_block_num2) != 0)
            return -1;
        uint32_t ind_idx2 = (uint32_t)(remaining % block_per_indirect);
        return ext4_read_indirect_block(fs, ind_block_num2, ind_idx2, phys_block);
    }
    return -1;
}

int ext4_read_extent_block(ext4_fs_t* fs, ext4_inode_t* inode, uint64_t lblock,
                            uint64_t* pblock, uint32_t* max_blocks)
{
    if (!fs || !inode || !pblock) return -1;
    if (!(inode->i_flags & EXT4_EXTENTS_FL)) {
        int ret = ext4_map_block_indirect(fs, inode, lblock, pblock);
        if (max_blocks) *max_blocks = (ret == 0) ? 1 : 0;
        return ret;
    }
    ext4_extent_header_t* eh = (ext4_extent_header_t*)inode->i_block;
    if (eh->eh_magic != EXT4_EXT_MAGIC) return -1;
    uint8_t* block_buf = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buf) return -1;
    ext4_extent_header_t* cur_header = eh;
    int depth = cur_header->eh_depth;
    uint64_t path_blocks[EXT4_MAX_DEPTH];
    int path_idx = 0;
    while (depth > 0 && cur_header) {
        if (cur_header->eh_magic != EXT4_EXT_MAGIC) {
            kfree(block_buf);
            return -1;
        }
        int found = 0;
        ext4_extent_idx_t* idx = (ext4_extent_idx_t*)(cur_header + 1);
        for (int i = 0; i < cur_header->eh_entries; i++) {
            if ((uint64_t)idx[i].ei_block <= lblock) {
                if (i + 1 < cur_header->eh_entries &&
                    (uint64_t)idx[i + 1].ei_block <= lblock) {
                    continue;
                }
                uint64_t leaf_block = ((uint64_t)idx[i].ei_leaf_hi << 32) | idx[i].ei_leaf_lo;
                if (ext4_read_block_raw(fs, leaf_block, block_buf) != 0) {
                    kfree(block_buf);
                    return -1;
                }
                path_blocks[path_idx++] = leaf_block;
                cur_header = (ext4_extent_header_t*)block_buf;
                depth--;
                found = 1;
                break;
            }
        }
        if (!found) {
            kfree(block_buf);
            return -1;
        }
    }
    if (cur_header == NULL || cur_header->eh_magic != EXT4_EXT_MAGIC || depth != 0) {
        kfree(block_buf);
        return -1;
    }
    ext4_extent_t* ext = (ext4_extent_t*)(cur_header + 1);
    for (int i = 0; i < cur_header->eh_entries; i++) {
        uint32_t ee_len = ext[i].ee_len;
        uint32_t actual_len = ee_len;
        if (ee_len & EXT4_EXT_ZERO_LEN) {
            actual_len = ee_len - EXT4_EXT_ZERO_LEN;
        }
        if (actual_len == 0) actual_len = 1;
        uint64_t ee_start = ((uint64_t)ext[i].ee_start_hi << 32) | ext[i].ee_start_lo;
        if (actual_len == 0) continue;
        uint64_t ee_end = (uint64_t)ext[i].ee_block + actual_len;
        if (lblock >= (uint64_t)ext[i].ee_block && lblock < ee_end) {
            *pblock = ee_start + (lblock - (uint64_t)ext[i].ee_block);
            if (max_blocks) {
                *max_blocks = (uint32_t)(ee_end - lblock);
            }
            kfree(block_buf);
            return 0;
        }
    }
    kfree(block_buf);
    return -1;
}

int ext4_read_file(ext4_fs_t* fs, ext4_inode_t* inode, void* buf,
                    uint64_t offset, uint64_t len)
{
    if (!fs || !inode || !buf || !fs->mounted) return -1;
    uint64_t file_size = ext4_inode_size64(inode);
    if (offset >= file_size) return 0;
    if (offset + len > file_size) len = file_size - offset;
    uint8_t* block_buf = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buf) return -1;
    uint8_t* dst = (uint8_t*)buf;
    uint64_t remaining = len;
    uint64_t cur_offset = offset;
    while (remaining > 0) {
        uint64_t lblock = cur_offset / fs->block_size;
        uint32_t block_off = (uint32_t)(cur_offset % fs->block_size);
        uint32_t to_read = (uint32_t)(fs->block_size - block_off);
        if (to_read > remaining) to_read = (uint32_t)remaining;
        uint64_t phys_block;
        if (ext4_read_extent_block(fs, inode, lblock, &phys_block, NULL) == 0) {
            if (ext4_read_block_raw(fs, phys_block, block_buf) != 0) {
                memset(dst, 0, to_read);
            } else {
                memcpy(dst, block_buf + block_off, to_read);
            }
        } else {
            memset(dst, 0, to_read);
        }
        dst += to_read;
        cur_offset += to_read;
        remaining -= to_read;
    }
    kfree(block_buf);
    return (int)len;
}

int ext4_dir_iterate(ext4_fs_t* fs, ext4_inode_t* inode,
                     int (*cb)(ext4_dir_entry_t* entry, void* arg), void* arg)
{
    if (!fs || !inode || !cb || !fs->mounted) return -1;
    uint64_t dir_size = ext4_inode_size64(inode);
    uint8_t* block_buf = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buf) return -1;
    uint64_t offset = 0;
    while (offset < dir_size) {
        uint64_t lblock = offset / fs->block_size;
        uint64_t phys_block;
        if (ext4_read_extent_block(fs, inode, lblock, &phys_block, NULL) != 0) {
            break;
        }
        if (ext4_read_block_raw(fs, phys_block, block_buf) != 0) {
            break;
        }
        uint32_t block_offset = (uint32_t)(offset % fs->block_size);
        while (block_offset < fs->block_size && offset < dir_size) {
            ext4_dir_entry_t* entry = (ext4_dir_entry_t*)(block_buf + block_offset);
            if (entry->rec_len == 0 || block_offset + entry->rec_len > fs->block_size) {
                break;
            }
            if (entry->inode != 0 && entry->name_len > 0) {
                int ret = cb(entry, arg);
                if (ret != 0) {
                    kfree(block_buf);
                    return 0;
                }
            }
            block_offset += entry->rec_len;
            offset += entry->rec_len;
        }
        if (block_offset >= fs->block_size) {
            offset = (offset / fs->block_size + 1) * fs->block_size;
        }
    }
    kfree(block_buf);
    return 0;
}

static uint32_t ext4_dx_hash(const char* name)
{
    uint32_t hash = 0;
    while (*name) {
        hash = hash * 31 + (uint8_t)*name;
        name++;
    }
    return hash;
}

int ext4_lookup(ext4_fs_t* fs, uint32_t parent_ino, const char* name, uint32_t* out_ino)
{
    if (!fs || !name || !out_ino || !fs->mounted) return -1;
    ext4_inode_t dir_inode;
    if (ext4_read_inode(fs, parent_ino, &dir_inode) != 0) return -1;
    if ((dir_inode.i_mode & 0xF000) != 0x4000) return -1;
    if (dir_inode.i_flags & EXT4_INDEX_FL) {
        ext4_extent_header_t* eh = (ext4_extent_header_t*)dir_inode.i_block;
        if (eh->eh_magic == EXT4_EXT_MAGIC) {
            uint8_t* block_buf = (uint8_t*)kmalloc(fs->block_size);
            if (!block_buf) return -1;
            uint64_t root_block_num;
            if (ext4_read_extent_block(fs, &dir_inode, 0, &root_block_num, NULL) != 0) {
                kfree(block_buf);
                return -1;
            }
            if (ext4_read_block_raw(fs, root_block_num, block_buf) != 0) {
                kfree(block_buf);
                return -1;
            }
            ext4_dx_root_t* root = (ext4_dx_root_t*)block_buf;
            if (root->fake_dirent_inode == 0 &&
                root->dx_root_info_len > 0) {
                uint32_t hash = ext4_dx_hash(name);
                ext4_dx_node_t* node = (ext4_dx_node_t*)block_buf;
                uint16_t count = root->dx_root_count;
                uint32_t target_block = 0;
                for (uint16_t i = 0; i < count; i++) {
                    if (hash < root->dx_entry[i].hash) {
                        target_block = root->dx_entry[i].block;
                        break;
                    }
                }
                if (target_block == 0) {
                    target_block = root->dx_root_block;
                }
                kfree(block_buf);
                block_buf = (uint8_t*)kmalloc(fs->block_size);
                if (!block_buf) return -1;
                if (ext4_read_block_raw(fs, target_block, block_buf) != 0) {
                    kfree(block_buf);
                    return -1;
                }
                uint32_t boff = 12;
                while (boff < fs->block_size) {
                    ext4_dir_entry_t* de = (ext4_dir_entry_t*)(block_buf + boff);
                    if (de->rec_len == 0) break;
                    if (de->inode != 0 && de->name_len == strlen(name) &&
                        memcmp(de->name, name, de->name_len) == 0) {
                        *out_ino = de->inode;
                        kfree(block_buf);
                        return 0;
                    }
                    boff += de->rec_len;
                }
                kfree(block_buf);
                return -1;
            }
            kfree(block_buf);
        }
    }
    uint8_t* block_buf = (uint8_t*)kmalloc(fs->block_size);
    if (!block_buf) return -1;
    uint64_t dir_size = ext4_inode_size64(&dir_inode);
    uint64_t offset = 0;
    while (offset < dir_size) {
        uint64_t lblock = offset / fs->block_size;
        uint64_t phys_block;
        if (ext4_read_extent_block(fs, &dir_inode, lblock, &phys_block, NULL) != 0)
            break;
        if (ext4_read_block_raw(fs, phys_block, block_buf) != 0) break;
        uint32_t boff = (uint32_t)(offset % fs->block_size);
        while (boff < fs->block_size && offset < dir_size) {
            ext4_dir_entry_t* de = (ext4_dir_entry_t*)(block_buf + boff);
            if (de->rec_len == 0 || boff + de->rec_len > fs->block_size) break;
            if (de->inode != 0 && de->name_len == strlen(name) &&
                memcmp(de->name, name, de->name_len) == 0) {
                *out_ino = de->inode;
                kfree(block_buf);
                return 0;
            }
            boff += de->rec_len;
            offset += de->rec_len;
        }
        if (boff >= fs->block_size)
            offset = (offset / fs->block_size + 1) * fs->block_size;
    }
    kfree(block_buf);
    return -1;
}

int ext4_replay_journal(ext4_fs_t* fs)
{
    if (!fs || !fs->journal) return -1;
    int ret = jbd2_journal_load();
    if (ret != 0) return ret;
    ret = jbd2_journal_checkpoint();
    if (ret != 0) return ret;
    return 0;
}
