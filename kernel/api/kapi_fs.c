

#include "kapi_fs.h"
#include "kapi.h"

#include <arch/fs.h>
#include <fs.h>
#include <arch/memory.h>
#include <string.h>

struct kapi_file {
    int fd;
    int valid;
};

struct kapi_dir {
    int valid;
    char path[256];
    int index;
};

#define KAPI_MAX_OPEN_FILES 256
static struct kapi_file kapi_file_table[KAPI_MAX_OPEN_FILES];
static struct kapi_dir  kapi_dir_table[KAPI_MAX_OPEN_FILES];
static int kapi_fs_initialized = 0;

/* 从路径中分离出父目录和文件名 */
static int split_path(const char* path, char* parent, char* name, size_t max_len)
{
    if (!path || !parent || !name || max_len == 0) {
        return -1;
    }

    const char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        return -1;
    }

    if (last_slash == path) {
        strncpy(parent, "/", max_len - 1);
        parent[max_len - 1] = '\0';
        if (strlen(path) > 1) {
            strncpy(name, path + 1, max_len - 1);
            name[max_len - 1] = '\0';
        } else {
            name[0] = '\0';
        }
    } else {
        size_t parent_len = (size_t)(last_slash - path);
        if (parent_len >= max_len) {
            parent_len = max_len - 1;
        }
        memcpy(parent, path, parent_len);
        parent[parent_len] = '\0';
        strncpy(name, last_slash + 1, max_len - 1);
        name[max_len - 1] = '\0';
    }
    return 0;
}

/* 在 VFS 树中查找指定路径的节点 */
static vfs_node_t* vfs_find_path(const char* path)
{
    if (!path || !vfs_root) {
        return NULL;
    }
    if (path[0] != '/') {
        return NULL;
    }

    if (strcmp(path, "/") == 0) {
        return vfs_root;
    }

    char temp[256];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    vfs_node_t* node = vfs_root;
    char* p = temp + 1;  /* 跳过开头的 '/' */

    while (*p) {
        char* token = p;
        while (*p && *p != '/') {
            p++;
        }
        if (*p == '/') {
            *p = '\0';
            p++;
        }

        if (node->finddir) {
            node = node->finddir(node, token);
            if (!node) {
                return NULL;
            }
        } else {
            return NULL;
        }

        while (*p == '/') {
            p++;
        }
    }

    return node;
}

int kapi_fs_init(void)
{
    if (kapi_fs_initialized) {
        return KAPI_OK;
    }

    fs_init();
    memset(kapi_file_table, 0, sizeof(kapi_file_table));
    memset(kapi_dir_table, 0, sizeof(kapi_dir_table));
    kapi_fs_initialized = 1;
    return KAPI_OK;
}

