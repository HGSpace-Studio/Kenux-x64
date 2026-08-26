#include <arch/fs.h>
#include <arch/memory.h>
#include <arch/spinlock.h>
#include <string.h>
#include <arch/time.h>

#define VFS_MAX_MOUNT_POINTS  32
#define VFS_MAX_OPEN_FILES    1024
#define VFS_MAX_INODE_CACHE   4096
#define VFS_MAX_DENTRY_CACHE  8192
#define VFS_MAX_PATH_LENGTH   4096
#define VFS_READ_AHEAD_SIZE   (64 * 1024)
#define VFS_WRITE_BUFFER_SIZE (128 * 1024)
#define VFS_DIR_CACHE_SIZE    256

typedef struct inode_cache_entry {
    uint64_t ino;
    uint32_t dev;
    struct inode* inode;
    int ref_count;
    bool dirty;
    uint64_t last_access_time;
    struct inode_cache_entry* hash_next;
    struct inode_cache_entry* lru_prev;
    struct inode_cache_entry* lru_next;
} inode_cache_entry_t;

typedef struct dentry_cache_entry {
    char name[256];
    uint64_t parent_ino;
    struct dentry* dentry;
    int ref_count;
    uint64_t last_access_time;
    struct dentry_cache_entry* hash_next;
    struct dentry_cache_entry* lru_prev;
    struct dentry_cache_entry* lru_next;
} dentry_cache_entry_t;

typedef struct file_handle {
    int fd;
    struct inode* inode;
    uint64_t position;
    int flags;
    int mode;
    int ref_count;
    bool dirty;
    uint8_t* read_ahead_buffer;
    size_t read_ahead_size;
    off_t read_ahead_offset;
    uint8_t* write_buffer;
    size_t write_buffer_size;
    size_t write_buffer_pos;
    off_t write_sync_position;
    spinlock_t lock;
    uint64_t open_time;
    uint64_t last_access_time;
    uint64_t read_count;
    uint64_t write_count;
    uint64_t seek_count;
    char path[VFS_MAX_PATH_LENGTH];
} file_handle_t;

typedef struct mount_point {
    int mount_id;
    char source[256];
    char target[256];
    char filesystem_type[32];
    unsigned long mount_flags;
    void* fs_data;
    struct super_block* sb;
    bool mounted;
    uint64_t mount_time;
    uint64_t access_count;
    spinlock_t lock;
} mount_point_t;

typedef struct dir_cache_entry {
    char path[VFS_MAX_PATH_LENGTH];
    struct dirent* entries;
    int entry_count;
    int capacity;
    uint64_t cache_time;
    uint64_t access_time;
    int access_count;
    bool valid;
    spinlock_t lock;
} dir_cache_entry_t;

typedef struct vfs_stats {
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t total_opens;
    uint64_t total_closes;
    uint64_t total_seeks;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t read_ahead_hits;
    uint64_t write_buffer_flushes;
    uint64_t inode_lookups;
    uint64_t dentry_lookups;
    uint64_t path_resolutions;
    uint64_t mount_operations;
    uint64_t sync_operations;
    uint64_t bytes_read;
    uint64_t bytes_written;
    double cache_hit_ratio;
    double avg_read_latency_ns;
    double avg_write_latency_ns;
    uint64_t current_open_files;
    int active_mount_points;
    uint64_t memory_used;
} vfs_stats_t;

static inode_cache_entry_t* inode_cache[VFS_MAX_INODE_CACHE];
static dentry_cache_entry_t* dentry_cache[VFS_MAX_DENTRY_CACHE];
static file_handle_t file_handles[VFS_MAX_OPEN_FILES];
static mount_point_t mount_points[VFS_MAX_MOUNT_POINTS];
static dir_cache_entry_t dir_cache[VFS_DIR_CACHE_SIZE];
static vfs_stats_t vfs_statistics;
static spinlock_t vfs_global_lock;
static spinlock_t inode_cache_lock;
static spinlock_t dentry_cache_lock;
static spinlock_t file_table_lock;
static int next_fd = 3;
static int next_mount_id = 1;
static int initialized = 0;

