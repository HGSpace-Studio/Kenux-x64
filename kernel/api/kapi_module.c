

#include "kapi_module.h"
#include "kapi.h"

#include <module.h>

void kapi_export_symbol(const char* name, void* addr)
{
    module_export_symbol(name, addr);
}

int kapi_module_load(const void* data, uint64_t size, const char* name)
{
    return module_load(data, size, name);
}

int kapi_module_unload(const char* name)
{
    return module_unload(name);
}
