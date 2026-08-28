#include "fs.h"
#include <arch/fs.h>
#include <arch/ext4.h>
#include <memory.h>
#include <string.h>
#include <slab.h>
#include <stdio.h>

extern int ext2_mount(const char* device, const char* mount_point);
extern int ext2_unmount(const char* mount_point);
extern void ext2_init(void);
extern int ext4_mount(vfs_node_t* mount_point, void* device);
extern int ext4_unmount(ext4_fs_t* fs);
extern void ext4_init(void);

static filesystem_t filesystems[FS_MAX];
static int fs_count = 0;

vfs_node_t* vfs_root = NULL;

mount_point_t mount_points[FS_MAX_MOUNTS];
int mount_count = 0;

static int fs_ext2_mount_adapter(const char* device, const char* mount_point)
{
    return ext2_mount(device, mount_point);
}

static int fs_ext2_unmount_adapter(const char* mount_point)
{
    return ext2_unmount(mount_point);
}

static int fs_ext3_mount_adapter(const char* device, const char* mount_point)
{
    return ext2_mount(device, mount_point);
}

static int fs_ext3_unmount_adapter(const char* mount_point)
{
    return ext2_unmount(mount_point);
}

static int fs_ext4_mount_adapter(const char* device, const char* mount_point)
{
    vfs_node_t* mp = NULL;
    if (mount_point) {
        mp = vfs_find_path(mount_point);
    }
    int ret = ext4_mount(mp, (void*)device);
    if (ret == 0 && mp) {
        if (!vfs_root) {
            vfs_root = mp;
        }
    }
    return ret;
}

static int fs_ext4_unmount_adapter(const char* mount_point)
{
    (void)mount_point;
    return 0;
}

void fs_init(void)
{
    memset(filesystems, 0, sizeof(filesystems));
    fs_count = 0;
    memset(mount_points, 0, sizeof(mount_points));
    mount_count = 0;

    vfs_root = vfs_create_node("/", FS_TYPE_DIRECTORY);
    if (vfs_root) {
        vfs_root->inode = 2;
        vfs_root->mode = 0755;
        vfs_root->size = 0;
        vfs_root->blksize = 4096;
    }

    ext2_init();
    ext4_init();

    filesystem_t fs_ext2 = {"ext2", fs_ext2_mount_adapter, fs_ext2_unmount_adapter};
    filesystem_t fs_ext3 = {"ext3", fs_ext3_mount_adapter, fs_ext3_unmount_adapter};
    filesystem_t fs_ext4 = {"ext4", fs_ext4_mount_adapter, fs_ext4_unmount_adapter};
    filesystem_t fs_ext  = {"ext",  fs_ext4_mount_adapter, fs_ext4_unmount_adapter};

    fs_register(&fs_ext2);
    fs_register(&fs_ext3);
    fs_register(&fs_ext4);
    fs_register(&fs_ext);
}

int fs_register(filesystem_t* fs)
{
    if (!fs || fs_count >= FS_MAX) {
        return -1;
    }

    filesystems[fs_count] = *fs;
    fs_count++;
    return 0;
}

int fs_mount(const char* fs_name, const char* device, const char* mount_point)
{
    for (int i = 0; i < fs_count; i++) {
        if (strcmp(filesystems[i].name, fs_name) == 0) {
            return filesystems[i].mount(device, mount_point);
        }
    }
    return -1;
}

int fs_unmount(const char* mount_point)
{
    if (!mount_point) {
        return -1;
    }

    for (int i = 0; i < fs_count; i++) {
        if (filesystems[i].unmount && strcmp(mount_point, "/") != 0) {
            return filesystems[i].unmount(mount_point);
        }
    }
    return -1;
}

int vfs_register_driver(const fs_driver_t* driver)
{
    (void)driver;
    return 0;
}

vfs_node_t* vfs_find_path(const char* path);

vfs_node_t* vfs_create_node(const char* name, uint64_t type)
{
    vfs_node_t* node = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    if (name) {
        strncpy(node->name, name, sizeof(node->name) - 1);
        node->name[sizeof(node->name) - 1] = '\0';
    }
    node->type = type;
    node->inode = 0;
    node->size = 0;
    node->mode = 0755;
    return node;
}

