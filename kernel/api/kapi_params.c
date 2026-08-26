#include "kapi_params.h"
#include "kapi.h"

#include <string.h>
#include <stdio.h>

#define KAPI_PARAM_MAX 64

typedef struct {
    char name[64];
    int type;
    union {
        int          bool_val;
        int          int_val;
        unsigned int uint_val;
        long         long_val;
        unsigned long ulong_val;
    };
    char   string_val[128];
    size_t string_len;
    int    perm;
    int    valid;
} kapi_param_entry_t;

#define KAPI_PARAM_TYPE_BOOL    0
#define KAPI_PARAM_TYPE_INT     1
#define KAPI_PARAM_TYPE_UINT    2
#define KAPI_PARAM_TYPE_LONG    3
#define KAPI_PARAM_TYPE_ULONG   4
#define KAPI_PARAM_TYPE_STRING  5

static kapi_param_entry_t kapi_param_table[KAPI_PARAM_MAX];
static int kapi_param_count = 0;

static kapi_param_entry_t* kapi_param_find(const char* name)
{
    for (int i = 0; i < kapi_param_count; i++) {
        if (kapi_param_table[i].valid && strcmp(kapi_param_table[i].name, name) == 0) {
            return &kapi_param_table[i];
        }
    }
    return NULL;
}

static kapi_param_entry_t* kapi_param_alloc(const char* name)
{
    if (kapi_param_count >= KAPI_PARAM_MAX) {
        return NULL;
    }
    kapi_param_entry_t* entry = &kapi_param_table[kapi_param_count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->valid = 1;
    kapi_param_count++;
    return entry;
}

void kapi_param_register_bool(const char* name, int* val, int perm)
{
    if (!name || !val) {
        return;
    }
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry) {
        entry = kapi_param_alloc(name);
    }
    if (entry) {
        entry->type = KAPI_PARAM_TYPE_BOOL;
        entry->bool_val = *val;
        entry->perm = perm;
    }
}

void kapi_param_register_int(const char* name, int* val, int perm)
{
    if (!name || !val) {
        return;
    }
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry) {
        entry = kapi_param_alloc(name);
    }
    if (entry) {
        entry->type = KAPI_PARAM_TYPE_INT;
        entry->int_val = *val;
        entry->perm = perm;
    }
}

void kapi_param_register_uint(const char* name, unsigned int* val, int perm)
{
    if (!name || !val) {
        return;
    }
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry) {
        entry = kapi_param_alloc(name);
    }
    if (entry) {
        entry->type = KAPI_PARAM_TYPE_UINT;
        entry->uint_val = *val;
        entry->perm = perm;
    }
}

void kapi_param_register_long(const char* name, long* val, int perm)
{
    if (!name || !val) {
        return;
    }
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry) {
        entry = kapi_param_alloc(name);
    }
    if (entry) {
        entry->type = KAPI_PARAM_TYPE_LONG;
        entry->long_val = *val;
        entry->perm = perm;
    }
}

void kapi_param_register_ulong(const char* name, unsigned long* val, int perm)
{
    if (!name || !val) {
        return;
    }
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry) {
        entry = kapi_param_alloc(name);
    }
    if (entry) {
        entry->type = KAPI_PARAM_TYPE_ULONG;
        entry->ulong_val = *val;
        entry->perm = perm;
    }
}

void kapi_param_register_string(const char* name, char* val, size_t len, int perm)
{
    if (!name || !val) {
        return;
    }
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry) {
        entry = kapi_param_alloc(name);
    }
    if (entry) {
        entry->type = KAPI_PARAM_TYPE_STRING;
        strncpy(entry->string_val, val, sizeof(entry->string_val) - 1);
        entry->string_val[sizeof(entry->string_val) - 1] = '\0';
        entry->string_len = len;
        entry->perm = perm;
    }
}

int kapi_param_get_bool(const char* name)
{
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry || entry->type != KAPI_PARAM_TYPE_BOOL) {
        return 0;
    }
    return entry->bool_val;
}

int kapi_param_get_int(const char* name)
{
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry || entry->type != KAPI_PARAM_TYPE_INT) {
        return 0;
    }
    return entry->int_val;
}

unsigned int kapi_param_get_uint(const char* name)
{
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry || entry->type != KAPI_PARAM_TYPE_UINT) {
        return 0;
    }
    return entry->uint_val;
}

long kapi_param_get_long(const char* name)
{
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry || entry->type != KAPI_PARAM_TYPE_LONG) {
        return 0;
    }
    return entry->long_val;
}

unsigned long kapi_param_get_ulong(const char* name)
{
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry || entry->type != KAPI_PARAM_TYPE_ULONG) {
        return 0;
    }
    return entry->ulong_val;
}

const char* kapi_param_get_string(const char* name)
{
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry || entry->type != KAPI_PARAM_TYPE_STRING) {
        return NULL;
    }
    return entry->string_val;
}

int kapi_param_set_int(const char* name, int val)
{
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry) {
        return KAPI_ENOENT;
    }
    if (entry->perm & KAPI_PARAM_PERM_W) {
        entry->int_val = val;
        return KAPI_OK;
    }
    return KAPI_EACCES;
}

int kapi_param_set_bool(const char* name, int val)
{
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry) {
        return KAPI_ENOENT;
    }
    if (entry->perm & KAPI_PARAM_PERM_W) {
        entry->bool_val = val;
        return KAPI_OK;
    }
    return KAPI_EACCES;
}

int kapi_param_set_string(const char* name, const char* val)
{
    kapi_param_entry_t* entry = kapi_param_find(name);
    if (!entry) {
        return KAPI_ENOENT;
    }
    if (entry->perm & KAPI_PARAM_PERM_W) {
        strncpy(entry->string_val, val, sizeof(entry->string_val) - 1);
        entry->string_val[sizeof(entry->string_val) - 1] = '\0';
        return KAPI_OK;
    }
    return KAPI_EACCES;
}

void kapi_param_list(void)
{
    for (int i = 0; i < kapi_param_count; i++) {
        if (!kapi_param_table[i].valid) {
            continue;
        }
        kapi_param_entry_t* e = &kapi_param_table[i];
        switch (e->type) {
            case KAPI_PARAM_TYPE_BOOL:
                printf("  %s = %d (bool)\n", e->name, e->bool_val);
                break;
            case KAPI_PARAM_TYPE_INT:
                printf("  %s = %d (int)\n", e->name, e->int_val);
                break;
            case KAPI_PARAM_TYPE_UINT:
                printf("  %s = %u (uint)\n", e->name, e->uint_val);
                break;
            case KAPI_PARAM_TYPE_LONG:
                printf("  %s = %ld (long)\n", e->name, e->long_val);
                break;
            case KAPI_PARAM_TYPE_ULONG:
                printf("  %s = %lu (ulong)\n", e->name, e->ulong_val);
                break;
            case KAPI_PARAM_TYPE_STRING:
                printf("  %s = \"%s\" (string)\n", e->name, e->string_val);
                break;
            default:
                break;
        }
    }
}