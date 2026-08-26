

#include "kapi_kfifo.h"
#include "kapi.h"

#include <arch/memory.h>
#include <string.h>

int kapi_kfifo_init(kapi_kfifo_t* fifo, unsigned int size)
{
    if (!fifo || size == 0) {
        return KAPI_EINVAL;
    }

    fifo->buffer = (unsigned char*)memory_alloc(size);
    if (!fifo->buffer) {
        return KAPI_ENOMEM;
    }

    fifo->size = size;
    fifo->in = 0;
    fifo->out = 0;
    return KAPI_OK;
}

void kapi_kfifo_free(kapi_kfifo_t* fifo)
{
    if (!fifo) {
        return;
    }

    if (fifo->buffer) {
        memory_free(fifo->buffer);
        fifo->buffer = NULL;
    }
    fifo->size = 0;
    fifo->in = 0;
    fifo->out = 0;
}

unsigned int kapi_kfifo_in(kapi_kfifo_t* fifo, const void* buf, unsigned int len)
{
    if (!fifo || !fifo->buffer || !buf) {
        return 0;
    }

    unsigned int avail = kapi_kfifo_avail(fifo);
    if (len > avail) {
        len = avail;
    }

    unsigned int l = len;
    unsigned int off = fifo->in & (fifo->size - 1);

    if (off + l > fifo->size) {
        unsigned int first = fifo->size - off;
        memcpy(fifo->buffer + off, buf, first);
        memcpy(fifo->buffer, (const unsigned char*)buf + first, l - first);
    } else {
        memcpy(fifo->buffer + off, buf, l);
    }

    fifo->in += l;
    return l;
}

unsigned int kapi_kfifo_out(kapi_kfifo_t* fifo, void* buf, unsigned int len)
{
    if (!fifo || !fifo->buffer || !buf) {
        return 0;
    }

    unsigned int used = kapi_kfifo_len(fifo);
    if (len > used) {
        len = used;
    }

    unsigned int l = len;
    unsigned int off = fifo->out & (fifo->size - 1);

    if (off + l > fifo->size) {
        unsigned int first = fifo->size - off;
        memcpy(buf, fifo->buffer + off, first);
        memcpy((unsigned char*)buf + first, fifo->buffer, l - first);
    } else {
        memcpy(buf, fifo->buffer + off, l);
    }

    fifo->out += l;
    return l;
}

unsigned int kapi_kfifo_len(const kapi_kfifo_t* fifo)
{
    if (!fifo) {
        return 0;
    }
    return fifo->in - fifo->out;
}

unsigned int kapi_kfifo_avail(const kapi_kfifo_t* fifo)
{
    if (!fifo) {
        return 0;
    }
    return fifo->size - (fifo->in - fifo->out);
}

int kapi_kfifo_is_empty(const kapi_kfifo_t* fifo)
{
    if (!fifo) {
        return 1;
    }
    return fifo->in == fifo->out;
}

int kapi_kfifo_is_full(const kapi_kfifo_t* fifo)
{
    if (!fifo) {
        return 0;
    }
    return (fifo->in - fifo->out) >= fifo->size;
}

void kapi_kfifo_reset(kapi_kfifo_t* fifo)
{
    if (!fifo) {
        return;
    }
    fifo->in = 0;
    fifo->out = 0;
}