void vfs_init_enhanced(void)
{
    if (initialized) return;

    spin_init(&vfs_global_lock);
    spin_init(&inode_cache_lock);
    spin_init(&dentry_cache_lock);
    spin_init(&file_table_lock);

    memset(inode_cache, 0, sizeof(inode_cache));
    memset(dentry_cache, 0, sizeof(dentry_cache));
    memset(file_handles, 0, sizeof(file_handles));
    memset(mount_points, 0, sizeof(mount_points));
    memset(dir_cache, 0, sizeof(dir_cache));
    memset(&vfs_statistics, 0, sizeof(vfs_stats_t));

    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        file_handles[i].fd = -1;
        spin_init(&file_handles[i].lock);
    }

    for (int i = 0; i < VFS_MAX_MOUNT_POINTS; i++) {
        mount_points[i].mount_id = 0;
        spin_init(&mount_points[i].lock);
    }

    for (int i = 0; i < VFS_DIR_CACHE_SIZE; i++) {
        spin_init(&dir_cache[i].lock);
    }

    initialized = 1;
}

struct inode* vfs_inode_lookup(uint64_t ino, uint32_t dev)
{
    if (!initialized) return NULL;

    spin_lock(&inode_cache_lock);

    uint32_t hash = (ino ^ dev) % VFS_MAX_INODE_CACHE;
    inode_cache_entry_t* entry = inode_cache[hash];

    while (entry) {
        if (entry->ino == ino && entry->dev == dev) {
            entry->ref_count++;
            entry->last_access_time = get_current_time_ns();
            vfs_statistics.cache_hits++;

            if (entry->lru_prev) {
                entry->lru_prev->lru_next = entry->lru_next;
            }
            if (entry->lru_next) {
                entry->lru_next->lru_prev = entry->lru_prev;
            }
            entry->lru_prev = NULL;
            entry->lru_next = inode_cache[hash];
            if (inode_cache[hash]) {
                inode_cache[hash]->lru_prev = entry;
            }
            inode_cache[hash] = entry;

            spin_unlock(&inode_cache_lock);
            return entry->inode;
        }
        entry = entry->hash_next;
    }

    vfs_statistics.cache_misses++;
    spin_unlock(&inode_cache_lock);

    return NULL;
}

void vfs_inode_cache(struct inode* inode)
{
    if (!initialized || !inode) return;

    spin_lock(&inode_cache_lock);

    uint32_t hash = (inode->i_ino ^ inode->i_dev) % VFS_MAX_INODE_CACHE;
    inode_cache_entry_t* new_entry = kzalloc(sizeof(inode_cache_entry_t));

    if (new_entry) {
        new_entry->ino = inode->i_ino;
        new_entry->dev = inode->i_dev;
        new_entry->inode = inode;
        new_entry->ref_count = 1;
        new_entry->dirty = false;
        new_entry->last_access_time = get_current_time_ns();
        new_entry->hash_next = inode_cache[hash];
        new_entry->lru_prev = NULL;
        new_entry->lru_next = inode_cache[hash];

        if (inode_cache[hash]) {
            inode_cache[hash]->lru_prev = new_entry;
        }
        inode_cache[hash] = new_entry;

        vfs_statistics.memory_used += sizeof(inode_cache_entry_t);
    }

    spin_unlock(&inode_cache_lock);
}

void vfs_inode_release(struct inode* inode)
{
    if (!initialized || !inode) return;

    spin_lock(&inode_cache_lock);

    uint32_t hash = (inode->i_ino ^ inode->i_dev) % VFS_MAX_INODE_CACHE;
    inode_cache_entry_t* entry = inode_cache[hash];

    while (entry) {
        if (entry->inode == inode) {
            entry->ref_count--;
            if (entry->ref_count <= 0 && !entry->dirty) {
                if (entry->hash_next) {
                    entry->hash_next->lru_prev = entry->lru_prev;
                }
                if (entry->lru_prev) {
                    entry->lru_prev->lru_next = entry->hash_next;
                } else {
                    inode_cache[hash] = entry->hash_next;
                }
                kfree(entry);
                vfs_statistics.memory_used -= sizeof(inode_cache_entry_t);
            }
            break;
        }
        entry = entry->hash_next;
    }

    spin_unlock(&inode_cache_lock);
}

