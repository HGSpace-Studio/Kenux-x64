#ifndef KAPI_VFS_H
#define KAPI_VFS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_mount    kapi_mount_t;
typedef struct kapi_dentry   kapi_dentry_t;
typedef struct kapi_inode    kapi_inode_t;
typedef struct kapi_vfs_sb   kapi_vfs_sb_t;

#define KAPI_VFS_MS_RDONLY      (1ULL << 0)
#define KAPI_VFS_MS_NOSUID      (1ULL << 1)
#define KAPI_VFS_MS_NODEV       (1ULL << 2)
#define KAPI_VFS_MS_NOEXEC      (1ULL << 3)
#define KAPI_VFS_MS_SYNCHRONOUS (1ULL << 4)
#define KAPI_VFS_MS_REMOUNT     (1ULL << 5)
#define KAPI_VFS_MS_BIND        (1ULL << 6)
#define KAPI_VFS_MS_MOVE        (1ULL << 7)
#define KAPI_VFS_MS_NOATIME     (1ULL << 8)
#define KAPI_VFS_MS_NODIRATIME  (1ULL << 9)

#define KAPI_VFS_DT_UNKNOWN      0
#define KAPI_VFS_DT_FIFO         1
#define KAPI_VFS_DT_CHR          2
#define KAPI_VFS_DT_DIR          4
#define KAPI_VFS_DT_BLK          6
#define KAPI_VFS_DT_REG          8
#define KAPI_VFS_DT_LNK         10
#define KAPI_VFS_DT_SOCK        12
#define KAPI_VFS_DT_WHT         14

#define KAPI_VFS_S_IFMT   0170000
#define KAPI_VFS_S_IFSOCK  0140000
#define KAPI_VFS_S_IFLNK   0120000
#define KAPI_VFS_S_IFREG   0100000
#define KAPI_VFS_S_IFBLK   0060000
#define KAPI_VFS_S_IFDIR   0040000
#define KAPI_VFS_S_IFCHR   0020000
#define KAPI_VFS_S_IFIFO   0010000

#define KAPI_VFS_DCACHE_NAME_LEN 256

typedef struct {
    char     name[KAPI_VFS_DCACHE_NAME_LEN];
    uint64_t ino;
    uint32_t type;
    uint32_t namelen;
    uint64_t parent_ino;
} kapi_dentry_info_t;

typedef struct {
    uint64_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t blocks;
    uint32_t blksize;
    uint32_t nlink;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint64_t atime_nsec;
    uint64_t mtime_nsec;
    uint64_t ctime_nsec;
} kapi_inode_info_t;

typedef struct {
    char     dev_name[64];
    char     dir_name[256];
    char     type[32];
    uint64_t flags;
    uint64_t block_size;
    uint64_t maxbytes;
    int      mounted;
} kapi_mount_info_t;

kapi_mount_t* kapi_vfs_kern_mount(const char* dev_name, const char* dir_name,
                                   const char* type, uint64_t flags, void* data);

int kapi_vfs_kern_umount(kapi_mount_t* mnt, uint64_t flags);

int kapi_vfs_umount_tree(kapi_mount_t* mnt, uint64_t flags);

int kapi_vfs_mount_info(kapi_mount_t* mnt, kapi_mount_info_t* info);

kapi_dentry_t* kapi_vfs_dentry_lookup(kapi_dentry_t* parent, const char* name);

kapi_dentry_t* kapi_vfs_dentry_create(kapi_dentry_t* parent, const char* name,
                                       uint32_t type, uint32_t mode);

int kapi_vfs_dentry_delete(kapi_dentry_t* dentry);

int kapi_vfs_dentry_get_info(kapi_dentry_t* dentry, kapi_dentry_info_t* info);

kapi_inode_t* kapi_vfs_dentry_get_inode(kapi_dentry_t* dentry);

void kapi_vfs_dentry_put(kapi_dentry_t* dentry);

kapi_inode_t* kapi_vfs_inode_alloc(kapi_vfs_sb_t* sb, uint32_t mode);

int kapi_vfs_inode_free(kapi_inode_t* inode);

int kapi_vfs_inode_get_info(kapi_inode_t* inode, kapi_inode_info_t* info);

int kapi_vfs_inode_set_attr(kapi_inode_t* inode, uint32_t mode, uint32_t uid,
                             uint32_t gid, uint64_t size);

int kapi_vfs_sync(void);

int kapi_vfs_init(void);

/* ===== Fused LeonOS fs.h (list_dir/stat/readdir/ACL/install) ===== */
#define KAPI_FS_NAME_LEN      128U
#define KAPI_FS_PATH_LEN      256U
#define KAPI_FS_MAX_ENTRIES   64U
#define KAPI_FS_ACL_MAX_ACE   16U
#define KAPI_FS_ACL_VERSION   1U

#define KAPI_FS_TYPE_FILE    1U
#define KAPI_FS_TYPE_DIR     2U
#define KAPI_FS_TYPE_DEVICE  3U

