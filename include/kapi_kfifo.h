

#ifndef KAPI_KFIFO_H
#define KAPI_KFIFO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned char* buffer;
    unsigned int size;
    unsigned int in;
    unsigned int out;
} kapi_kfifo_t;

int kapi_kfifo_init(kapi_kfifo_t* fifo, unsigned int size);

void kapi_kfifo_free(kapi_kfifo_t* fifo);

unsigned int kapi_kfifo_in(kapi_kfifo_t* fifo, const void* buf, unsigned int len);

unsigned int kapi_kfifo_out(kapi_kfifo_t* fifo, void* buf, unsigned int len);

unsigned int kapi_kfifo_len(const kapi_kfifo_t* fifo);

unsigned int kapi_kfifo_avail(const kapi_kfifo_t* fifo);

int kapi_kfifo_is_empty(const kapi_kfifo_t* fifo);

int kapi_kfifo_is_full(const kapi_kfifo_t* fifo);

void kapi_kfifo_reset(kapi_kfifo_t* fifo);

#ifdef __cplusplus
}
#endif

#endif