struct dentry* vfs_dentry_lookup(const char* name, uint64_t parent_ino)
{
    if (!initialized || !name) return NULL;

    spin_lock(&dentry_cache_lock);

    uint32_t hash = (str_hash(name) ^ parent_ino) % VFS_MAX_DENTRY_CACHE;
    dentry_cache_entry_t* entry = dentry_cache[hash];

    while (entry) {
        if (strcmp(entry->name, name) == 0 && entry->parent_ino == parent_ino) {
            entry->ref_count++;
            entry->last_access_time = get_current_time_ns();
            vfs_statistics.cache_hits++;
            spin_unlock(&dentry_cache_lock);
            return entry->dentry;
        }
        entry = entry->hash_next;
    }

    vfs_statistics.cache_misses++;
    spin_unlock(&dentry_cache_lock);

    return NULL;
}

void vfs_dentry_cache(struct dentry* dentry, const char* name, uint64_t parent_ino)
{
    if (!initialized || !dentry || !name) return;

    spin_lock(&dentry_cache_lock);

    uint32_t hash = (str_hash(name) ^ parent_ino) % VFS_MAX_DENTRY_CACHE;
    dentry_cache_entry_t* new_entry = kzalloc(sizeof(dentry_cache_entry_t));

    if (new_entry) {
        strncpy(new_entry->name, name, 255);
        new_entry->parent_ino = parent_ino;
        new_entry->dentry = dentry;
        new_entry->ref_count = 1;
        new_entry->last_access_time = get_current_time_ns();
        new_entry->hash_next = dentry_cache[hash];
        dentry_cache[hash] = new_entry;

        vfs_statistics.memory_used += sizeof(dentry_cache_entry_t);
    }

    spin_unlock(&dentry_cache_lock);
}

int vfs_open_enhanced(const char* pathname, int flags, mode_t mode)
{
    if (!initialized || !pathname) return -EINVAL;

    spin_lock(&file_table_lock);

    int fd = -1;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (file_handles[i].fd == -1) {
            fd = next_fd++;
            file_handles[i].fd = fd;
            break;
        }
    }

    if (fd < 0) {
        spin_unlock(&file_table_lock);
        return -EMFILE;
    }

    file_handle_t* fh = &file_handles[fd % VFS_MAX_OPEN_FILES];
    fh->flags = flags;
    fh->mode = mode;
    fh->position = 0;
    fh->ref_count = 1;
    fh->dirty = false;
    fh->read_count = 0;
    fh->write_count = 0;
    fh->seek_count = 0;
    fh->open_time = get_current_time_ns();
    fh->last_access_time = fh->open_time;

    if (flags & O_RDONLY || flags & O_RDWR) {
        fh->read_ahead_buffer = kzalloc(VFS_READ_AHEAD_SIZE);
        fh->read_ahead_size = VFS_READ_AHEAD_SIZE;
        fh->read_ahead_offset = 0;
        vfs_statistics.memory_used += VFS_READ_AHEAD_SIZE;
    } else {
        fh->read_ahead_buffer = NULL;
        fh->read_ahead_size = 0;
    }

    if (flags & O_WRONLY || flags & O_RDWR) {
        fh->write_buffer = kzalloc(VFS_WRITE_BUFFER_SIZE);
        fh->write_buffer_size = VFS_WRITE_BUFFER_SIZE;
        fh->write_buffer_pos = 0;
        fh->write_sync_position = 0;
        vfs_statistics.memory_used += VFS_WRITE_BUFFER_SIZE;
    } else {
        fh->write_buffer = NULL;
        fh->write_buffer_size = 0;
    }

    strncpy(fh->path, pathname, VFS_MAX_PATH_LENGTH - 1);

    struct inode* inode = vfs_namei(pathname);
    if (!inode && !(flags & O_CREAT)) {
        if (fh->read_ahead_buffer) kfree(fh->read_ahead_buffer);
        if (fh->write_buffer) kfree(fh->write_buffer);
        memset(fh, 0, sizeof(file_handle_t));
        fh->fd = -1;
        spin_unlock(&file_table_lock);
        return -ENOENT;
    }

    if (!inode && (flags & O_CREAT)) {
        inode = vfs_create(pathname, mode);
        if (!inode) {
            if (fh->read_ahead_buffer) kfree(fh->read_ahead_buffer);
            if (fh->write_buffer) kfree(fh->write_buffer);
            memset(fh, 0, sizeof(file_handle_t));
            fh->fd = -1;
            spin_unlock(&file_table_lock);
            return -EACCES;
        }
    }

    fh->inode = inode;
    vfs_inode_cache(inode);

    if (flags & O_TRUNC) {
        vfs_truncate(inode, 0);
    }

    if (flags & O_APPEND) {
        fh->position = inode->i_size;
    }

    vfs_statistics.total_opens++;
    vfs_statistics.current_open_files++;

    spin_unlock(&file_table_lock);
    return fd;
}

