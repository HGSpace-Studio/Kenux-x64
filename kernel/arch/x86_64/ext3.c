#include <arch/ext3.h>
#include <arch/ext2.h>
#include <arch/jbd2.h>
#include <arch/memory.h>
#include <arch/ahci.h>
#include <string.h>

static ext3_fs_t ext3_fs_table[EXT3_MAX_FS];
static uint32_t ext3_fs_count = 0;
static spinlock_t ext3_global_lock = SPINLOCK_INIT;

void ext3_init(void)
{
    spin_init(&ext3_global_lock);
    ext3_fs_count = 0;
    for (uint32_t i = 0; i < EXT3_MAX_FS; i++) {
        ext3_fs_table[i].mounted = 0;
        ext3_fs_table[i].group_desc = NULL;
        ext3_fs_table[i].journal = NULL;
        spin_init(&ext3_fs_table[i].lock);
        spin_init(&ext3_fs_table[i].tx_lock);
        ext3_fs_table[i].active_tx = 0;
        for (uint32_t j = 0; j < EXT3_MAX_TX; j++) {
            ext3_fs_table[i].transactions[j].transaction_id = 0;
            ext3_fs_table[i].transactions[j].num_blocks = 0;
            ext3_fs_table[i].transactions[j].block_numbers = NULL;
            ext3_fs_table[i].transactions[j].block_data = NULL;
            ext3_fs_table[i].transactions[j].flags = 0;
        }
    }
}

static int ext3_read_disk_block(void* device, uint64_t lba, void* buf, uint32_t sectors)
{
    (void)device;
    uint8_t* dst = (uint8_t*)buf;
    for (uint32_t i = 0; i < sectors; i++) {
        if (ahci_read_sector(0, lba + i, dst + i * 512) != 0) return -1;
    }
    return 0;
}

static int ext3_write_disk_block(void* device, uint64_t lba, const void* buf, uint32_t sectors)
{
    (void)device;
    const uint8_t* src = (const uint8_t*)buf;
    for (uint32_t i = 0; i < sectors; i++) {
        if (ahci_write_sector(0, lba + i, src + i * 512) != 0) return -1;
    }
    return 0;
}

int ext3_mount(vfs_node_t* mount_point, void* device)
{
    if (!device) return -1;
    spin_lock(&ext3_global_lock);
    if (ext3_fs_count >= EXT3_MAX_FS) {
        spin_unlock(&ext3_global_lock);
        return -2;
    }

    ext3_fs_t* fs = NULL;
    for (uint32_t i = 0; i < EXT3_MAX_FS; i++) {
        if (!ext3_fs_table[i].mounted) {
            fs = &ext3_fs_table[i];
            break;
        }
    }
    if (!fs) {
        spin_unlock(&ext3_global_lock);
        return -3;
    }

    uint8_t sb_buf[1024];
    if (ext3_read_disk_block(device, 2, sb_buf, 2) != 0) {
        spin_unlock(&ext3_global_lock);
        return -4;
    }

    memcpy(&fs->superblock, sb_buf, sizeof(ext2_superblock_t));
    if (fs->superblock.magic != EXT3_MAGIC) {
        spin_unlock(&ext3_global_lock);
        return -5;
    }

    if (!(fs->superblock.feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL)) {
        spin_unlock(&ext3_global_lock);
        return -6;
    }

    fs->block_size = 1024 << fs->superblock.log_block_size;
    fs->inode_size = (fs->superblock.rev_level >= 1) ? fs->superblock.inode_size : 128;
    fs->inodes_per_group = fs->superblock.inodes_per_group;
    fs->blocks_per_group = fs->superblock.blocks_per_group;
    fs->first_data_block = fs->superblock.first_data_block;

    uint64_t total_blocks = fs->superblock.block_count;
    fs->group_count = (uint32_t)((total_blocks + fs->blocks_per_group - 1) / fs->blocks_per_group);

    fs->group_desc = (ext2_group_desc_t*)memory_alloc(
        (uint64_t)fs->group_count * sizeof(ext2_group_desc_t));
    if (!fs->group_desc) {
        spin_unlock(&ext3_global_lock);
        return -7;
    }

    uint32_t gdt_blocks = (fs->group_count * sizeof(ext2_group_desc_t) + fs->block_size - 1) / fs->block_size;
    uint8_t* gdt_buf = (uint8_t*)memory_alloc((uint64_t)gdt_blocks * fs->block_size);
    if (!gdt_buf) {
        memory_free(fs->group_desc);
        fs->group_desc = NULL;
        spin_unlock(&ext3_global_lock);
        return -8;
    }

    uint64_t gdt_lba = (fs->first_data_block + 1) * (fs->block_size / 512);
    for (uint32_t i = 0; i < gdt_blocks; i++) {
        ext3_read_disk_block(device, gdt_lba + (uint64_t)i * (fs->block_size / 512),
                             gdt_buf + (uint64_t)i * fs->block_size, fs->block_size / 512);
    }
    memcpy(fs->group_desc, gdt_buf, (uint64_t)fs->group_count * sizeof(ext2_group_desc_t));
    memory_free(gdt_buf);

    fs->journal_inode = EXT3_JOURNAL_INODE;
    fs->device = device;
    fs->mounted = 1;
    spin_init(&fs->lock);
    spin_init(&fs->tx_lock);
    fs->active_tx = 0;

    ext3_recover(fs);

    if (mount_point) {
        mount_point->impl_data = fs;
    }

    ext3_fs_count++;
    spin_unlock(&ext3_global_lock);
    return 0;
}

