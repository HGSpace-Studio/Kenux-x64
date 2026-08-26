#ifndef KAPI_PARAMS_H
#define KAPI_PARAMS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_PARAM_PERM_R      0444
#define KAPI_PARAM_PERM_W      0222
#define KAPI_PARAM_PERM_RW     0666

#define kapi_module_param_bool(name, perm) \
    int kapi_param_##name; \
    static int __kapi_param_bool_##name

#define kapi_module_param_int(name, perm) \
    int kapi_param_##name; \
    static int __kapi_param_int_##name

#define kapi_module_param_uint(name, perm) \
    unsigned int kapi_param_##name; \
    static unsigned int __kapi_param_uint_##name

#define kapi_module_param_long(name, perm) \
    long kapi_param_##name; \
    static long __kapi_param_long_##name

#define kapi_module_param_ulong(name, perm) \
    unsigned long kapi_param_##name; \
    static unsigned long __kapi_param_ulong_##name

#define kapi_module_param_string(name, len, perm) \
    char kapi_param_##name[len]; \
    static char __kapi_param_string_##name[len]

#define kapi_module_param_array(name, type, num, perm) \
    type kapi_param_##name[num]; \
    static type __kapi_param_array_##name[num]

void kapi_param_register_bool(const char* name, int* val, int perm);

void kapi_param_register_int(const char* name, int* val, int perm);

void kapi_param_register_uint(const char* name, unsigned int* val, int perm);

void kapi_param_register_long(const char* name, long* val, int perm);

void kapi_param_register_ulong(const char* name, unsigned long* val, int perm);

void kapi_param_register_string(const char* name, char* val, size_t len, int perm);

int kapi_param_get_bool(const char* name);

int kapi_param_get_int(const char* name);

unsigned int kapi_param_get_uint(const char* name);

long kapi_param_get_long(const char* name);

unsigned long kapi_param_get_ulong(const char* name);

const char* kapi_param_get_string(const char* name);

int kapi_param_set_int(const char* name, int val);

int kapi_param_set_bool(const char* name, int val);

int kapi_param_set_string(const char* name, const char* val);

void kapi_param_list(void);

#ifdef __cplusplus
}
#endif

#endif