ssize_t vfs_read_enhanced(int fd, void* buf, size_t count)
{
    if (!initialized || !buf || count == 0) return -EINVAL;

    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || file_handles[fd].fd == -1) {
        return -EBADF;
    }

    file_handle_t* fh = &file_handles[fd];
    spin_lock(&fh->lock);

    if (!(fh->flags & O_RDONLY) && !(fh->flags & O_RDWR)) {
        spin_unlock(&fh->lock);
        return -EBADF;
    }

    uint64_t start_time = get_current_time_ns();

    ssize_t total_read = 0;
    uint8_t* buffer = (uint8_t*)buf;

    if (fh->read_ahead_buffer && fh->position >= fh->read_ahead_offset &&
        fh->position < fh->read_ahead_offset + fh->read_ahead_size) {

        size_t cached_available = fh->read_ahead_offset + fh->read_ahead_size - fh->position;
        size_t to_copy = (count < cached_available) ? count : cached_available;

        memcpy(buffer, fh->read_ahead_buffer + (fh->position - fh->read_ahead_offset), to_copy);
        total_read = to_read;
        buffer += to_copy;
        count -= to_copy;
        fh->position += to_copy;

        vfs_statistics.read_ahead_hits++;
    }

    while (count > 0) {
        ssize_t bytes = vfs_read(fh->inode, buffer, count, fh->position);
        if (bytes <= 0) break;

        total_read += bytes;
        buffer += bytes;
        count -= bytes;
        fh->position += bytes;

        if (fh->read_ahead_buffer && bytes > 0) {
            size_t ahead_size = (VFS_READ_AHEAD_SIZE < count * 2) ?
                               VFS_READ_AHEAD_SIZE : count * 2;
            ssize_t ahead_bytes = vfs_read(fh->inode, fh->read_ahead_buffer,
                                          ahead_size, fh->position);
            if (ahead_bytes > 0) {
                fh->read_ahead_offset = fh->position;
                fh->read_ahead_size = ahead_bytes;
            }
        }
    }

    fh->read_count++;
    fh->last_access_time = get_current_time_ns();

    uint64_t elapsed = get_current_time_ns() - start_time;
    vfs_statistics.avg_read_latency_ns =
        (vfs_statistics.avg_read_latency_ns + elapsed) / 2;
    vfs_statistics.total_reads++;
    vfs_statistics.bytes_read += total_read;

    spin_unlock(&fh->lock);
    return total_read;
}

