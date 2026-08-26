

#ifndef KAPI_MODULE_H
#define KAPI_MODULE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void kapi_export_symbol(const char* name, void* addr);

int kapi_module_load(const void* data, uint64_t size, const char* name);

int kapi_module_unload(const char* name);

#define kapi_module_init(initfn) int init_module(void) { return initfn(); }

#define kapi_module_exit(exitfn) void cleanup_module(void) { exitfn(); }

#ifdef __cplusplus
}
#endif

#endif