void vfs_add_child(vfs_node_t* parent, vfs_node_t* child)
{
    if (!parent || !child) return;
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

static open_file_t open_files[FS_MAX_OPEN_FDS];
static int next_fd = 0;

int vfs_open(const char* path, int flags, int mode)
{
    (void)flags; (void)mode;
    vfs_node_t* node = vfs_find_path(path);
    if (!node) return -1;
    if (next_fd >= FS_MAX_OPEN_FDS) return -1;
    int fd = next_fd++;
    open_files[fd].node = node;
    open_files[fd].offset = 0;
    open_files[fd].flags = flags;
    open_files[fd].ref_count = 1;
    if (node->open) node->open(node, flags);
    return fd;
}

int vfs_close(int fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    if (!open_files[fd].node) return -1;
    if (open_files[fd].node->close) open_files[fd].node->close(open_files[fd].node);
    open_files[fd].node = NULL;
    open_files[fd].offset = 0;
    open_files[fd].ref_count = 0;
    return 0;
}

int vfs_read(int fd, void* buf, uint64_t count)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    open_file_t* f = &open_files[fd];
    if (!f->node || !f->node->read) return -1;
    int ret = f->node->read(f->node, f->offset, buf, count);
    if (ret > 0) f->offset += (uint64_t)ret;
    return ret;
}

int vfs_write(int fd, const void* buf, uint64_t count)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    open_file_t* f = &open_files[fd];
    if (!f->node || !f->node->write) return -1;
    int ret = f->node->write(f->node, f->offset, buf, count);
    if (ret > 0) f->offset += (uint64_t)ret;
    return ret;
}

int vfs_lseek(int fd, int64_t offset, int whence)
{
    (void)whence;
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    open_file_t* f = &open_files[fd];
    if (!f->node) return -1;
    if (offset >= 0) f->offset = (uint64_t)offset;
    return (int)f->offset;
}

int fs_open(const char* name)
{
    return vfs_open(name, FS_O_RDONLY, 0);
}

int fs_close(int fd)
{
    return vfs_close(fd);
}

int fs_read(int fd, void* buffer, uint64_t size)
{
    return vfs_read(fd, buffer, size);
}

int fs_write(int fd, const void* buffer, uint64_t size)
{
    return vfs_write(fd, buffer, size);
}

int fs_seek(int fd, uint64_t offset)
{
    return vfs_lseek(fd, (int64_t)offset, 0);
}

int vfs_mount(const char* source, const char* target, const char* fstype)
{
    if (!fstype || !target) return -1;
    if (mount_count >= FS_MAX_MOUNTS) return -1;

    vfs_node_t* target_node = vfs_find_path(target);
    if (!target_node) return -1;

    for (int i = 0; i < fs_count; i++) {
        if (strcmp(filesystems[i].name, fstype) == 0) {
            int ret = filesystems[i].mount(source, target);
            if (ret == 0) {
                strncpy(mount_points[mount_count].mountpoint, target, 255);
                mount_points[mount_count].mountpoint[255] = '\0';
                mount_points[mount_count].root = target_node;
                mount_points[mount_count].mounted = 1;
                strncpy(mount_points[mount_count].fstype, fstype, 31);
                mount_points[mount_count].fstype[31] = '\0';
                mount_count++;
            }
            return ret;
        }
    }
    return -1;
}

int vfs_umount(const char* target)
{
    if (!target) return -1;
    for (int i = 0; i < mount_count; i++) {
        if (strcmp(mount_points[i].mountpoint, target) == 0) {
            for (int j = 0; j < fs_count; j++) {
                if (strcmp(filesystems[j].name, mount_points[i].fstype) == 0) {
                    int ret = filesystems[j].unmount(target);
                    if (ret == 0) {
                        mount_points[i].mounted = 0;
                        if (i < mount_count - 1) {
                            mount_points[i] = mount_points[mount_count - 1];
                        }
                        mount_count--;
                    }
                    return ret;
                }
            }
            return -1;
        }
    }
    return -1;
}

vfs_node_t* vfs_find_path(const char* path)
{
    if (!path || !vfs_root) return NULL;
    if (path[0] != '/') return NULL;
    if (strcmp(path, "/") == 0) return vfs_root;

    char temp[256];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    vfs_node_t* node = vfs_root;
    char* p = temp + 1;
    while (*p) {
        char* token = p;
        while (*p && *p != '/') p++;
        if (*p == '/') *p++ = '\0';
        if (!node->finddir) return NULL;
        node = node->finddir(node, token);
        if (!node) return NULL;
        while (*p == '/') p++;
    }
    return node;
}

static int get_parent_and_name(const char* path, char* parent_path, char* name, int max_len)
{
    if (!path || !parent_path || !name) return -1;
    
    int len = (int)strlen(path);
    if (len == 0 || path[0] != '/') return -1;
    
    if (strcmp(path, "/") == 0) {
        strcpy(parent_path, "/");
        name[0] = '\0';
        return 0;
    }
    
    const char* last_slash = NULL;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = &path[i];
            break;
        }
    }
    
    if (!last_slash) return -1;
    
    int parent_len = (int)(last_slash - path);
    if (parent_len == 0) {
        strcpy(parent_path, "/");
    } else {
        if (parent_len >= max_len) return -1;
        strncpy(parent_path, path, (size_t)parent_len);
        parent_path[parent_len] = '\0';
    }
    
    strncpy(name, last_slash + 1, (size_t)(max_len - 1));
    name[max_len - 1] = '\0';
    
    return 0;
}

