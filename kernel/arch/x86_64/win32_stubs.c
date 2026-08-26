/* Win32 subsystem compatibility stubs — fills in missing symbols */
#include <stdint.h>
#include <string.h>
#include <arch/fs.h>
#include <arch/memory.h>

/* ================= KAPI memory info wrappers ================= */

uint64_t kapi_get_total_memory(void)
{
    return memory_get_total();
}

uint64_t kapi_get_free_memory(void)
{
    return memory_get_free();
}

/* ================= vfs_stat stub (minimal implementation) ================= */

typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    uint64_t st_size;
    uint64_t st_blksize;
    uint64_t st_blocks;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
} vfs_stat_t;

extern int   vfs_open(const char* path, int flags, int mode);
extern int   vfs_close(int fd);
extern int   vfs_lseek(int fd, int64_t offset, int whence);
extern int   vfs_read(int fd, void* buf, uint64_t count);
extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* p);

/* Minimal path walker using vfs_root + finddir() */
static vfs_node_t* vfs_walk(const char* path)
{
    vfs_node_t* cur;
    const char* p;
    char seg[256];
    int i;

    if (path == NULL) return NULL;
    cur = vfs_root;
    if (cur == NULL) return NULL;

    p = path;
    while (*p) {
        while (*p == '/') p++;
        if (*p == '\0') break;

        for (i = 0; i < 255 && *p && *p != '/'; i++, p++) {
            seg[i] = *p;
        }
        seg[i] = '\0';
        if (i == 0) continue;

        if (cur->finddir == NULL) return NULL;
        cur = cur->finddir(cur, seg);
        if (cur == NULL) return NULL;
    }
    return cur;
}

int vfs_stat(const char* path, void* statbuf)
{
    vfs_stat_t* st = (vfs_stat_t*)statbuf;
    vfs_node_t* node;

    if (path == NULL || statbuf == NULL) {
        return -1;
    }

    memset(st, 0, sizeof(*st));
    node = vfs_walk(path);
    if (node == NULL) {
        return -1;
    }

    st->st_ino   = (uint64_t)(uintptr_t)node;
    st->st_mode  = (uint32_t)(node->type == FS_TYPE_DIRECTORY ? 0x4000 : 0x8000) | 0x1FFu;
    st->st_nlink = 1;
    st->st_size  = node->size;
    st->st_mtime = node->mtime;
    st->st_ctime = node->ctime;
    return 0;
}
