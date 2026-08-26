#ifndef ARCH_X86_64_FS_H
#define ARCH_X86_64_FS_H

#include <arch/types.h>

#define FS_MAX_FILES         256
#define FS_MAX_NAME          256

typedef struct {
    char     name[FS_MAX_NAME];
    uint64_t inode;
    uint64_t size;
    uint64_t offset;
    uint64_t flags;
} fs_file_t;
#define FS_MAX_OPEN_FDS      1024
#define FS_MAX_MOUNTS        16
#define FS_MAX_TYPES         8

#define FS_O_RDONLY          0x0000
#define FS_O_WRONLY          0x0001
#define FS_O_RDWR            0x0002
#define FS_O_CREAT           0x0040
#define FS_O_TRUNC           0x0200
#define FS_O_APPEND          0x0400
#define FS_O_NONBLOCK        0x0800
#define FS_O_DIRECTORY       0x1000

#define FS_TYPE_REGULAR       1
#define FS_TYPE_DIRECTORY     2
#define FS_TYPE_CHARDEV       3
#define FS_TYPE_BLOCKDEV      4
#define FS_TYPE_FIFO          5
#define FS_TYPE_SYMLINK       6
#define FS_TYPE_SOCKET        7

typedef struct vfs_node {
    uint64_t inode;
    char     name[FS_MAX_NAME];
    uint64_t size;
    uint64_t type;
    uint64_t mode;
    uid_t    uid;
    gid_t    gid;
    uint64_t nlink;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint64_t blksize;
    uint64_t blocks;

    int (*read)(struct vfs_node* node, uint64_t offset, void* buf, uint64_t size);
    int (*write)(struct vfs_node* node, uint64_t offset, const void* buf, uint64_t size);
    int (*open)(struct vfs_node* node, int flags);
    int (*close)(struct vfs_node* node);
    struct vfs_node* (*finddir)(struct vfs_node* node, const char* name);
    int (*readdir)(struct vfs_node* node, int index, char* name, uint64_t max_len);
    int (*mkdir)(struct vfs_node* parent, const char* name, uint64_t mode);
    int (*unlink)(struct vfs_node* parent, const char* name);

    void* impl_data;

    struct vfs_node* children;
    struct vfs_node* next;
    struct vfs_node* parent;
} vfs_node_t;

typedef struct {
    vfs_node_t* node;
    uint64_t    offset;
    int         flags;
    int         ref_count;
} open_file_t;

typedef struct {
    char         mountpoint[256];
    vfs_node_t*  root;
    int          mounted;
    char         fstype[32];
} mount_point_t;

typedef struct {
    char name[32];
    vfs_node_t* (*mount)(const char* source, const char* target);
    int (*unmount)(vfs_node_t* root);
} fs_driver_t;

typedef struct {
    open_file_t files[FS_MAX_OPEN_FDS];
    int next_fd;
} file_table_t;

void fs_init(void);
int  fs_open(const char* name);
int  fs_close(int fd);
int  fs_read(int fd, void* buffer, uint64_t size);
int  fs_write(int fd, const void* buffer, uint64_t size);
int  fs_seek(int fd, uint64_t offset);

int  vfs_open(const char* path, int flags, int mode);
int  vfs_close(int fd);
int  vfs_read(int fd, void* buf, uint64_t count);
int  vfs_write(int fd, const void* buf, uint64_t count);
int  vfs_lseek(int fd, int64_t offset, int whence);
int  vfs_stat(const char* path, void* statbuf);
int  vfs_mkdir(const char* path, int mode);
int  vfs_rmdir(const char* path);
int  vfs_unlink(const char* path);
int  vfs_rename(const char* oldpath, const char* newpath);
int  vfs_mount(const char* source, const char* target, const char* fstype);
int  vfs_umount(const char* target);

int  vfs_register_driver(const fs_driver_t* driver);
vfs_node_t* vfs_create_node(const char* name, uint64_t type);
void vfs_add_child(vfs_node_t* parent, vfs_node_t* child);

int  vfs_chmod(const char* path, uint64_t mode);
int  vfs_chown(const char* path, uid_t uid, gid_t gid);
int  vfs_symlink(const char* target, const char* linkpath);
int  vfs_readlink(const char* path, char* buf, uint64_t bufsize);
int  vfs_truncate(const char* path, uint64_t length);
int  vfs_fsync(int fd);
int  vfs_fstat(int fd, void* statbuf);
int  vfs_access(const char* path, int mode);
int  vfs_chdir(const char* path);
int  vfs_getcwd(char* buf, uint64_t size);
int  vfs_dup(int fd);
int  vfs_dup2(int oldfd, int newfd);
int  vfs_pipe(int pipefd[2]);
int  vfs_ioctl(int fd, uint64_t request, void* arg);

vfs_node_t* vfs_find_path(const char* path);

extern vfs_node_t* vfs_root;

extern mount_point_t mount_points[FS_MAX_MOUNTS];
extern int mount_count;

#endif