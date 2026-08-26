

#ifndef ARCH_X86_64_MMAP_H
#define ARCH_X86_64_MMAP_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/fs.h>

#define MAP_SHARED     0x01
#define MAP_PRIVATE    0x02
#define MAP_FIXED      0x10
#define MAP_ANONYMOUS  0x20
#define MAP_ANON       MAP_ANONYMOUS

#define PROT_NONE      0x0
#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define PROT_EXEC      0x4

#define MMAP_FLAG_DIRTY    0x1000
#define MMAP_FLAG_COW      0x2000
#define MMAP_FLAG_POPULATED 0x4000

#define MMAP_MAX_AREAS  256

#define MMAP_SEARCH_START  0x7F0000000000ULL

#define MMAP_MIN_ADDR      0x000010000000ULL

typedef struct {
    uint64_t     start;
    uint64_t     length;
    uint32_t     prot;
    uint32_t     flags;
    uint64_t     offset;
    int          ref_count;

    vfs_node_t*  file;

    int          active;
} mmap_area_t;

typedef struct {
    mmap_area_t  areas[MMAP_MAX_AREAS];
    int          area_count;
    spinlock_t   lock;
} mmap_context_t;

mmap_area_t* mmap_alloc(void);

void mmap_free(mmap_area_t* area);

uint64_t mmap_find_free_area(mmap_context_t* ctx, uint64_t length);

uint64_t mmap_do_mmap(mmap_context_t* ctx, uint64_t addr, uint64_t length,
                       uint32_t prot, uint32_t flags, int fd, uint64_t offset);

int mmap_do_munmap(mmap_context_t* ctx, uint64_t addr, uint64_t length);

int mmap_do_mprotect(mmap_context_t* ctx, uint64_t addr, uint64_t len,
                      uint32_t prot);

int mmap_page_fault_handler(mmap_context_t* ctx, uint64_t vaddr,
                             uint64_t error_code);

int mmap_fork_inherit(mmap_context_t* parent, mmap_context_t* child);

void mmap_context_init(mmap_context_t* ctx);

void mmap_context_destroy(mmap_context_t* ctx);

#endif
