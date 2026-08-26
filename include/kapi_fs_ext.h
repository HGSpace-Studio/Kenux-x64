#ifndef KAPI_FS_EXT_H
#define KAPI_FS_EXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_FS_MAX_PATH       4096
#define KAPI_FS_MAX_NAME       256
#define KAPI_FS_MAX_SYMLINK    1024

#define KAPI_FS_RDONLY         0x0001
#define KAPI_FS_WRONLY         0x0002
#define KAPI_FS_RDWR           0x0004
#define KAPI_FS_NONBLOCK       0x0008
#define KAPI_FS_APPEND         0x0010
#define KAPI_FS_CREAT          0x0020
#define KAPI_FS_TRUNC          0x0040
#define KAPI_FS_EXCL           0x0080
#define KAPI_FS_SYNC           0x0100
#define KAPI_FS_DSYNC          0x0200
#define KAPI_FS_RSYNC          0x0400
#define KAPI_FS_NOFOLLOW       0x0800
#define KAPI_FS_CLOEXEC        0x1000
#define KAPI_FS_DIRECT         0x2000
#define KAPI_FS_LARGEFILE      0x4000
#define KAPI_FS_DIRECTORY      0x8000

#define KAPI_S_IFMT            0170000
#define KAPI_S_IFSOCK          0140000
#define KAPI_S_IFLNK           0120000
#define KAPI_S_IFREG           0100000
#define KAPI_S_IFBLK           0060000
#define KAPI_S_IFDIR           0040000
#define KAPI_S_IFCHR           0020000
#define KAPI_S_IFIFO           0010000
#define KAPI_S_ISUID           0004000
#define KAPI_S_ISGID           0002000
#define KAPI_S_ISVTX           0001000
#define KAPI_S_IRUSR           0000400
#define KAPI_S_IWUSR           0000200
#define KAPI_S_IXUSR           0000100
#define KAPI_S_IRGRP           0000040
#define KAPI_S_IWGRP           0000020
#define KAPI_S_IXGRP           0000010
#define KAPI_S_IROTH           0000004
#define KAPI_S_IWOTH           0000002
#define KAPI_S_IXOTH           0000001

#define KAPI_SEEK_SET          0
#define KAPI_SEEK_CUR          1
#define KAPI_SEEK_END          2
#define KAPI_SEEK_DATA         3
#define KAPI_SEEK_HOLE         4

#define KAPI_AT_FDCWD          -100
#define KAPI_AT_SYMLINK_NOFOLLOW    0x100
#define KAPI_AT_REMOVEDIR           0x200
#define KAPI_AT_EACCESS             0x400
#define KAPI_AT_EMPTY_PATH          0x1000
#define KAPI_AT_STATX_SYNC_AS_STAT  0x0000
#define KAPI_AT_STATX_FORCE_SYNC    0x2000
#define KAPI_AT_STATX_DONT_SYNC     0x4000

typedef uint64_t kapi_ino_t;
typedef int64_t kapi_off_t;
typedef uint64_t kapi_dev_t;
typedef uint32_t kapi_mode_t;
typedef uint32_t kapi_uid_t;
typedef uint32_t kapi_gid_t;

typedef struct {
    kapi_ino_t   st_ino;
    kapi_mode_t  st_mode;
    kapi_nlink_t st_nlink;
    kapi_uid_t   st_uid;
    kapi_gid_t   st_gid;
    kapi_dev_t   st_rdev;
    kapi_off_t   st_size;
    int64_t      st_blksize;
    int64_t      st_blocks;
    int64_t      st_atime;
    int64_t      st_mtime;
    int64_t      st_ctime;
} kapi_stat_t;

typedef struct {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t __spare0[1];
    kapi_ino_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    struct {
        int64_t tv_sec;
        uint32_t tv_nsec;
        int32_t __spare1;
    } stx_atime, stx_btime, stx_ctime, stx_mtime;
    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t __spare2[14];
} kapi_statx_t;

typedef struct {
    uint64_t f_flags;
    uint64_t f_pos;
    kapi_ino_t f_inode;
    kapi_mode_t f_mode;
    int64_t f_count;
    int64_t f_version;
    uint64_t f_size;
    uint64_t f_blksize;
    uint64_t f_blocks;
    uint64_t f_lock;
    uint64_t f_mount_id;
} kapi_fd_stats_t;

