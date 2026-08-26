#ifndef KAPI_DMA_H
#define KAPI_DMA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_DMA_DIR_TO_DEVICE   1
#define KAPI_DMA_DIR_FROM_DEVICE 2
#define KAPI_DMA_DIR_BIDIRECTION 3

#define KAPI_DMA_FLAGS_CONSISTENT  0x01
#define KAPI_DMA_FLAGS_CACHED      0x02
#define KAPI_DMA_FLAGS_32BIT_MASK  0x04
#define KAPI_DMA_FLAGS_64BIT_MASK  0x08
#define KAPI_DMA_FLAGS_WRITE_COMBINE 0x10

typedef struct {
    void*      vaddr;
    uint64_t   paddr;
    size_t     size;
    int        dir;
    uint32_t   flags;
    int        valid;
} kapi_dma_handle_t;

kapi_dma_handle_t* kapi_dma_alloc(size_t size, uint32_t flags);
void kapi_dma_free(kapi_dma_handle_t* handle);

void* kapi_dma_get_vaddr(kapi_dma_handle_t* handle);
uint64_t kapi_dma_get_paddr(kapi_dma_handle_t* handle);

int kapi_dma_sync_single(kapi_dma_handle_t* handle, size_t offset, size_t size, int direction);
int kapi_dma_sync_for_device(kapi_dma_handle_t* handle);
int kapi_dma_sync_for_cpu(kapi_dma_handle_t* handle);

int kapi_dma_map_single(kapi_dma_handle_t* handle, void* addr, size_t size, int dir);
void kapi_dma_unmap_single(kapi_dma_handle_t* handle);

int kapi_dma_alloc_coherent(size_t size, uint64_t* dma_handle, void** cpu_handle, uint32_t flags);
void kapi_dma_free_coherent(size_t size, void* cpu_addr, uint64_t dma_handle);

int kapi_dma_get_max_segments(void);
int kapi_dma_init(void);

#ifdef __cplusplus
}
#endif

#endif