int ext3_unmount(ext3_fs_t* fs)
{
    if (!fs || !fs->mounted) return -1;
    spin_lock(&fs->lock);
    if (fs->group_desc) {
        memory_free(fs->group_desc);
        fs->group_desc = NULL;
    }
    fs->mounted = 0;
    spin_unlock(&fs->lock);
    return 0;
}

int ext3_read_block(ext3_fs_t* fs, uint32_t block, void* buf)
{
    if (!fs || !buf) return -1;
    uint64_t lba = (uint64_t)block * (fs->block_size / 512);
    return ext3_read_disk_block(fs->device, lba, buf, fs->block_size / 512);
}

int ext3_write_block(ext3_fs_t* fs, uint32_t block, const void* buf)
{
    if (!fs || !buf) return -1;
    uint64_t lba = (uint64_t)block * (fs->block_size / 512);
    return ext3_write_disk_block(fs->device, lba, buf, fs->block_size / 512);
}

int ext3_journal_start(ext3_fs_t* fs)
{
    if (!fs) return -1;
    spin_lock(&fs->tx_lock);
    uint32_t tx_id = fs->active_tx;
    if (tx_id >= EXT3_MAX_TX) {
        spin_unlock(&fs->tx_lock);
        return -2;
    }
    ext3_transaction_t* tx = &fs->transactions[tx_id];
    tx->transaction_id = tx_id + 1;
    tx->num_blocks = 0;
    tx->block_numbers = (uint32_t*)memory_alloc(4096 * sizeof(uint32_t));
    tx->block_data = (void**)memory_alloc(4096 * sizeof(void*));
    tx->flags = EXT3_JOURNAL_COMMIT;
    if (!tx->block_numbers || !tx->block_data) {
        if (tx->block_numbers) memory_free(tx->block_numbers);
        if (tx->block_data) memory_free(tx->block_data);
        spin_unlock(&fs->tx_lock);
        return -3;
    }
    spin_unlock(&fs->tx_lock);
    return (int)tx_id;
}