kapi_file_t kapi_open(const char* path, int flags, int mode)
{
    if (!path) {
        return NULL;
    }

    int slot = -1;
    for (int i = 0; i < KAPI_MAX_OPEN_FILES; i++) {
        if (!kapi_file_table[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    int fd = fs_open(path);
    if (fd < 0) {
        return NULL;
    }

    kapi_file_table[slot].fd = fd;
    kapi_file_table[slot].valid = 1;
    return &kapi_file_table[slot];
}

int kapi_close(kapi_file_t file)
{
    if (!file || !file->valid) {
        return KAPI_EINVAL;
    }

    int ret = fs_close(file->fd);
    file->valid = 0;
    file->fd = -1;

    return ret == 0 ? KAPI_OK : KAPI_ERROR;
}

int64_t kapi_read(kapi_file_t file, void* buf, size_t count)
{
    if (!file || !file->valid || !buf) {
        return KAPI_EINVAL;
    }

    return (int64_t)fs_read(file->fd, buf, count);
}

int64_t kapi_write(kapi_file_t file, const void* buf, size_t count)
{
    if (!file || !file->valid || !buf) {
        return KAPI_EINVAL;
    }

    return (int64_t)fs_write(file->fd, buf, count);
}

int64_t kapi_seek(kapi_file_t file, int64_t offset, int whence)
{
    if (!file || !file->valid) {
        return KAPI_EINVAL;
    }

    return (int64_t)fs_seek(file->fd, (uint64_t)offset);
}

int kapi_stat(const char* path, kapi_file_stat_t* stat)
{
    if (!path || !stat) {
        return KAPI_EINVAL;
    }

    vfs_node_t* node = vfs_find_path(path);
    if (!node) {
        return KAPI_ENOENT;
    }

    memset(stat, 0, sizeof(*stat));
    stat->mode = (uint32_t)node->mode;
    stat->size = node->size;
    stat->atime = node->atime;
    stat->mtime = node->mtime;
    stat->ctime = node->ctime;
    stat->blocks = node->blocks;
    stat->blksize = (uint32_t)node->blksize;
    return KAPI_OK;
}

int kapi_unlink(const char* path)
{
    if (!path) {
        return KAPI_EINVAL;
    }

    /* 优先尝试全局 vfs_unlink，如不存在则通过父节点函数指针调用 */
    char parent_path[256];
    char name[256];
    if (split_path(path, parent_path, name, sizeof(parent_path)) != 0) {
        return KAPI_EINVAL;
    }

    vfs_node_t* parent = vfs_find_path(parent_path);
    if (!parent) {
        return KAPI_ENOENT;
    }

    if (parent->unlink) {
        int ret = parent->unlink(parent, name);
        return ret == 0 ? KAPI_OK : KAPI_ERROR;
    }

    return KAPI_ENOSYS;
}

int kapi_rename(const char* oldpath, const char* newpath)
{
    if (!oldpath || !newpath) {
        return KAPI_EINVAL;
    }

    /* 当前 VFS 未提供全局 rename，通过源父节点的 unlink + 目标父节点的 mkdir 模拟
       实际实现应由具体文件系统支持 */
    (void)oldpath;
    (void)newpath;
    return KAPI_ENOSYS;
}

int kapi_mkdir(const char* path, int mode)
{
    if (!path) {
        return KAPI_EINVAL;
    }

    char parent_path[256];
    char name[256];
    if (split_path(path, parent_path, name, sizeof(parent_path)) != 0) {
        return KAPI_EINVAL;
    }

    vfs_node_t* parent = vfs_find_path(parent_path);
    if (!parent) {
        return KAPI_ENOENT;
    }

    if (parent->mkdir) {
        int ret = parent->mkdir(parent, name, (uint64_t)mode);
        return ret == 0 ? KAPI_OK : KAPI_ERROR;
    }

    return KAPI_ENOSYS;
}

int kapi_rmdir(const char* path)
{
    if (!path) {
        return KAPI_EINVAL;
    }

    /* 通过父节点的 unlink 函数指针删除目录（语义上等价） */
    char parent_path[256];
    char name[256];
    if (split_path(path, parent_path, name, sizeof(parent_path)) != 0) {
        return KAPI_EINVAL;
    }

    vfs_node_t* parent = vfs_find_path(parent_path);
    if (!parent) {
        return KAPI_ENOENT;
    }

    if (parent->unlink) {
        int ret = parent->unlink(parent, name);
        return ret == 0 ? KAPI_OK : KAPI_ERROR;
    }

    return KAPI_ENOSYS;
}

kapi_dir_t kapi_opendir(const char* path)
{
    if (!path) {
        return NULL;
    }

    int slot = -1;
    for (int i = 0; i < KAPI_MAX_OPEN_FILES; i++) {
        if (!kapi_dir_table[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    strncpy(kapi_dir_table[slot].path, path, sizeof(kapi_dir_table[slot].path) - 1);
    kapi_dir_table[slot].path[sizeof(kapi_dir_table[slot].path) - 1] = '\0';
    kapi_dir_table[slot].valid = 1;
    kapi_dir_table[slot].index = 0;

    return &kapi_dir_table[slot];
}

int kapi_readdir(kapi_dir_t dir, kapi_dirent_t* entry)
{
    if (!dir || !dir->valid || !entry) {
        return KAPI_EINVAL;
    }

    vfs_node_t* node = vfs_find_path(dir->path);
    if (!node) {
        return KAPI_ERROR;
    }

    if (!node->readdir) {
        return KAPI_ERROR;
    }

    char name_buf[256];
    int ret = node->readdir(node, dir->index, name_buf, sizeof(name_buf));
    if (ret != 0) {
        return KAPI_ERROR;
    }

    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, name_buf, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';

    /* 尝试查找子节点以获取类型和大小 */
    vfs_node_t* child = NULL;
    if (node->finddir) {
        child = node->finddir(node, name_buf);
    }
    if (child) {
        entry->type = (int)child->type;
        entry->size = child->size;
    } else {
        entry->type = KAPI_FT_UNKNOWN;
        entry->size = 0;
    }

    dir->index++;
    return KAPI_OK;
}

int kapi_closedir(kapi_dir_t dir)
{
    if (!dir || !dir->valid) {
        return KAPI_EINVAL;
    }

    dir->valid = 0;
    return KAPI_OK;
}

static char kapi_cwd[256] = "/";

char* kapi_getcwd(char* buf, size_t size)
{
    if (!buf || size == 0) {
        return NULL;
    }

    strncpy(buf, kapi_cwd, size - 1);
    buf[size - 1] = '\0';
    return buf;
}

int kapi_chdir(const char* path)
{
    if (!path) {
        return KAPI_EINVAL;
    }

    /* 验证路径存在且为目录 */
    vfs_node_t* node = vfs_find_path(path);
    if (!node) {
        return KAPI_ENOENT;
    }
    if (node->type != FS_TYPE_DIRECTORY) {
        return KAPI_EINVAL;
    }

    strncpy(kapi_cwd, path, sizeof(kapi_cwd) - 1);
    kapi_cwd[sizeof(kapi_cwd) - 1] = '\0';
    return KAPI_OK;
}

int kapi_mount(const char* source, const char* target, const char* fstype, uint64_t flags)
{
    if (!source || !target || !fstype) {
        return KAPI_EINVAL;
    }

    const char* name = fstype;
    if (strcmp(name, "ext") == 0) {
        name = "ext4";
    }
    if (strcmp(name, "ext3") == 0) {
        name = "ext3";
    }
    if (strcmp(name, "ext2") == 0) {
        name = "ext2";
    }

    int ret = vfs_mount(source, target, name);
    if (ret == 0) {
        return KAPI_OK;
    }

    ret = fs_mount(name, source, target);
    if (ret == 0) {
        return KAPI_OK;
    }

    (void)flags;
    return KAPI_ERROR;
}

int kapi_umount(const char* target)
{
    if (!target) {
        return KAPI_EINVAL;
    }

    int ret = vfs_umount(target);
    return ret == 0 ? KAPI_OK : KAPI_ERROR;
}