#define KAPI_FS_PERM_READ   0x00000001U
#define KAPI_FS_PERM_WRITE  0x00000002U
#define KAPI_FS_PERM_EXEC   0x00000004U
#define KAPI_FS_PERM_DELETE 0x00000008U
#define KAPI_FS_PERM_MANAGE 0x00000010U
#define KAPI_FS_PERM_FULL   (KAPI_FS_PERM_READ|KAPI_FS_PERM_WRITE| \
                             KAPI_FS_PERM_EXEC|KAPI_FS_PERM_DELETE| \
                             KAPI_FS_PERM_MANAGE)

#define KAPI_FS_ACL_PRINCIPAL_OWNER        1U
#define KAPI_FS_ACL_PRINCIPAL_SYSTEM       2U
#define KAPI_FS_ACL_PRINCIPAL_ADMINISTRATORS 3U
#define KAPI_FS_ACL_PRINCIPAL_USERS        4U
#define KAPI_FS_ACL_PRINCIPAL_EVERYONE     5U

#define KAPI_INSTALL_MAX_DISKS            8U
#define KAPI_INSTALL_DISK_FLAG_BOOT_ROOT  0x00000001U
#define KAPI_INSTALL_DISK_FLAG_TARGET_MOUNTED 0x00000002U

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t size;
} kapi_fs_stat_t;

typedef struct {
    uint32_t type;
    char     name[KAPI_FS_NAME_LEN];
} kapi_fs_dir_entry_t;

typedef struct {
    uint32_t principal;
    uint32_t flags;
    uint32_t permissions;
    uint32_t reserved;
} kapi_fs_acl_ace_t;

typedef struct {
    uint32_t            version;
    uint32_t            owner_uid;
    uint32_t            flags;
    uint32_t            ace_count;
    kapi_fs_acl_ace_t   aces[KAPI_FS_ACL_MAX_ACE];
} kapi_fs_acl_t;

typedef struct {
    uint32_t id;
    uint32_t port;
    uint32_t sector_size;
    uint32_t flags;
    uint64_t sector_count;
    char     name[32];
} kapi_install_disk_t;

int  kapi_fs_list_dir(const char* path, kapi_fs_dir_entry_t* entries,
                      uint32_t capacity, uint32_t* out_count);
int  kapi_fs_stat(const char* path, kapi_fs_stat_t* st);
int  kapi_fs_fstat(int fd, kapi_fs_stat_t* st);
long kapi_fs_lseek(int fd, long offset, int whence);
int  kapi_fs_readdir(int fd, kapi_fs_dir_entry_t* entry);
int  kapi_fs_acl_get(const char* path, kapi_fs_acl_t* acl);
int  kapi_fs_acl_set(const char* path, const kapi_fs_acl_t* acl);
int  kapi_fs_acl_take_ownership(const char* path, kapi_fs_acl_t* acl);
int  kapi_fs_acl_repair(const char* path, kapi_fs_acl_t* acl);
int  kapi_install_list_disks(kapi_install_disk_t* disks, uint32_t capacity,
                             uint32_t* out_count);
int  kapi_install_format_esp(uint32_t disk_id);
int  kapi_install_mount_target(uint32_t disk_id);

/* LeonOS compat aliases */
#define KAPI_FS_ListDir(p,e,c,o)        kapi_fs_list_dir((p),(e),(c),(o))
#define KAPI_FS_Stat(p,s)               kapi_fs_stat((p),(s))
#define KAPI_FS_FStat(fd,s)             kapi_fs_fstat((fd),(s))
#define KAPI_FS_LSeek(fd,o,w)           kapi_fs_lseek((fd),(o),(w))
#define KAPI_FS_ReadDir(fd,e)           kapi_fs_readdir((fd),(e))
#define KAPI_FS_ACLGet(p,a)             kapi_fs_acl_get((p),(a))
#define KAPI_FS_ACLSet(p,a)             kapi_fs_acl_set((p),(a))
#define KAPI_FS_ACLTakeOwnership(p,a)   kapi_fs_acl_take_ownership((p),(a))
#define KAPI_FS_ACLRepair(p,a)          kapi_fs_acl_repair((p),(a))
#define KAPI_Install_ListDisks(d,c,o)   kapi_install_list_disks((d),(c),(o))
#define KAPI_Install_FormatESP(id)      kapi_install_format_esp((id))
#define KAPI_Install_MountTarget(id)    kapi_install_mount_target((id))
typedef kapi_fs_stat_t      KAPI_FS_STAT;
typedef kapi_fs_dir_entry_t KAPI_FS_DIR_ENTRY;
typedef kapi_fs_acl_ace_t   KAPI_FS_ACL_ACE;
typedef kapi_fs_acl_t       KAPI_FS_ACL;
typedef kapi_install_disk_t KAPI_INSTALL_DISK;

#ifdef __cplusplus
}
#endif

#endif