int ext3_journal_commit(ext3_fs_t* fs)
{
    if (!fs) return -1;
    spin_lock(&fs->tx_lock);
    uint32_t tx_id = fs->active_tx;
    if (tx_id >= EXT3_MAX_TX) {
        spin_unlock(&fs->tx_lock);
        return -2;
    }
    ext3_transaction_t* tx = &fs->transactions[tx_id];
    if (tx->transaction_id == 0) {
        spin_unlock(&fs->tx_lock);
        return -3;
    }

    for (uint32_t i = 0; i < tx->num_blocks; i++) {
        ext3_write_block(fs, tx->block_numbers[i], tx->block_data[i]);
    }

    if (tx->block_numbers) memory_free(tx->block_numbers);
    if (tx->block_data) memory_free(tx->block_data);
    for (uint32_t i = 0; i < tx->num_blocks; i++) {
        if (tx->block_data && i < 4096) {
        }
    }
    tx->transaction_id = 0;
    tx->num_blocks = 0;
    tx->block_numbers = NULL;
    tx->block_data = NULL;
    fs->active_tx = (fs->active_tx + 1) % EXT3_MAX_TX;
    spin_unlock(&fs->tx_lock);
    return 0;
}

int ext3_journal_abort(ext3_fs_t* fs)
{
    if (!fs) return -1;
    spin_lock(&fs->tx_lock);
    uint32_t tx_id = fs->active_tx;
    if (tx_id < EXT3_MAX_TX) {
        ext3_transaction_t* tx = &fs->transactions[tx_id];
        if (tx->block_numbers) memory_free(tx->block_numbers);
        if (tx->block_data) memory_free(tx->block_data);
        tx->transaction_id = 0;
        tx->num_blocks = 0;
        tx->block_numbers = NULL;
        tx->block_data = NULL;
        tx->flags = EXT3_JOURNAL_REVOKE;
    }
    spin_unlock(&fs->tx_lock);
    return 0;
}

int ext3_recover(ext3_fs_t* fs)
{
    if (!fs) return -1;
    if (fs->superblock.state & 0x0001) {
        ext2_inode_t* journal_inode = ext3_get_inode(fs, fs->journal_inode);
        if (journal_inode) {
            memory_free(journal_inode);
        }
        fs->superblock.state &= ~0x0001;
        uint8_t sb_buf[1024];
        memcpy(sb_buf, &fs->superblock, sizeof(ext2_superblock_t));
        ext3_write_block(fs, fs->first_data_block, sb_buf);
    }
    return 0;
}

ext2_inode_t* ext3_get_inode(ext3_fs_t* fs, uint32_t ino)
{
    if (!fs || ino == 0) return NULL;
    uint32_t group = (ino - 1) / fs->inodes_per_group;
    uint32_t idx = (ino - 1) % fs->inodes_per_group;
    if (group >= fs->group_count) return NULL;

    uint32_t inode_table_block = fs->group_desc[group].inode_table;
    uint32_t inodes_per_block = fs->block_size / fs->inode_size;
    uint32_t block_offset = idx / inodes_per_block;
    uint32_t offset_in_block = (idx % inodes_per_block) * fs->inode_size;

    uint8_t* block_buf = (uint8_t*)memory_alloc(fs->block_size);
    if (!block_buf) return NULL;

    if (ext3_read_block(fs, inode_table_block + block_offset, block_buf) != 0) {
        memory_free(block_buf);
        return NULL;
    }

    ext2_inode_t* result = (ext2_inode_t*)memory_alloc(fs->inode_size);
    if (!result) {
        memory_free(block_buf);
        return NULL;
    }
    memcpy(result, block_buf + offset_in_block, fs->inode_size);
    memory_free(block_buf);
    return result;
}