int vfs_mkdir(const char* path, int mode)
{
    if (!path || !vfs_root) return -1;
    
    char parent_path[256];
    char name[256];
    
    if (get_parent_and_name(path, parent_path, name, sizeof(name)) < 0) {
        return -1;
    }
    
    if (name[0] == '\0') return -1;
    
    vfs_node_t* parent = vfs_find_path(parent_path);
    if (!parent) return -1;
    
    if (parent->finddir && parent->finddir(parent, name)) {
        return -1;
    }
    
    if (parent->mkdir) {
        return parent->mkdir(parent, name, (uint64_t)mode);
    }
    
    vfs_node_t* node = vfs_create_node(name, FS_TYPE_DIRECTORY);
    if (!node) return -1;
    node->mode = (uint64_t)mode;
    vfs_add_child(parent, node);
    
    return 0;
}

int vfs_rmdir(const char* path)
{
    if (!path || !vfs_root) return -1;
    
    char parent_path[256];
    char name[256];
    
    if (get_parent_and_name(path, parent_path, name, sizeof(name)) < 0) {
        return -1;
    }
    
    if (name[0] == '\0') return -1;
    
    vfs_node_t* parent = vfs_find_path(parent_path);
    if (!parent) return -1;
    
    if (parent->unlink) {
        return parent->unlink(parent, name);
    }
    
    vfs_node_t* prev = NULL;
    vfs_node_t* node = parent->children;
    while (node) {
        if (strcmp(node->name, name) == 0) {
            if (node->children) return -1;
            if (prev) {
                prev->next = node->next;
            } else {
                parent->children = node->next;
            }
            kfree(node);
            return 0;
        }
        prev = node;
        node = node->next;
    }
    
    return -1;
}

int vfs_unlink(const char* path)
{
    if (!path || !vfs_root) return -1;
    
    char parent_path[256];
    char name[256];
    
    if (get_parent_and_name(path, parent_path, name, sizeof(name)) < 0) {
        return -1;
    }
    
    if (name[0] == '\0') return -1;
    
    vfs_node_t* parent = vfs_find_path(parent_path);
    if (!parent) return -1;
    
    if (parent->unlink) {
        return parent->unlink(parent, name);
    }
    
    vfs_node_t* prev = NULL;
    vfs_node_t* node = parent->children;
    while (node) {
        if (strcmp(node->name, name) == 0) {
            if (node->type == FS_TYPE_DIRECTORY) return -1;
            if (prev) {
                prev->next = node->next;
            } else {
                parent->children = node->next;
            }
            kfree(node);
            return 0;
        }
        prev = node;
        node = node->next;
    }
    
    return -1;
}

