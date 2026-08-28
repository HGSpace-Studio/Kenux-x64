#include <arch/fs.h>
#include <arch/memory.h>
#include <string.h>

static vfs_node_t devfs_root;
static spinlock_t devfs_lock = SPINLOCK_INIT;

typedef struct {
    char name[FS_MAX_NAME];
    uint32_t major;
    uint32_t minor;
    uint16_t type;
    vfs_node_t* node;
} devfs_entry_t;

#define DEVFS_MAX_ENTRIES 256

static devfs_entry_t devfs_entries[DEVFS_MAX_ENTRIES];
static uint32_t devfs_entry_count = 0;

static int devfs_lookup(vfs_node_t* parent, const char* name, vfs_node_t** out)
{
    if (!parent || !name || !out) return -1;

    spinlock_acquire(&devfs_lock);
    for (uint32_t i = 0; i < devfs_entry_count; i++) {
        if (strcmp(devfs_entries[i].name, name) == 0) {
            *out = devfs_entries[i].node;
            spinlock_release(&devfs_lock);
            return 0;
        }
    }
    spinlock_release(&devfs_lock);
    return -2;
}

static int devfs_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* dirent)
{
    if (!node || !dirent) return -1;

    spinlock_acquire(&devfs_lock);
    if (index >= devfs_entry_count) {
        spinlock_release(&devfs_lock);
        return -2;
    }

    strncpy(dirent->name, devfs_entries[index].name, FS_MAX_NAME - 1);
    dirent->name[FS_MAX_NAME - 1] = '\0';
    dirent->ino = index;
    dirent->type = devfs_entries[index].type;

    spinlock_release(&devfs_lock);
    return 0;
}

static int devfs_mknod(vfs_node_t* parent, const char* name, uint32_t major, uint32_t minor, uint16_t type)
{
    if (!name) return -1;

    spinlock_acquire(&devfs_lock);

    if (devfs_entry_count >= DEVFS_MAX_ENTRIES) {
        spinlock_release(&devfs_lock);
        return -2;
    }

    for (uint32_t i = 0; i < devfs_entry_count; i++) {
        if (strcmp(devfs_entries[i].name, name) == 0) {
            spinlock_release(&devfs_lock);
            return -3;
        }
    }

    devfs_entry_t* entry = &devfs_entries[devfs_entry_count];
    strncpy(entry->name, name, FS_MAX_NAME - 1);
    entry->name[FS_MAX_NAME - 1] = '\0';
    entry->major = major;
    entry->minor = minor;
    entry->type = type;

    entry->node = (vfs_node_t*)memory_alloc(sizeof(vfs_node_t));
    if (!entry->node) {
        spinlock_release(&devfs_lock);
        return -4;
    }
    memset(entry->node, 0, sizeof(vfs_node_t));
    strncpy(entry->node->name, name, FS_MAX_NAME - 1);
    entry->node->type = type;
    entry->node->nlink = 1;

    devfs_entry_count++;
    spinlock_release(&devfs_lock);
    return 0;
}

int devfs_init(void)
{
    memset(&devfs_root, 0, sizeof(vfs_node_t));
    strncpy(devfs_root.name, "dev", FS_MAX_NAME - 1);
    devfs_root.type = FS_TYPE_DIR;
    devfs_root.nlink = 2;
    devfs_root.ops.lookup = devfs_lookup;
    devfs_root.ops.readdir = devfs_readdir;

    memset(devfs_entries, 0, sizeof(devfs_entries));
    devfs_entry_count = 0;

    devfs_mknod(&devfs_root, "null",    1, 3,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "zero",    1, 5,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "full",    1, 7,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "random",  1, 8,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "urandom", 1, 9,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "console", 5, 1,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "tty0",    4, 0,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "tty1",    4, 1,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "ttyS0",   4, 64, FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "stdin",   0, 0,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "stdout",  0, 1,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "stderr",  0, 2,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "sda",     8, 0,  FS_TYPE_BLKDEV);
    devfs_mknod(&devfs_root, "sda1",    8, 1,  FS_TYPE_BLKDEV);
    devfs_mknod(&devfs_root, "sdb",     8, 16, FS_TYPE_BLKDEV);
    devfs_mknod(&devfs_root, "nvme0",   0, 0,  FS_TYPE_BLKDEV);
    devfs_mknod(&devfs_root, "input",   0, 0,  FS_TYPE_DIR);
    devfs_mknod(&devfs_root, "mouse0",  13, 0, FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "kbd0",    13, 1, FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "fb0",     29, 0, FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "dri",     0, 0,  FS_TYPE_DIR);
    devfs_mknod(&devfs_root, "snd",     0, 0,  FS_TYPE_DIR);
    devfs_mknod(&devfs_root, "net",     0, 0,  FS_TYPE_DIR);
    devfs_mknod(&devfs_root, "shm",     0, 0,  FS_TYPE_DIR);
    devfs_mknod(&devfs_root, "pts",     0, 0,  FS_TYPE_DIR);
    devfs_mknod(&devfs_root, "ptmx",    5, 2,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "hpet",    0, 0,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "kmsg",    0, 0,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "mem",     1, 1,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "kmem",    1, 2,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "port",    1, 4,  FS_TYPE_CHARDEV);
    devfs_mknod(&devfs_root, "cpu",     0, 0,  FS_TYPE_DIR);
    devfs_mknod(&devfs_root, "disk",    0, 0,  FS_TYPE_DIR);

    return 0;
}

vfs_node_t* devfs_get_root(void)
{
    return &devfs_root;
}

int devfs_register(const char* name, uint32_t major, uint32_t minor, uint16_t type)
{
    return devfs_mknod(&devfs_root, name, major, minor, type);
}

int devfs_unregister(const char* name)
{
    if (!name) return -1;

    spinlock_acquire(&devfs_lock);
    for (uint32_t i = 0; i < devfs_entry_count; i++) {
        if (strcmp(devfs_entries[i].name, name) == 0) {
            if (devfs_entries[i].node) memory_free(devfs_entries[i].node);
            devfs_entries[i] = devfs_entries[devfs_entry_count - 1];
            memset(&devfs_entries[devfs_entry_count - 1], 0, sizeof(devfs_entry_t));
            devfs_entry_count--;
            spinlock_release(&devfs_lock);
            return 0;
        }
    }
    spinlock_release(&devfs_lock);
    return -2;
}