typedef struct dirent {
    kapi_ino_t d_ino;
    off_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[KAPI_FS_MAX_NAME];
} kapi_dirent_t;

typedef struct {
    uint64_t total_space;
    uint64_t free_space;
    uint64_t available_space;
    uint64_t total_files;
    uint64_t free_files;
    uint64_t fsid;
    uint32_t block_size;
    uint32_t max_name_len;
    uint32_t fs_type;
    char fs_name[16];
    char mount_point[KAPI_FS_MAX_PATH];
    char device[KAPI_FS_MAX_PATH];
} kapi_fs_stats_t;

typedef struct {
    bool is_file;
    bool is_dir;
    bool is_link;
    bool is_char_dev;
    bool is_block_dev;
    bool is_fifo;
    bool is_socket;
    bool exists;
    bool readable;
    bool writable;
    bool executable;
    kapi_mode_t mode;
    kapi_uid_t uid;
    kapi_gid_t gid;
    kapi_off_t size;
    int64_t atime;
    int64_t mtime;
    int64_t ctime;
    int nlinks;
    kapi_dev_t dev;
    kapi_ino_t inode;
} kapi_file_info_t;

typedef void* kapi_dir_t;

typedef int (*kapi_filter_fn_t)(const kapi_dirent_t* entry, void* user_data);
typedef int (*kapi_callback_fn_t)(const char* path, const kapi_file_info_t* info, void* user_data);

int kapi_open(const char* pathname, int flags);

int kapi_openat(int dirfd, const char* pathname, int flags, ...);

int kapi_creat(const char* pathname, mode_t mode);

int kapi_close(int fd);

ssize_t kapi_read(int fd, void* buf, size_t count);

ssize_t kapi_write(int fd, const void* buf, size_t count);

ssize_t kapi_pread(int fd, void* buf, size_t count, off_t offset);

ssize_t kapi_pwrite(int fd, const void* buf, size_t count, off_t offset);

off_t kapi_lseek(int fd, off_t offset, int whence);

int kapi_ftruncate(int fd, off_t length);

int kapi_truncate(const char* path, off_t length);

int kapi_fsync(int fd);

int kapi_fdatasync(int fd);

int kapi_sync(void);

int kapi_fstat(int fd, kapi_stat_t* statbuf);

int kapi_stat(const char* pathname, kapi_stat_t* statbuf);

int kapi_lstat(const char* pathname, kapi_stat_t* statbuf);

int kapi_fstatat(int dirfd, const char* pathname, kapi_stat_t* statbuf, int flags);

int kapi_statx(int dirfd, const char* pathname, int flags, unsigned int mask, kapi_statx_t* statxbuf);

int kapi_access(const char* pathname, int mode);

int kapi_faccessat(int dirfd, const char* pathname, int mode, int flags);

int kapi_chmod(const char* pathname, mode_t mode);

int kapi_fchmod(int fd, mode_t mode);

int kapi_fchmodat(int dirfd, const char* pathname, mode_t mode, int flags);

int kapi_chown(const char* pathname, uid_t owner, gid_t group);

int kapi_fchown(int fd, uid_t owner, gid_t group);

int kapi_fchownat(int dirfd, const char* pathname, uid_t owner, gid_t group, int flags);

int kapi_utime(const char* filename, const struct utimbuf* times);

int kapi_utimes(const char* filename, const struct timeval times[2]);

int kapi_futimens(int fd, const struct timespec times[2]);

int kapi_utimensat(int dirfd, const char* pathname, const struct timespec times[2], int flags);

int kapi_mkdir(const char* pathname, mode_t mode);

int kapi_mkdirat(int dirfd, const char* pathname, mode_t mode);

int kapi_rmdir(const char* pathname);

int kapi_unlink(const char* pathname);

int kapi_unlinkat(int dirfd, const char* pathname, int flags);

int kapi_rename(const char* oldpath, const char* newpath);

int kapi_renameat(int olddirfd, const char* oldpath, int newdirfd, const char* newpath);

