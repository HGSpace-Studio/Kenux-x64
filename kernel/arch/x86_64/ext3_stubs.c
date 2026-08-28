#include "kapi.h"

void ext3_free_block(void* sb, uint32_t block)
{
    (void)sb;
    (void)block;
}

uint32_t ext3_alloc_block(void* sb, uint32_t goal)
{
    (void)sb;
    (void)goal;
    return 0;
}

void ext3_free_inode(void* sb, uint32_t ino)
{
    (void)sb;
    (void)ino;
}

uint32_t ext3_alloc_inode(void* sb)
{
    (void)sb;
    return 0;
}

int ext3_write_inode_data(void* inode, const void* data, size_t size)
{
    (void)inode;
    (void)data;
    (void)size;
    return 0;
}

int ext3_read_inode_data(void* inode, void* data, size_t size)
{
    (void)inode;
    (void)data;
    (void)size;
    return 0;
}

int ext3_mount(const char* device, const char* mountpoint, unsigned long flags, const void* data)
{
    (void)device;
    (void)mountpoint;
    (void)flags;
    (void)data;
    return 0;
}

int ext3_read_block(void* sb, uint32_t block, void* buffer)
{
    (void)sb;
    (void)block;
    (void)buffer;
    return 0;
}

int ext3_get_inode(void* sb, uint32_t ino, void* inode)
{
    (void)sb;
    (void)ino;
    (void)inode;
    return 0;
}