ssize_t vfs_write_enhanced(int fd, const void* buf, size_t count)
{
    if (!initialized || !buf || count == 0) return -EINVAL;

    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || file_handles[fd].fd == -1) {
        return -EBADF;
    }

    file_handle_t* fh = &file_handles[fd];
    spin_lock(&fh->lock);

    if (!(fh->flags & O_WRONLY) && !(fh->flags & O_RDWR)) {
        spin_unlock(&fh->lock);
        return -EBADF;
    }

    uint64_t start_time = get_current_time_ns();

    ssize_t total_written = 0;
    const uint8_t* buffer = (const uint8_t*)buf;

    if (fh->write_buffer) {
        size_t space_remaining = fh->write_buffer_size - fh->write_buffer_pos;
        size_t to_buffer = (count < space_remaining) ? count : space_remaining;

        memcpy(fh->write_buffer + fh->write_buffer_pos, buffer, to_buffer);
        fh->write_buffer_pos += to_buffer;
        fh->dirty = true;

        total_written = to_buffer;
        buffer += to_buffer;
        count -= to_buffer;

        if (fh->write_buffer_pos >= fh->write_buffer_size) {
            ssize_t flushed = vfs_write(fh->inode, fh->write_buffer,
                                       fh->write_buffer_pos, fh->write_sync_position);
            if (flushed > 0) {
                fh->write_sync_position += flushed;
                fh->write_buffer_pos = 0;
                fh->dirty = false;
                vfs_statistics.write_buffer_flushes++;
            }
        }
    }

    if (count > 0) {
        ssize_t written = vfs_write(fh->inode, buffer, count, fh->position);
        if (written > 0) {
            total_written += written;
            fh->position += written;
        }
    }

    fh->write_count++;
    fh->last_access_time = get_current_time_ns();

    uint64_t elapsed = get_current_time_ns() - start_time;
    vfs_statistics.avg_write_latency_ns =
        (vfs_statistics.avg_write_latency_ns + elapsed) / 2;
    vfs_statistics.total_writes++;
    vfs_statistics.bytes_written += total_written;

    spin_unlock(&fh->lock);
    return total_written;
}

off_t vfs_lseek_enhanced(int fd, off_t offset, int whence)
{
    if (!initialized) return -EINVAL;

    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || file_handles[fd].fd == -1) {
        return -EBADF;
    }

    file_handle_t* fh = &file_handles[fd];
    spin_lock(&fh->lock);

    off_t new_position;

    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = fh->position + offset;
            break;
        case SEEK_END:
            new_position = fh->inode->i_size + offset;
            break;
        default:
            spin_unlock(&fh->lock);
            return -EINVAL;
    }

    if (new_position < 0) new_position = 0;

    fh->position = new_position;
    fh->seek_count++;

    if (fh->read_ahead_buffer) {
        fh->read_ahead_offset = 0;
        fh->read_ahead_size = 0;
    }

    spin_unlock(&fh->lock);
    return new_position;
}

int vfs_close_enhanced(int fd)
{
    if (!initialized) return -EINVAL;

    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || file_handles[fd].fd == -1) {
        return -EBADF;
    }

    file_handle_t* fh = &file_handles[fd];
    spin_lock(&fh->lock);

    if (fh->dirty && fh->write_buffer && fh->write_buffer_pos > 0) {
        vfs_write(fh->inode, fh->write_buffer, fh->write_buffer_pos,
                 fh->write_sync_position);
        vfs_statistics.write_buffer_flushes++;
    }

    if (fh->inode) {
        vfs_inode_release(fh->inode);
    }

    if (fh->read_ahead_buffer) {
        kfree(fh->read_ahead_buffer);
        vfs_statistics.memory_used -= VFS_READ_AHEAD_SIZE;
    }

    if (fh->write_buffer) {
        kfree(fh->write_buffer);
        vfs_statistics.memory_used -= VFS_WRITE_BUFFER_SIZE;
    }

    vfs_statistics.total_closes++;
    vfs_statistics.current_open_files--;

    memset(fh, 0, sizeof(file_handle_t));
    fh->fd = -1;

    spin_unlock(&fh->lock);
    return 0;
}