int vfs_rename(const char* oldpath, const char* newpath)
{
    if (!oldpath || !newpath || !vfs_root) return -1;
    
    vfs_node_t* node = vfs_find_path(oldpath);
    if (!node) return -1;
    
    char old_parent_path[256];
    char old_name[256];
    char new_parent_path[256];
    char new_name[256];
    
    if (get_parent_and_name(oldpath, old_parent_path, old_name, sizeof(old_name)) < 0) {
        return -1;
    }
    
    if (get_parent_and_name(newpath, new_parent_path, new_name, sizeof(new_name)) < 0) {
        return -1;
    }
    
    vfs_node_t* old_parent = vfs_find_path(old_parent_path);
    vfs_node_t* new_parent = vfs_find_path(new_parent_path);
    
    if (!old_parent || !new_parent) return -1;
    
    if (strcmp(old_parent_path, new_parent_path) == 0) {
        strncpy(node->name, new_name, FS_MAX_NAME - 1);
        node->name[FS_MAX_NAME - 1] = '\0';
        return 0;
    }
    
    vfs_node_t* prev = NULL;
    vfs_node_t* curr = old_parent->children;
    while (curr) {
        if (curr == node) {
            if (prev) {
                prev->next = curr->next;
            } else {
                old_parent->children = curr->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    
    strncpy(node->name, new_name, FS_MAX_NAME - 1);
    node->name[FS_MAX_NAME - 1] = '\0';
    node->parent = new_parent;
    node->next = new_parent->children;
    new_parent->children = node;
    
    return 0;
}

int vfs_chmod(const char* path, uint64_t mode)
{
    if (!path) return -1;
    vfs_node_t* node = vfs_find_path(path);
    if (!node) return -1;
    node->mode = mode;
    return 0;
}

int vfs_chown(const char* path, uid_t uid, gid_t gid)
{
    if (!path) return -1;
    vfs_node_t* node = vfs_find_path(path);
    if (!node) return -1;
    node->uid = uid;
    node->gid = gid;
    return 0;
}

int vfs_symlink(const char* target, const char* linkpath)
{
    if (!target || !linkpath) return -1;

    char parent_path[256];
    char name[256];
    if (get_parent_and_name(linkpath, parent_path, name, sizeof(name)) < 0) return -1;
    if (name[0] == '\0') return -1;

    vfs_node_t* parent = vfs_find_path(parent_path);
    if (!parent) return -1;

    vfs_node_t* node = vfs_create_node(name, FS_TYPE_SYMLINK);
    if (!node) return -1;
    node->impl_data = kzalloc(strlen(target) + 1);
    if (node->impl_data) {
        strcpy((char*)node->impl_data, target);
    }
    vfs_add_child(parent, node);
    return 0;
}

int vfs_readlink(const char* path, char* buf, uint64_t bufsize)
{
    if (!path || !buf || bufsize == 0) return -1;
    vfs_node_t* node = vfs_find_path(path);
    if (!node || node->type != FS_TYPE_SYMLINK) return -1;
    if (!node->impl_data) return -1;
    const char* target = (const char*)node->impl_data;
    uint64_t len = strlen(target);
    if (len >= bufsize) len = bufsize - 1;
    memcpy(buf, target, len);
    buf[len] = '\0';
    return (int)len;
}

int vfs_truncate(const char* path, uint64_t length)
{
    if (!path) return -1;
    vfs_node_t* node = vfs_find_path(path);
    if (!node) return -1;
    if (node->type == FS_TYPE_DIRECTORY) return -1;
    node->size = length;
    return 0;
}

int vfs_fsync(int fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    if (!open_files[fd].node) return -1;
    return 0;
}

int vfs_fstat(int fd, void* statbuf)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    open_file_t* f = &open_files[fd];
    if (!f->node) return -1;
    return vfs_stat("/", statbuf);
}

int vfs_access(const char* path, int mode)
{
    if (!path) return -1;
    vfs_node_t* node = vfs_find_path(path);
    if (!node) return -1;
    (void)mode;
    return 0;
}

static char current_working_dir[4096] = "/";

int vfs_chdir(const char* path)
{
    if (!path) return -1;
    vfs_node_t* node;
    if (path[0] == '/') {
        node = vfs_find_path(path);
    } else {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", current_working_dir, path);
        node = vfs_find_path(full);
    }
    if (!node || node->type != FS_TYPE_DIRECTORY) return -1;
    if (path[0] == '/') {
        strncpy(current_working_dir, path, sizeof(current_working_dir) - 1);
        current_working_dir[sizeof(current_working_dir) - 1] = '\0';
    } else {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", current_working_dir, path);
        strncpy(current_working_dir, full, sizeof(current_working_dir) - 1);
        current_working_dir[sizeof(current_working_dir) - 1] = '\0';
    }
    return 0;
}

int vfs_getcwd(char* buf, uint64_t size)
{
    if (!buf || size == 0) return -1;
    uint64_t len = strlen(current_working_dir);
    if (len >= size) return -1;
    strcpy(buf, current_working_dir);
    return (int)len;
}

int vfs_dup(int fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    if (!open_files[fd].node) return -1;
    if (next_fd >= FS_MAX_OPEN_FDS) return -1;
    int newfd = next_fd++;
    open_files[newfd] = open_files[fd];
    open_files[newfd].ref_count = 1;
    return newfd;
}

int vfs_dup2(int oldfd, int newfd)
{
    if (oldfd < 0 || oldfd >= FS_MAX_OPEN_FDS) return -1;
    if (newfd < 0 || newfd >= FS_MAX_OPEN_FDS) return -1;
    if (!open_files[oldfd].node) return -1;
    if (open_files[newfd].node) {
        vfs_close(newfd);
    }
    open_files[newfd] = open_files[oldfd];
    open_files[newfd].ref_count = 1;
    return newfd;
}

int vfs_pipe(int pipefd[2])
{
    if (!pipefd) return -1;
    vfs_node_t* node = vfs_create_node("pipe", FS_TYPE_FIFO);
    if (!node) return -1;
    if (next_fd >= FS_MAX_OPEN_FDS - 1) return -1;
    int fd0 = next_fd++;
    int fd1 = next_fd++;
    open_files[fd0].node = node;
    open_files[fd0].offset = 0;
    open_files[fd0].flags = FS_O_RDONLY;
    open_files[fd0].ref_count = 1;
    open_files[fd1].node = node;
    open_files[fd1].offset = 0;
    open_files[fd1].flags = FS_O_WRONLY;
    open_files[fd1].ref_count = 1;
    pipefd[0] = fd0;
    pipefd[1] = fd1;
    return 0;
}

int vfs_ioctl(int fd, uint64_t request, void* arg)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FDS) return -1;
    if (!open_files[fd].node) return -1;
    (void)request; (void)arg;
    return 0;
}