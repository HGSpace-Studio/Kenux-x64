#include "kapi_seq_file.h"
#include "kapi.h"

#include <arch/fs.h>
#include <fs.h>
#include <arch/memory.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

struct kapi_seq_file {
    char*    buf;
    size_t   size;
    size_t   count;
    uint64_t pos;
    int      is_private;
    int      valid;
    kapi_seq_operations_t* ops;
    void*    iter;
};

#define KAPI_SEQ_MAX_ENTRIES 128

static kapi_seq_file_t kapi_seq_table[KAPI_SEQ_MAX_ENTRIES];
static int kapi_seq_initialized = 0;

typedef struct {
    char                  name[256];
    kapi_seq_operations_t ops;
    int                   valid;
} kapi_seq_registry_t;

static kapi_seq_registry_t kapi_seq_registry[KAPI_SEQ_MAX_ENTRIES];

int kapi_seq_file_init(void)
{
    if (kapi_seq_initialized) {
        return KAPI_OK;
    }

    memset(kapi_seq_table, 0, sizeof(kapi_seq_table));
    memset(kapi_seq_registry, 0, sizeof(kapi_seq_registry));
    kapi_seq_initialized = 1;
    return KAPI_OK;
}

kapi_seq_file_t* kapi_seq_open(void* buf, size_t size, int is_private)
{
    int slot = -1;
    for (int i = 0; i < KAPI_SEQ_MAX_ENTRIES; i++) {
        if (!kapi_seq_table[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return NULL;
    }

    kapi_seq_file_t* seq = &kapi_seq_table[slot];
    memset(seq, 0, sizeof(*seq));

    if (is_private && buf) {
        seq->buf = (char*)buf;
        seq->size = size;
        seq->is_private = 1;
    } else {
        seq->buf = (char*)memory_alloc(KAPI_SEQ_BUF_SIZE);
        if (!seq->buf) {
            return NULL;
        }
        seq->size = KAPI_SEQ_BUF_SIZE;
        seq->is_private = 0;
    }

    seq->count = 0;
    seq->pos = 0;
    seq->valid = 1;
    return seq;
}

int kapi_seq_read(kapi_seq_file_t* seq, char* buf, size_t count, uint64_t* pos)
{
    if (!seq || !seq->valid || !buf) {
        return KAPI_EINVAL;
    }

    if (!seq->ops) {
        return KAPI_ERROR;
    }

    if (*pos >= seq->count) {
        seq->count = 0;
        seq->buf[0] = '\0';

        if (seq->ops->start) {
            uint64_t p = *pos;
            seq->iter = seq->ops->start(seq, &p);
            if (seq->iter && seq->ops->show) {
                seq->ops->show(seq, seq->iter);
            }
        }
    }

    size_t remaining = seq->count - (size_t)*pos;
    if (remaining == 0) {
        return 0;
    }

    size_t to_copy = count < remaining ? count : remaining;
    memcpy(buf, seq->buf + *pos, to_copy);
    *pos += to_copy;

    if (seq->ops->next && seq->iter) {
        uint64_t p = *pos;
        seq->iter = seq->ops->next(seq, seq->iter, &p);
        if (seq->iter && seq->ops->show) {
            seq->count = 0;
            seq->ops->show(seq, seq->iter);
        }
    }

    return (int)to_copy;
}

int kapi_seq_write(kapi_seq_file_t* seq, const char* buf, size_t count)
{
    if (!seq || !seq->valid || !buf) {
        return KAPI_EINVAL;
    }

    size_t remaining = seq->size - seq->count;
    if (count > remaining) {
        count = remaining;
    }

    memcpy(seq->buf + seq->count, buf, count);
    seq->count += count;
    return (int)count;
}

int kapi_seq_printf(kapi_seq_file_t* seq, const char* fmt, ...)
{
    if (!seq || !seq->valid || !fmt) {
        return KAPI_EINVAL;
    }

    va_list args;
    va_start(args, fmt);
    int ret = vsprintf(seq->buf + seq->count, fmt, args);
    va_end(args);

    if (ret > 0) {
        seq->count += (size_t)ret;
        if (seq->count > seq->size) {
            seq->count = seq->size;
        }
    }
    return ret;
}

int kapi_seq_puts(kapi_seq_file_t* seq, const char* str)
{
    if (!seq || !seq->valid || !str) {
        return KAPI_EINVAL;
    }

    size_t len = strlen(str);
    return kapi_seq_write(seq, str, len);
}

int kapi_seq_putc(kapi_seq_file_t* seq, char c)
{
    if (!seq || !seq->valid) {
        return KAPI_EINVAL;
    }

    return kapi_seq_write(seq, &c, 1);
}

int kapi_seq_put_dec(kapi_seq_file_t* seq, int64_t val)
{
    if (!seq || !seq->valid) {
        return KAPI_EINVAL;
    }

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)val);
    if (len > 0) {
        return kapi_seq_write(seq, buf, (size_t)len);
    }
    return 0;
}

int kapi_seq_put_hex(kapi_seq_file_t* seq, uint64_t val)
{
    if (!seq || !seq->valid) {
        return KAPI_EINVAL;
    }

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%llx", (unsigned long long)val);
    if (len > 0) {
        return kapi_seq_write(seq, buf, (size_t)len);
    }
    return 0;
}

void kapi_seq_close(kapi_seq_file_t* seq)
{
    if (!seq || !seq->valid) {
        return;
    }

    if (seq->ops && seq->ops->stop && seq->iter) {
        seq->ops->stop(seq, seq->iter);
    }

    if (!seq->is_private && seq->buf) {
        memory_free(seq->buf);
    }

    seq->valid = 0;
    seq->buf = NULL;
}

int kapi_seq_register(const char* name, kapi_seq_operations_t* ops)
{
    if (!name || !ops) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_SEQ_MAX_ENTRIES; i++) {
        if (!kapi_seq_registry[i].valid) {
            strncpy(kapi_seq_registry[i].name, name,
                    sizeof(kapi_seq_registry[i].name) - 1);
            kapi_seq_registry[i].ops = *ops;
            kapi_seq_registry[i].valid = 1;
            return KAPI_OK;
        }
    }
    return KAPI_ERROR;
}

int kapi_seq_unregister(const char* name)
{
    if (!name) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_SEQ_MAX_ENTRIES; i++) {
        if (kapi_seq_registry[i].valid &&
            strcmp(kapi_seq_registry[i].name, name) == 0) {
            kapi_seq_registry[i].valid = 0;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}