int vfs_fsync_enhanced(int fd)
{
    if (!initialized) return -EINVAL;

    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || file_handles[fd].fd == -1) {
        return -EBADF;
    }

    file_handle_t* fh = &file_handles[fd];
    spin_lock(&fh->lock);

    int result = 0;

    if (fh->dirty && fh->write_buffer && fh->write_buffer_pos > 0) {
        ssize_t flushed = vfs_write(fh->inode, fh->write_buffer,
                                   fh->write_buffer_pos, fh->write_sync_position);
        if (flushed > 0) {
            fh->write_sync_position += flushed;
            fh->write_buffer_pos = 0;
            fh->dirty = false;
        } else {
            result = -EIO;
        }
    }

    if (result == 0) {
        result = vfs_fsync(fh->inode);
    }

    vfs_statistics.sync_operations++;
    spin_unlock(&fh->lock);
    return result;
}

char* vfs_realpath_enhanced(const char* path, char* resolved_path)
{
    if (!initialized || !path || !resolved_path) return NULL;

    vfs_statistics.path_resolutions++;

    char temp_path[VFS_MAX_PATH_LENGTH];
    strncpy(temp_path, path, VFS_MAX_PATH_LENGTH - 1);

    if (temp_path[0] != '/') {
        if (getcwd(temp_path, VFS_MAX_PATH_LENGTH) == NULL) {
            return NULL;
        }
        strncat(temp_path, "/", VFS_MAX_PATH_LENGTH - strlen(temp_path) - 1);
        strncat(temp_path, path, VFS_MAX_PATH_LENGTH - strlen(temp_path) - 1);
    }

    char* components[VFS_MAX_PATH_LENGTH / 2];
    int component_count = 0;
    char* saveptr;
    char* token = strtok_r(temp_path, "/", &saveptr);

    while (token && component_count < VFS_MAX_PATH_LENGTH / 2) {
        if (strcmp(token, "..") == 0) {
            if (component_count > 0) {
                component_count--;
            }
        } else if (strcmp(token, ".") != 0) {
            components[component_count++] = token;
        }
        token = strtok_r(NULL, "/", &saveptr);
    }

    resolved_path[0] = '/';
    resolved_path[1] = '\0';

    for (int i = 0; i < component_count; i++) {
        strncat(resolved_path, components[i], VFS_MAX_PATH_LENGTH - strlen(resolved_path) - 1);
        if (i < component_count - 1) {
            strncat(resolved_path, "/", VFS_MAX_PATH_LENGTH - strlen(resolved_path) - 1);
        }
    }

    return resolved_path;
}

DIR* vfs_opendir_enhanced(const char* name)
{
    if (!initialized || !name) return NULL;

    DIR* dir = kzalloc(sizeof(DIR));
    if (!dir) return NULL;

    dir->offset = 0;
    dir->size = 0;

    uint32_t hash = str_hash(name) % VFS_DIR_CACHE_SIZE;
    dir_cache_entry_t* cache = &dir_cache[hash];

    spin_lock(&cache->lock);

    if (cache->valid && strcmp(cache->path, name) == 0) {
        if (cache->entries) {
            dir->entries = cache->entries;
            dir->size = cache->entry_count;
            cache->access_count++;
            cache->access_time = get_current_time_ns();
            spin_unlock(&cache->lock);
            return dir;
        }
    }

    spin_unlock(&cache->lock);

    struct inode* dir_inode = vfs_namei(name);
    if (!dir_inode || !S_ISDIR(dir_inode->i_mode)) {
        kfree(dir);
        return NULL;
    }

    int capacity = 64;
    struct dirent* entries = kzalloc(sizeof(struct dirent) * capacity);
    if (!entries) {
        kfree(dir);
        return NULL;
    }

    int count = 0;
    off_t pos = 0;
    struct dirent* entry;

    while ((entry = vfs_readdir(dir_inode, &pos)) != NULL) {
        if (count >= capacity) {
            capacity *= 2;
            struct dirent* new_entries = krealloc(entries, sizeof(struct dirent) * capacity);
            if (!new_entries) break;
            entries = new_entries;
        }
        entries[count++] = *entry;
    }

    dir->entries = entries;
    dir->size = count;

    spin_lock(&cache->lock);

    if (cache->entries) kfree(cache->entries);

    strncpy(cache->path, name, VFS_MAX_PATH_LENGTH - 1);
    cache->entries = entries;
    cache->entry_count = count;
    cache->capacity = capacity;
    cache->cache_time = get_current_time_ns();
    cache->access_time = cache->cache_time;
    cache->access_count = 1;
    cache->valid = true;

    spin_unlock(&cache->lock);

    return dir;
}

