#ifndef KAPI_DEBUGFS_H
#define KAPI_DEBUGFS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_debugfs_file kapi_debugfs_file_t;

typedef int (*kapi_debugfs_read_fn)(kapi_debugfs_file_t* file, char* buf,
                                     size_t count, uint64_t* pos);
typedef int (*kapi_debugfs_write_fn)(kapi_debugfs_file_t* file, const char* buf,
                                      size_t count, uint64_t* pos);

typedef struct {
    kapi_debugfs_read_fn  read;
    kapi_debugfs_write_fn write;
    void*                 private;
} kapi_debugfs_fops_t;

#define KAPI_DEBUGFS_DIR   0x01
#define KAPI_DEBUGFS_FILE  0x02
#define KAPI_DEBUGFS_SYML  0x04

kapi_debugfs_file_t* kapi_debugfs_create_dir(const char* name,
                                              kapi_debugfs_file_t* parent);

kapi_debugfs_file_t* kapi_debugfs_create_file(const char* name, uint32_t mode,
                                               kapi_debugfs_file_t* parent,
                                               void* data,
                                               kapi_debugfs_fops_t* fops);

kapi_debugfs_file_t* kapi_debugfs_create_symlink(const char* name,
                                                  kapi_debugfs_file_t* parent,
                                                  const char* target);

kapi_debugfs_file_t* kapi_debugfs_create_u32(const char* name, uint32_t mode,
                                              kapi_debugfs_file_t* parent,
                                              uint32_t* value);

kapi_debugfs_file_t* kapi_debugfs_create_u64(const char* name, uint32_t mode,
                                              kapi_debugfs_file_t* parent,
                                              uint64_t* value);

kapi_debugfs_file_t* kapi_debugfs_create_bool(const char* name, uint32_t mode,
                                               kapi_debugfs_file_t* parent,
                                               int* value);

kapi_debugfs_file_t* kapi_debugfs_create_blob(const char* name, uint32_t mode,
                                               kapi_debugfs_file_t* parent,
                                               void* data, size_t size);

void kapi_debugfs_remove(kapi_debugfs_file_t* file);

void kapi_debugfs_remove_recursive(kapi_debugfs_file_t* file);

int kapi_debugfs_init(void);

#ifdef __cplusplus
}
#endif

#endif