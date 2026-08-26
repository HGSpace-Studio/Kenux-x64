

#include "kapi_memory.h"
#include "kapi.h"

#include <arch/memory.h>
#include <arch/memory.h>
#include <string.h>

static struct {
    uint64_t used;
} kapi_mem_stats;

int kapi_memory_init(void)
{
    kapi_mem_stats.used = 0;
    return KAPI_OK;
}

void* kapi_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    void* ptr = memory_alloc(size);
    if (ptr) {
        kapi_mem_stats.used += size;
    }
    return ptr;
}

void kapi_free(void* ptr)
{
    if (!ptr) {
        return;
    }

    memory_free(ptr);
}

void* kapi_realloc(void* ptr, size_t size)
{
    if (size == 0) {
        kapi_free(ptr);
        return NULL;
    }

    if (!ptr) {
        return kapi_malloc(size);
    }

    /* 修复: 原代码用新 size 做 memcpy, 当新 size > 旧 size 时会越界读取.
       用 ksize 拿到旧块大小, 取 min. */
    extern uint64_t ksize(void* ptr);
    uint64_t old_size = ksize(ptr);
    size_t copy_size = (old_size < size) ? (size_t)old_size : size;

    void* new_ptr = kapi_malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, copy_size);
        kapi_free(ptr);
    }

    return new_ptr;
}

void* kapi_calloc(size_t count, size_t size)
{
    if (count == 0 || size == 0) {
        return NULL;
    }

    size_t total = count * size;
    void* ptr = kapi_malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

uint64_t kapi_page_alloc(int count, uint32_t flags)
{
    (void)flags;
    if (count <= 0) {
        return 0;
    }

    uint64_t size = (uint64_t)count * MEMORY_BLOCK_SIZE;
    void* ptr = memory_alloc(size);
    if (!ptr) {
        return 0;
    }

    return (uint64_t)(uintptr_t)ptr;
}

void kapi_page_free(uint64_t paddr, int count)
{
    (void)count;
    if (paddr == 0) {
        return;
    }

    memory_free((void*)(uintptr_t)paddr);
}

uint64_t kapi_mmap(uint64_t vaddr, uint64_t paddr, size_t size, int prot)
{
    (void)prot;

    if (paddr != 0) {
        return paddr;
    }

    void* ptr = memory_alloc(size);
    if (!ptr) {
        return 0;
    }

    uint64_t result = (uint64_t)(uintptr_t)ptr;
    if (vaddr != 0) {

        (void)vaddr;
    }

    return result;
}

int kapi_munmap(uint64_t vaddr, size_t size)
{
    (void)size;
    if (vaddr == 0) {
        return KAPI_EINVAL;
    }

    memory_free((void*)(uintptr_t)vaddr);
    return KAPI_OK;
}

int kapi_mprotect(uint64_t vaddr, size_t size, int prot)
{
    (void)vaddr;
    (void)size;
    (void)prot;

    return KAPI_OK;
}

int kapi_mem_get_info(kapi_mem_info_t* info)
{
    if (!info) {
        return KAPI_EINVAL;
    }

    memset(info, 0, sizeof(*info));
    info->total = memory_get_total();
    info->free = memory_get_free();
    info->used = info->total - info->free;
    info->total_pages = info->total / MEMORY_BLOCK_SIZE;
    info->free_pages = info->free / MEMORY_BLOCK_SIZE;

    return KAPI_OK;
}

uint64_t kapi_mem_get_usage(void)
{
    return kapi_mem_stats.used;
}