int vfs_mount_enhanced(const char* source, const char* target,
                      const char* filesystemtype, unsigned long mountflags,
                      const void* data)
{
    if (!initialized || !source || !target || !filesystemtype) return -EINVAL;

    spin_lock(&vfs_global_lock);

    int slot = -1;
    for (int i = 0; i < VFS_MAX_MOUNT_POINTS; i++) {
        if (!mount_points[i].mounted) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        spin_unlock(&vfs_global_lock);
        return -ENODEV;
    }

    mount_point_t* mp = &mount_points[slot];
    mp->mount_id = next_mount_id++;
    strncpy(mp->source, source, 255);
    strncpy(mp->target, target, 255);
    strncpy(mp->filesystem_type, filesystemtype, 31);
    mp->mount_flags = mountflags;
    mp->mounted = true;
    mp->mount_time = get_current_time_ns();
    mp->access_count = 0;

    int result = vfs_do_mount(source, target, filesystemtype, mountflags, data, &mp->sb);

    if (result != 0) {
        memset(mp, 0, sizeof(mount_point_t));
        mp->mount_id = 0;
        spin_unlock(&vfs_global_lock);
        return result;
    }

    vfs_statistics.mount_operations++;
    vfs_statistics.active_mount_points++;

    spin_unlock(&vfs_global_lock);
    return 0;
}

int vfs_umount_enhanced(const char* target)
{
    if (!initialized || !target) return -EINVAL;

    spin_lock(&vfs_global_lock);

    for (int i = 0; i < VFS_MAX_MOUNT_POINTS; i++) {
        mount_point_t* mp = &mount_points[i];
        if (mp->mounted && strcmp(mp->target, target) == 0) {
            int result = vfs_do_umount(mp->sb);
            if (result != 0) {
                spin_unlock(&vfs_global_lock);
                return result;
            }

            memset(mp, 0, sizeof(mount_point_t));
            mp->mount_id = 0;
            vfs_statistics.mount_operations++;
            vfs_statistics.active_mount_points--;

            spin_unlock(&vfs_global_lock);
            return 0;
        }
    }

    spin_unlock(&vfs_global_lock);
    return -EINVAL;
}

void vfs_flush_all_buffers(void)
{
    if (!initialized) return;

    spin_lock(&file_table_lock);

    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        file_handle_t* fh = &file_handles[i];
        if (fh->fd == -1) continue;

        spin_lock(&fh->lock);

        if (fh->dirty && fh->write_buffer && fh->write_buffer_pos > 0) {
            vfs_write(fh->inode, fh->write_buffer, fh->write_buffer_pos,
                     fh->write_sync_position);
            fh->write_sync_position += fh->write_buffer_pos;
            fh->write_buffer_pos = 0;
            fh->dirty = false;
            vfs_statistics.write_buffer_flushes++;
        }

        spin_unlock(&fh->lock);
    }

    spin_unlock(&file_table_lock);
}

void vfs_invalidate_inode_cache(void)
{
    if (!initialized) return;

    spin_lock(&inode_cache_lock);

    for (int i = 0; i < VFS_MAX_INODE_CACHE; i++) {
        inode_cache_entry_t* entry = inode_cache[i];
        while (entry) {
            inode_cache_entry_t* next = entry->hash_next;
            if (!entry->dirty && entry->ref_count <= 0) {
                kfree(entry);
            }
            entry = next;
        }
        inode_cache[i] = NULL;
    }

    spin_unlock(&inode_cache_lock);
}