static uint32_t ext3_get_block_from_inode(ext3_fs_t* fs, ext2_inode_t* inode, uint32_t logical_block)
{
    uint32_t entries_per_block = fs->block_size / sizeof(uint32_t);

    if (logical_block < 12) {
        return inode->block[logical_block];
    }

    uint32_t rel = logical_block - 12;
    if (rel < entries_per_block) {
        uint32_t ind = inode->block[12];
        if (ind == 0) return 0;
        uint32_t* buf = (uint32_t*)memory_alloc(fs->block_size);
        if (!buf) return 0;
        ext3_read_block(fs, ind, buf);
        uint32_t result = buf[rel];
        memory_free(buf);
        return result;
    }

    uint32_t rel2 = rel - entries_per_block;
    uint32_t dind_count = entries_per_block * entries_per_block;
    if (rel2 < dind_count) {
        uint32_t dind = inode->block[13];
        if (dind == 0) return 0;
        uint32_t* buf = (uint32_t*)memory_alloc(fs->block_size);
        if (!buf) return 0;
        ext3_read_block(fs, dind, buf);
        uint32_t ind = buf[rel2 / entries_per_block];
        if (ind == 0) { memory_free(buf); return 0; }
        ext3_read_block(fs, ind, buf);
        uint32_t result = buf[rel2 % entries_per_block];
        memory_free(buf);
        return result;
    }

    uint32_t rel3 = rel2 - dind_count;
    uint32_t tind_count = entries_per_block * dind_count;
    if (rel3 < tind_count) {
        uint32_t tind = inode->block[14];
        if (tind == 0) return 0;
        uint32_t* buf = (uint32_t*)memory_alloc(fs->block_size);
        if (!buf) return 0;
        ext3_read_block(fs, tind, buf);
        uint32_t dind = buf[rel3 / dind_count];
        if (dind == 0) { memory_free(buf); return 0; }
        ext3_read_block(fs, dind, buf);
        uint32_t ind = buf[(rel3 % dind_count) / entries_per_block];
        if (ind == 0) { memory_free(buf); return 0; }
        ext3_read_block(fs, ind, buf);
        uint32_t result = buf[(rel3 % dind_count) % entries_per_block];
        memory_free(buf);
        return result;
    }

    return 0;
}

int ext3_read_inode_data(ext3_fs_t* fs, ext2_inode_t* inode, uint64_t offset, void* buf, uint64_t size)
{
    if (!fs || !inode || !buf) return -1;

    uint64_t total_read = 0;
    uint32_t start_block = (uint32_t)(offset / fs->block_size);
    uint32_t offset_in_block = (uint32_t)(offset % fs->block_size);
    uint8_t* block_buf = (uint8_t*)memory_alloc(fs->block_size);
    if (!block_buf) return -2;

    while (total_read < size) {
        uint32_t phys_block = ext3_get_block_from_inode(fs, inode, start_block);
        if (phys_block == 0) break;

        if (ext3_read_block(fs, phys_block, block_buf) != 0) break;

        uint64_t avail = fs->block_size - offset_in_block;
        uint64_t want = size - total_read;
        uint64_t copy = (want < avail) ? want : avail;

        memcpy((uint8_t*)buf + total_read, block_buf + offset_in_block, copy);
        total_read += copy;
        start_block++;
        offset_in_block = 0;
    }

    memory_free(block_buf);
    return (int)total_read;
}

int ext3_write_inode_data(ext3_fs_t* fs, ext2_inode_t* inode, uint64_t offset, const void* buf, uint64_t size)
{
    if (!fs || !inode || !buf) return -1;

    uint64_t total_written = 0;
    uint32_t start_block = (uint32_t)(offset / fs->block_size);
    uint32_t offset_in_block = (uint32_t)(offset % fs->block_size);
    uint8_t* block_buf = (uint8_t*)memory_alloc(fs->block_size);
    if (!block_buf) return -2;

    while (total_written < size) {
        uint32_t phys_block = ext3_get_block_from_inode(fs, inode, start_block);
        if (phys_block == 0) {
            phys_block = ext3_alloc_block(fs);
            if (phys_block == 0) break;
        }

        if (offset_in_block != 0 || (size - total_written) < fs->block_size) {
            ext3_read_block(fs, phys_block, block_buf);
        }

        uint64_t avail = fs->block_size - offset_in_block;
        uint64_t want = size - total_written;
        uint64_t copy = (want < avail) ? want : avail;

        memcpy(block_buf + offset_in_block, (const uint8_t*)buf + total_written, copy);
        ext3_write_block(fs, phys_block, block_buf);
        total_written += copy;
        start_block++;
        offset_in_block = 0;
    }

    memory_free(block_buf);
    return (int)total_written;
}

