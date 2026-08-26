#ifndef MEMORY_H
#define MEMORY_H

#include <arch/types.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

typedef struct {
    uint64_t present : 1;
    uint64_t writable : 1;
    uint64_t user : 1;
    uint64_t write_through : 1;
    uint64_t cache_disable : 1;
    uint64_t accessed : 1;
    uint64_t dirty : 1;
    uint64_t attr_index : 1;
    uint64_t global : 1;
    uint64_t available : 3;
    uint64_t addr : 40;
    uint64_t available_high : 11;
    uint64_t nx : 1;
} __attribute__((packed)) page_table_entry_t;

typedef struct {
    page_table_entry_t entries[512];
} __attribute__((packed)) page_table_t;

typedef struct {
    page_table_t* pdpt;
} __attribute__((packed)) page_directory_t;

void memory_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kcalloc(size_t nmemb, size_t size);

#endif
