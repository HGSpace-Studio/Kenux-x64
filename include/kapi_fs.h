

#ifndef KAPI_FS_H
#define KAPI_FS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_file* kapi_file_t;

typedef struct kapi_dir* kapi_dir_t;

#define KAPI_O_RDONLY    0x0001
#define KAPI_O_WRONLY    0x0002
#define KAPI_O_RDWR      0x0004
#define KAPI_O_CREAT     0x0008
#define KAPI_O_TRUNC     0x0010
#define KAPI_O_APPEND    0x0020
#define KAPI_O_EXCL      0x0040
#define KAPI_O_NONBLOCK  0x0080
#define KAPI_O_DIRECTORY 0x0100

#define KAPI_S_IRUSR     0400
#define KAPI_S_IWUSR     0200
#define KAPI_S_IXUSR     0100
#define KAPI_S_IRGRP     0040
#define KAPI_S_IWGRP     0020
#define KAPI_S_IXGRP     0010
#define KAPI_S_IROTH     0004
#define KAPI_S_IWOTH     0002
#define KAPI_S_IXOTH     0001

#define KAPI_FT_UNKNOWN  0
#define KAPI_FT_REG      1
#define KAPI_FT_DIR      2
#define KAPI_FT_CHR      3
#define KAPI_FT_BLK      4
#define KAPI_FT_FIFO     5
#define KAPI_FT_LNK      6
#define KAPI_FT_SOCK     7

typedef struct {
    uint64_t size;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint64_t blocks;
    uint32_t blksize;
} kapi_file_stat_t;

typedef struct {
    int      type;
    char     name[256];
    uint64_t size;
} kapi_dirent_t;

kapi_file_t kapi_open(const char* path, int flags, int mode);

int kapi_close(kapi_file_t file);

int64_t kapi_read(kapi_file_t file, void* buf, size_t count);

int64_t kapi_write(kapi_file_t file, const void* buf, size_t count);

int64_t kapi_seek(kapi_file_t file, int64_t offset, int whence);

int kapi_stat(const char* path, kapi_file_stat_t* stat);

int kapi_unlink(const char* path);

int kapi_rename(const char* oldpath, const char* newpath);

int kapi_mkdir(const char* path, int mode);

int kapi_rmdir(const char* path);

kapi_dir_t kapi_opendir(const char* path);

int kapi_readdir(kapi_dir_t dir, kapi_dirent_t* entry);

int kapi_closedir(kapi_dir_t dir);

char* kapi_getcwd(char* buf, size_t size);

int kapi_chdir(const char* path);

int kapi_mount(const char* source, const char* target, const char* fstype, uint64_t flags);

int kapi_umount(const char* target);

#ifdef __cplusplus
}
#endif

#endif
