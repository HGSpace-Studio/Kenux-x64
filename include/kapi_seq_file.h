#ifndef KAPI_SEQ_FILE_H
#define KAPI_SEQ_FILE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_seq_file kapi_seq_file_t;

typedef void* (*kapi_seq_start_fn)(kapi_seq_file_t* seq, uint64_t* pos);
typedef void  (*kapi_seq_stop_fn)(kapi_seq_file_t* seq, void* v);
typedef void* (*kapi_seq_next_fn)(kapi_seq_file_t* seq, void* v, uint64_t* pos);
typedef int   (*kapi_seq_show_fn)(kapi_seq_file_t* seq, void* v);

typedef struct {
    kapi_seq_start_fn start;
    kapi_seq_stop_fn  stop;
    kapi_seq_next_fn  next;
    kapi_seq_show_fn  show;
    void*             private;
} kapi_seq_operations_t;

#define KAPI_SEQ_BUF_SIZE 4096

kapi_seq_file_t* kapi_seq_open(void* buf, size_t size, int is_private);

int kapi_seq_read(kapi_seq_file_t* seq, char* buf, size_t count, uint64_t* pos);

int kapi_seq_write(kapi_seq_file_t* seq, const char* buf, size_t count);

int kapi_seq_printf(kapi_seq_file_t* seq, const char* fmt, ...);

int kapi_seq_puts(kapi_seq_file_t* seq, const char* str);

int kapi_seq_putc(kapi_seq_file_t* seq, char c);

int kapi_seq_put_dec(kapi_seq_file_t* seq, int64_t val);

int kapi_seq_put_hex(kapi_seq_file_t* seq, uint64_t val);

void kapi_seq_close(kapi_seq_file_t* seq);

int kapi_seq_register(const char* name, kapi_seq_operations_t* ops);

int kapi_seq_unregister(const char* name);

int kapi_seq_file_init(void);

#ifdef __cplusplus
}
#endif

#endif