void vfs_invalidate_dentry_cache(void)
{
    if (!initialized) return;

    spin_lock(&dentry_cache_lock);

    for (int i = 0; i < VFS_MAX_DENTRY_CACHE; i++) {
        dentry_cache_entry_t* entry = dentry_cache[i];
        while (entry) {
            dentry_cache_entry_t* next = entry->hash_next;
            if (entry->ref_count <= 0) {
                kfree(entry);
            }
            entry = next;
        }
        dentry_cache[i] = NULL;
    }

    spin_unlock(&dentry_cache_lock);
}

void vfs_invalidate_dir_cache(void)
{
    if (!initialized) return;

    for (int i = 0; i < VFS_DIR_CACHE_SIZE; i++) {
        spin_lock(&dir_cache[i].lock);

        if (dir_cache[i].entries) {
            kfree(dir_cache[i].entries);
            dir_cache[i].entries = NULL;
        }
        dir_cache[i].valid = false;
        dir_cache[i].entry_count = 0;

        spin_unlock(&dir_cache[i].lock);
    }
}

int vfs_get_stats(vfs_stats_t* stats)
{
    if (!stats || !initialized) return -EINVAL;

    memcpy(stats, &vfs_statistics, sizeof(vfs_stats_t));

    if (vfs_statistics.cache_hits + vfs_statistics.cache_misses > 0) {
        stats->cache_hit_ratio =
            (double)vfs_statistics.cache_hits /
            (double)(vfs_statistics.cache_hits + vfs_statistics.cache_misses);
    } else {
        stats->cache_hit_ratio = 0.0;
    }

    return 0;
}

void vfs_dump_info(void)
{
    vfs_stats_t stats;
    vfs_get_stats(&stats);

    printk("Enhanced VFS Statistics:\n");
    printk("  Total reads:           %llu\n", stats.total_reads);
    printk("  Total writes:          %llu\n", stats.total_writes);
    printk("  Total opens:           %llu\n", stats.total_opens);
    printk("  Total closes:          %llu\n", stats.total_closes);
    printk("  Total seeks:           %llu\n", stats.total_seeks);
    printk("  Cache hits:            %llu\n", stats.cache_hits);
    printk("  Cache misses:          %llu\n", stats.cache_misses);
    printk("  Cache hit ratio:       %.2f%%\n", stats.cache_hit_ratio * 100.0);
    printk("  Read-ahead hits:       %llu\n", stats.read_ahead_hits);
    printk("  Write buffer flushes:  %llu\n", stats.write_buffer_flushes);
    printk("  Inode lookups:         %llu\n", stats.inode_lookups);
    printk("  Dentry lookups:        %llu\n", stats.dentry_lookups);
    printk("  Path resolutions:      %llu\n", stats.path_resolutions);
    printk("  Mount operations:      %llu\n", stats.mount_operations);
    printk("  Sync operations:       %llu\n", stats.sync_operations);
    printk("  Bytes read:            %llu KB\n", stats.bytes_read / 1024ULL);
    printk("  Bytes written:         %llu KB\n", stats.bytes_written / 1024ULL);
    printk("  Avg read latency:      %.2f us\n", stats.avg_read_latency_ns / 1000.0);
    printk("  Avg write latency:     %.2f us\n", stats.avg_write_latency_ns / 1000.0);
    printk("  Current open files:    %llu\n", stats.current_open_files);
    printk("  Active mount points:   %d\n", stats.active_mount_points);
    printk("  Memory used:           %llu KB\n", stats.memory_used / 1024ULL);

    printk("\nActive Mount Points:\n");
    for (int i = 0; i < VFS_MAX_MOUNT_POINTS; i++) {
        if (mount_points[i].mounted) {
            printk("  [%d] %s on %s (%s) flags=0x%lx accesses=%llu\n",
                   mount_points[i].mount_id,
                   mount_points[i].source,
                   mount_points[i].target,
                   mount_points[i].filesystem_type,
                   mount_points[i].mount_flags,
                   mount_points[i].access_count);
        }
    }
}