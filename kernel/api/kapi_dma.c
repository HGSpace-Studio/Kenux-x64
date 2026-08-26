#include "kapi_dma.h"
#include "kapi.h"

#include <arch/memory.h>
#include <string.h>

#define KAPI_MAX_DMA_HANDLES 128

static kapi_dma_handle_t kapi_dma_handles[KAPI_MAX_DMA_HANDLES];
static int kapi_dma_initialized = 0;

int kapi_dma_init(void)
{
    if (kapi_dma_initialized) {
        return KAPI_OK;
    }

    memset(kapi_dma_handles, 0, sizeof(kapi_dma_handles));
    kapi_dma_initialized = 1;
    return KAPI_OK;
}

kapi_dma_handle_t* kapi_dma_alloc(size_t size, uint32_t flags)
{
    if (size == 0) {
        return NULL;
    }

    int slot = -1;
    for (int i = 0; i < KAPI_MAX_DMA_HANDLES; i++) {
        if (!kapi_dma_handles[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    void* vaddr = memory_alloc(size);
    if (!vaddr) {
        return NULL;
    }

    uint64_t paddr = (uint64_t)(uintptr_t)vaddr;

    kapi_dma_handles[slot].vaddr = vaddr;
    kapi_dma_handles[slot].paddr = paddr;
    kapi_dma_handles[slot].size = size;
    kapi_dma_handles[slot].flags = flags;
    kapi_dma_handles[slot].valid = 1;
    kapi_dma_handles[slot].dir = KAPI_DMA_DIR_BIDIRECTION;

    return &kapi_dma_handles[slot];
}

void kapi_dma_free(kapi_dma_handle_t* handle)
{
    if (!handle || !handle->valid) {
        return;
    }

    memory_free(handle->vaddr);
    handle->valid = 0;
    handle->vaddr = NULL;
    handle->paddr = 0;
    handle->size = 0;
}

void* kapi_dma_get_vaddr(kapi_dma_handle_t* handle)
{
    if (!handle || !handle->valid) {
        return NULL;
    }

    return handle->vaddr;
}

uint64_t kapi_dma_get_paddr(kapi_dma_handle_t* handle)
{
    if (!handle || !handle->valid) {
        return 0;
    }

    return handle->paddr;
}

int kapi_dma_sync_single(kapi_dma_handle_t* handle, size_t offset, size_t size, int direction)
{
    if (!handle || !handle->valid || size == 0) {
        return KAPI_EINVAL;
    }

    (void)offset;
    (void)size;
    (void)direction;

    return KAPI_OK;
}

int kapi_dma_sync_for_device(kapi_dma_handle_t* handle)
{
    if (!handle || !handle->valid) {
        return KAPI_EINVAL;
    }

    return KAPI_OK;
}

int kapi_dma_sync_for_cpu(kapi_dma_handle_t* handle)
{
    if (!handle || !handle->valid) {
        return KAPI_EINVAL;
    }

    return KAPI_OK;
}

int kapi_dma_map_single(kapi_dma_handle_t* handle, void* addr, size_t size, int dir)
{
    if (!handle || !addr || size == 0) {
        return KAPI_EINVAL;
    }

    handle->vaddr = addr;
    handle->paddr = (uint64_t)(uintptr_t)addr;
    handle->size = size;
    handle->dir = dir;

    return KAPI_OK;
}

void kapi_dma_unmap_single(kapi_dma_handle_t* handle)
{
    if (!handle || !handle->valid) {
        return;
    }

    handle->vaddr = NULL;
    handle->paddr = 0;
    handle->size = 0;
}

int kapi_dma_alloc_coherent(size_t size, uint64_t* dma_handle, void** cpu_handle, uint32_t flags)
{
    if (size == 0 || !dma_handle || !cpu_handle) {
        return KAPI_EINVAL;
    }

    (void)flags;

    *cpu_handle = memory_alloc(size);
    if (!*cpu_handle) {
        *dma_handle = 0;
        return KAPI_ENOMEM;
    }

    *dma_handle = (uint64_t)(uintptr_t)*cpu_handle;
    return KAPI_OK;
}

void kapi_dma_free_coherent(size_t size, void* cpu_addr, uint64_t dma_handle)
{
    (void)size;
    (void)dma_handle;

    if (cpu_addr) {
        memory_free(cpu_addr);
    }
}

int kapi_dma_get_max_segments(void)
{
    return 64;
}