int kapi_renameat2(int olddirfd, const char* oldpath, int newdirfd, const char* newpath, unsigned int flags);

int kapi_link(const char* oldpath, const char* newpath);

int kapi_linkat(int olddirfd, const char* oldpath, int newdirfd, const char* newpath, int flags);

int kapi_symlink(const char* target, const char* linkpath);

int kapi_symlinkat(const char* target, int newdirfd, const char* linkpath);

ssize_t kapi_readlink(const char* pathname, char* buf, size_t bufsiz);

ssize_t kapi_readlinkat(int dirfd, const char* pathname, char* buf, size_t bufsiz);

char* kapi_realpath(const char* path, char* resolved_path);

char* kapi_canonicalize(const char* path, char* resolved_path, size_t size);

kapi_dir_t kapi_opendir(const char* name);

kapi_dir_t kapi_fdopendir(int fd);

int kapi_closedir(kapi_dir_t dirp);

kapi_dirent_t* kapi_readdir(kapi_dir_t dirp);

int kapi_readdir_r(kapi_dir_t dirp, kapi_dirent_t* entry, kapi_dirent_t** result);

void kapi_seekdir(kapi_dir_t dirp, long loc);

long kapi_telldir(kapi_dir_t dirp);

void kapi rewinddir(kapi_dir_t dirp);

int kapi_dirfd(kapi_dir_t dirp);

int kapi_scandir(const char* dirp, kapi_dirent_t*** namelist, kapi_filter_fn_t filter, int(*compar)(const kapi_dirent_t**, const kapi_dirent_t**));

int kapi_alphasort(const kapi_dirent_t** a, const kapi_dirent_t** b);

int kapi_versionsort(const kapi_dirent_t** a, const kapi_dirent_t** b);

int kapi_getdents(unsigned int fd, kapi_dirent_t* dirp, unsigned int count);

int kapi_getdents64(unsigned int fd, kapi_dirent_t* dirp, unsigned int count);

int kapi_chdir(const char* path);

int kapi_fchdir(int fd);

char* kapi_getcwd(char* buf, size_t size);

int kapi_dup(int oldfd);

int kapi_dup2(int oldfd, int newfd);

int kapi_dup3(int oldfd, int newfd, int flags);

int kapi_pipe(int pipefd[2]);

int kapi_pipe2(int pipefd[2], int flags);

int kapi_mkfifo(const char* pathname, mode_t mode);

int kapi_mknod(const char* pathname, mode_t mode, dev_t dev);

mode_t kapi_umask(mode_t mask);

int kapi_chroot(const char* path);

int kapi_mount(const char* source, const char* target, const char* filesystemtype, unsigned long mountflags, const void* data);

int kapi_umount(const char* target);

int kapi_umount2(const char* target, int flags);

int kapi_statfs(const char* path, kapi_fs_stats_t* stat);

int kapi_statvfs(const char* path, kapi_fs_stats_t* stat);

int kapi_fstatfs(int fd, kapi_fs_stats_t* stat);

int kapi_fstatvfs(int fd, kapi_fs_stats_t* stat);

int kapi_get_file_info(const char* path, kapi_file_info_t* info);

int kapi_get_fd_info(int fd, kapi_fd_stats_t* info);

int kapi_walk_tree(const char* root, kapi_callback_fn_t callback, void* user_data, int max_depth);

int kapi_find_file(const char* directory, const char* pattern, char* results[], int max_results);

int kapi_copy_file(const char* src, const char* dst, bool preserve_attrs);

int kapi_move_file(const char* src, const char* dst);

int kapi_delete_recursive(const char* path);

bool kapi_path_exists(const char* path);

bool kapi_is_absolute(const char* path);

bool kapi_is_relative(const char* path);

char* kapi_basename(const char* path);

char* kapi_dirname(const char* path);

int kapi_join_paths(const char* path1, const char* path2, char* result, size_t size);

int kapi_normalize_path(const char* path, char* result, size_t size);

int kapi_make_relative(const char* from, const char* to, char* result, size_t size);

int kapi_set_nonblocking(int fd);

int kapi_get_nonblocking(int fd);

int kapi_set_cloexec(int fd);

int kapi_get_cloexec(int fd);

#ifdef __cplusplus
}
#endif

#endif