uint32_t ext3_alloc_block(ext3_fs_t* fs)
{
    if (!fs || !fs->group_desc) return 0;
    for (uint32_t g = 0; g < fs->group_count; g++) {
        if (fs->group_desc[g].free_blocks == 0) continue;
        uint32_t bitmap_block = fs->group_desc[g].block_bitmap;
        uint8_t* bitmap = (uint8_t*)memory_alloc(fs->block_size);
        if (!bitmap) continue;
        ext3_read_block(fs, bitmap_block, bitmap);
        uint32_t bits = fs->block_size * 8;
        for (uint32_t i = 0; i < bits && i < fs->blocks_per_group; i++) {
            if (!(bitmap[i / 8] & (1 << (i % 8)))) {
                bitmap[i / 8] |= (1 << (i % 8));
                ext3_write_block(fs, bitmap_block, bitmap);
                memory_free(bitmap);
                fs->group_desc[g].free_blocks--;
                uint32_t block = g * fs->blocks_per_group + i + fs->first_data_block;
                return block;
            }
        }
        memory_free(bitmap);
    }
    return 0;
}

void ext3_free_block(ext3_fs_t* fs, uint32_t block)
{
    if (!fs || !fs->group_desc || block < fs->first_data_block) return;
    uint32_t rel = block - fs->first_data_block;
    uint32_t g = rel / fs->blocks_per_group;
    uint32_t i = rel % fs->blocks_per_group;
    if (g >= fs->group_count) return;

    uint32_t bitmap_block = fs->group_desc[g].block_bitmap;
    uint8_t* bitmap = (uint8_t*)memory_alloc(fs->block_size);
    if (!bitmap) return;
    ext3_read_block(fs, bitmap_block, bitmap);
    bitmap[i / 8] &= ~(1 << (i % 8));
    ext3_write_block(fs, bitmap_block, bitmap);
    memory_free(bitmap);
    fs->group_desc[g].free_blocks++;
}

uint32_t ext3_alloc_inode(ext3_fs_t* fs)
{
    if (!fs || !fs->group_desc) return 0;
    for (uint32_t g = 0; g < fs->group_count; g++) {
        if (fs->group_desc[g].free_inodes == 0) continue;
        uint32_t bitmap_block = fs->group_desc[g].inode_bitmap;
        uint8_t* bitmap = (uint8_t*)memory_alloc(fs->block_size);
        if (!bitmap) continue;
        ext3_read_block(fs, bitmap_block, bitmap);
        uint32_t bits = fs->block_size * 8;
        for (uint32_t i = 0; i < bits && i < fs->inodes_per_group; i++) {
            if (!(bitmap[i / 8] & (1 << (i % 8)))) {
                bitmap[i / 8] |= (1 << (i % 8));
                ext3_write_block(fs, bitmap_block, bitmap);
                memory_free(bitmap);
                fs->group_desc[g].free_inodes--;
                return g * fs->inodes_per_group + i + 1;
            }
        }
        memory_free(bitmap);
    }
    return 0;
}

void ext3_free_inode(ext3_fs_t* fs, uint32_t ino)
{
    if (!fs || !fs->group_desc || ino == 0) return;
    uint32_t g = (ino - 1) / fs->inodes_per_group;
    uint32_t i = (ino - 1) % fs->inodes_per_group;
    if (g >= fs->group_count) return;

    uint32_t bitmap_block = fs->group_desc[g].inode_bitmap;
    uint8_t* bitmap = (uint8_t*)memory_alloc(fs->block_size);
    if (!bitmap) return;
    ext3_read_block(fs, bitmap_block, bitmap);
    bitmap[i / 8] &= ~(1 << (i % 8));
    ext3_write_block(fs, bitmap_block, bitmap);
    memory_free(bitmap);
    fs->group_desc[g].free_inodes++;
}