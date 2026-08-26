#include <arch/ext2.h>
#include <arch/memory.h>
#include <string.h>
#include <fs.h>
#include <arch/sata.h>
#include <arch/ahci.h>

static ext2_fs_t ext2_filesystems[16];
static int ext2_fs_count = 0;

static uint32_t ext2_block_to_lba(ext2_fs_t* fs, uint32_t block)
{
    return block * (EXT2_BLOCK_SIZE / 512);
}

static int ext2_read_block(ext2_fs_t* fs, uint32_t block, void* buffer)
{
    uint64_t lba = ext2_block_to_lba(fs, block);
    return ahci_read_sector(0, lba, buffer);
}

static int ext2_write_block(ext2_fs_t* fs, uint32_t block, const void* buffer)
{
    uint64_t lba = ext2_block_to_lba(fs, block);
    return ahci_write_sector(0, lba, buffer);
}

static ext2_inode_t* ext2_get_inode(ext2_fs_t* fs, uint32_t inode_num)
{
    uint32_t group = (inode_num - 1) / fs->superblock->inodes_per_group;
    uint32_t inode_table_block = fs->group_desc[group].inode_table;
    uint32_t inode_size = fs->superblock->inode_size;

    ext2_inode_t* inode_table = memory_alloc(EXT2_BLOCK_SIZE);
    ext2_read_block(fs, inode_table_block, inode_table);

    ext2_inode_t* inode = &inode_table[(inode_num - 1) % fs->superblock->inodes_per_group];

    ext2_inode_t* result = memory_alloc(inode_size);
    memcpy(result, inode, inode_size);

    memory_free(inode_table);

    return result;
}

static uint32_t ext2_get_block_num(ext2_fs_t* fs, ext2_inode_t* inode, uint32_t block_num)
{
    uint32_t entries_per_block = EXT2_BLOCK_SIZE / sizeof(uint32_t);

    if (block_num < 12) {
        return inode->block[block_num];
    }

    uint32_t rel = block_num - 12;
    if (rel < entries_per_block) {
        uint32_t ind_block = inode->block[12];
        if (ind_block == 0) return 0;
        uint32_t* buf = (uint32_t*)memory_alloc(EXT2_BLOCK_SIZE);
        ext2_read_block(fs, ind_block, buf);
        uint32_t result = buf[rel];
        memory_free(buf);
        return result;
    }

    uint32_t rel2 = rel - entries_per_block;
    uint32_t dind_entries = entries_per_block * entries_per_block;
    if (rel2 < dind_entries) {
        uint32_t dind_block = inode->block[13];
        if (dind_block == 0) return 0;
        uint32_t* buf = (uint32_t*)memory_alloc(EXT2_BLOCK_SIZE);
        ext2_read_block(fs, dind_block, buf);
        uint32_t ind_idx = rel2 / entries_per_block;
        uint32_t ind_block = buf[ind_idx];
        if (ind_block == 0) {
            memory_free(buf);
            return 0;
        }
        ext2_read_block(fs, ind_block, buf);
        uint32_t result = buf[rel2 % entries_per_block];
        memory_free(buf);
        return result;
    }

    uint32_t rel3 = rel2 - dind_entries;
    uint32_t tind_entries = entries_per_block * dind_entries;
    if (rel3 < tind_entries) {
        uint32_t tind_block = inode->block[14];
        if (tind_block == 0) return 0;
        uint32_t* buf = (uint32_t*)memory_alloc(EXT2_BLOCK_SIZE);
        ext2_read_block(fs, tind_block, buf);
        uint32_t dind_idx = rel3 / dind_entries;
        uint32_t dind_block = buf[dind_idx];
        if (dind_block == 0) {
            memory_free(buf);
            return 0;
        }
        ext2_read_block(fs, dind_block, buf);
        uint32_t ind_idx = (rel3 % dind_entries) / entries_per_block;
        uint32_t ind_block = buf[ind_idx];
        if (ind_block == 0) {
            memory_free(buf);
            return 0;
        }
        ext2_read_block(fs, ind_block, buf);
        uint32_t result = buf[rel3 % entries_per_block];
        memory_free(buf);
        return result;
    }

    return 0;
}

static int ext2_read_file(ext2_fs_t* fs, ext2_inode_t* inode, void* buffer, uint64_t size, uint64_t offset)
{
    uint64_t bytes_read = 0;
    uint64_t current_offset = offset;

    while (bytes_read < size && current_offset < inode->size) {
        uint32_t block_num = current_offset / EXT2_BLOCK_SIZE;
        uint32_t block_offset = current_offset % EXT2_BLOCK_SIZE;
        uint32_t read_size = EXT2_BLOCK_SIZE - block_offset;

        if (bytes_read + read_size > size) {
            read_size = size - bytes_read;
        }

        uint32_t block = ext2_get_block_num(fs, inode, block_num);
        if (block == 0) {
            break;
        }

        void* block_buffer = memory_alloc(EXT2_BLOCK_SIZE);
        ext2_read_block(fs, block, block_buffer);

        memcpy((uint8_t*)buffer + bytes_read, (uint8_t*)block_buffer + block_offset, read_size);

        memory_free(block_buffer);

        current_offset += read_size;
        bytes_read += read_size;
    }

    return bytes_read;
}

typedef struct {
    ext2_fs_t* fs;
    ext2_inode_t* inode;
    uint32_t inode_num;
    uint64_t offset;
    int used;
} ext2_fd_t;

static ext2_fd_t ext2_fds[256];

static int ext2_alloc_fd(ext2_fs_t* fs, uint32_t inode_num)
{
    for (int i = 0; i < 256; i++) {
        if (!ext2_fds[i].used) {
            ext2_fds[i].fs = fs;
            ext2_fds[i].inode = ext2_get_inode(fs, inode_num);
            ext2_fds[i].inode_num = inode_num;
            ext2_fds[i].offset = 0;
            ext2_fds[i].used = 1;
            return i;
        }
    }
    return -1;
}

static ext2_fd_t* ext2_get_fd(int fd)
{
    if (fd < 0 || fd >= 256) return NULL;
    if (!ext2_fds[fd].used) return NULL;
    return &ext2_fds[fd];
}

static uint32_t ext2_lookup_path(ext2_fs_t* fs, const char* path)
{
    if (!path || path[0] != '/') return 0;

    uint32_t current_ino = 2;
    char temp[256];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char* p = temp + 1;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        char* token = p;
        while (*p && *p != '/') p++;
        if (*p == '/') *p++ = '\0';

        ext2_inode_t* dir = ext2_get_inode(fs, current_ino);
        if (!dir) return 0;

        uint32_t found = 0;
        uint8_t* block_buf = (uint8_t*)memory_alloc(EXT2_BLOCK_SIZE);
        uint32_t dir_blocks = (dir->size + EXT2_BLOCK_SIZE - 1) / EXT2_BLOCK_SIZE;

        for (uint32_t b = 0; b < dir_blocks && !found; b++) {
            uint32_t block = ext2_get_block_num(fs, dir, b);
            if (block == 0) continue;
            ext2_read_block(fs, block, block_buf);

            uint32_t offset = 0;
            while (offset < EXT2_BLOCK_SIZE) {
                ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buf + offset);
                if (entry->rec_len == 0) break;
                if (entry->inode != 0 && entry->name_len == strlen(token) &&
                    memcmp(entry->name, token, entry->name_len) == 0) {
                    found = entry->inode;
                    break;
                }
                offset += entry->rec_len;
            }
        }

        memory_free(block_buf);
        memory_free(dir);

        if (!found) return 0;
        current_ino = found;
    }

    return current_ino;
}

void ext2_init(void)
{
    memset(ext2_filesystems, 0, sizeof(ext2_filesystems));
    ext2_fs_count = 0;
    memset(ext2_fds, 0, sizeof(ext2_fds));
}

int ext2_mount(const char* device, const char* mount_point)
{
    (void)device; (void)mount_point;
    if (ext2_fs_count >= 16) {
        return -1;
    }

    ext2_fs_t* fs = &ext2_filesystems[ext2_fs_count];

    void* superblock_buffer = memory_alloc(EXT2_BLOCK_SIZE);
    ahci_read_sector(0, 2, superblock_buffer);

    fs->superblock = (ext2_superblock_t*)superblock_buffer;

    if (fs->superblock->magic != EXT2_MAGIC) {
        memory_free(superblock_buffer);
        return -1;
    }

    fs->block_size = EXT2_BLOCK_SIZE;
    fs->block_groups = (fs->superblock->block_count + fs->superblock->blocks_per_group - 1) / fs->superblock->blocks_per_group;

    fs->group_desc = memory_alloc(EXT2_BLOCK_SIZE);
    ext2_read_block(fs, 1, fs->group_desc);

    ext2_fs_count++;

    return 0;
}

int ext2_unmount(const char* mount_point)
{
    (void)mount_point;
    for (int i = 0; i < 256; i++) {
        if (ext2_fds[i].used && ext2_fds[i].fs) {
            memory_free(ext2_fds[i].inode);
            ext2_fds[i].used = 0;
        }
    }
    return 0;
}

int ext2_open(const char* path)
{
    if (!path) return -1;
    if (ext2_fs_count == 0) return -1;
    ext2_fs_t* fs = &ext2_filesystems[ext2_fs_count - 1];
    uint32_t ino = ext2_lookup_path(fs, path);
    if (ino == 0) return -1;
    return ext2_alloc_fd(fs, ino);
}

int ext2_close(int fd)
{
    ext2_fd_t* f = ext2_get_fd(fd);
    if (!f) return -1;
    if (f->inode) memory_free(f->inode);
    f->used = 0;
    f->inode = NULL;
    return 0;
}

int ext2_read(int fd, void* buffer, uint64_t size)
{
    ext2_fd_t* f = ext2_get_fd(fd);
    if (!f || !f->inode) return -1;
    int ret = ext2_read_file(f->fs, f->inode, buffer, size, f->offset);
    if (ret > 0) f->offset += ret;
    return ret;
}

int ext2_write(int fd, const void* buffer, uint64_t size)
{
    (void)fd; (void)buffer; (void)size;
    return -1;
}

int ext2_seek(int fd, uint64_t offset)
{
    ext2_fd_t* f = ext2_get_fd(fd);
    if (!f) return -1;
    f->offset = offset;
    return 0;
}

int ext2_mkdir(const char* path)
{
    (void)path;
    return -1;
}

int ext2_rmdir(const char* path)
{
    (void)path;
    return -1;
}

int ext2_unlink(const char* path)
{
    (void)path;
    return -1;
}
