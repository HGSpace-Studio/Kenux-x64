

#include "fifo.h"
#include "wait.h"
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

extern void* kmalloc(size_t size);
extern void  kfree(void* ptr);
extern void  process_yield(void);

fifo_pipe_t* fifo_create(void)
{
    fifo_pipe_t* pipe;

    pipe = (fifo_pipe_t*)kmalloc(sizeof(fifo_pipe_t));
    if (!pipe)
        return NULL;

    memset(pipe->data, 0, FIFO_PIPE_SIZE);
    pipe->read_pos  = 0;
    pipe->write_pos = 0;
    pipe->bytes     = 0;

    spin_init(&pipe->lock);

    wait_queue_init(&pipe->read_waiters);
    wait_queue_init(&pipe->write_waiters);

    pipe->ref_count = 2;

    pipe->flags = 0;

    return pipe;
}

void fifo_destroy(fifo_pipe_t* pipe)
{
    if (!pipe)
        return;

    kfree(pipe);
}

int64_t fifo_read(fifo_pipe_t* pipe, void* buf, uint64_t count)
{
    const uint8_t* src;
    uint8_t*       dst;
    uint32_t       to_copy;
    uint32_t       contiguous;
    uint32_t       first_chunk;
    uint32_t       was_full;
    int64_t        total = 0;

    if (!pipe || !buf || count == 0)
        return -1;

    dst = (uint8_t*)buf;

    spin_lock(&pipe->lock);

    while (total < (int64_t)count) {

        if (pipe->bytes == 0) {

            if (pipe->flags & FIFO_WRITER_CLOSED) {
                spin_unlock(&pipe->lock);
                return (total > 0) ? total : 0;
            }

            wait_queue_wait(&pipe->read_waiters, &pipe->lock);

            continue;
        }

        to_copy = (uint32_t)count - (uint32_t)total;
        if (to_copy > pipe->bytes)
            to_copy = pipe->bytes;

        was_full = (pipe->bytes == FIFO_PIPE_SIZE) ? 1 : 0;

        src = &pipe->data[pipe->read_pos];

        contiguous = FIFO_PIPE_SIZE - pipe->read_pos;
        if (contiguous >= to_copy) {

            memcpy(dst + total, src, to_copy);
            pipe->read_pos += to_copy;
        } else {

            first_chunk = contiguous;
            memcpy(dst + total, src, first_chunk);

            memcpy(dst + total + first_chunk, pipe->data, to_copy - first_chunk);
            pipe->read_pos = to_copy - first_chunk;
        }

        pipe->bytes -= to_copy;
        total += to_copy;

        if (was_full) {
            wait_queue_wake_all(&pipe->write_waiters);
        }
    }

    spin_unlock(&pipe->lock);
    return total;
}

int64_t fifo_write(fifo_pipe_t* pipe, const void* buf, uint64_t count)
{
    const uint8_t* src;
    uint8_t*       dst;
    uint32_t       remaining;
    uint32_t       space;
    uint32_t       to_copy;
    uint32_t       contiguous;
    uint32_t       first_chunk;
    uint32_t       was_empty;
    int64_t        total = 0;

    if (!pipe || !buf || count == 0)
        return -1;

    src = (const uint8_t*)buf;
    remaining = (uint32_t)count;

    spin_lock(&pipe->lock);

    while (remaining > 0) {

        if (pipe->flags & FIFO_READER_CLOSED) {
            spin_unlock(&pipe->lock);
            return (total > 0) ? total : -1;
        }

        space = FIFO_PIPE_SIZE - pipe->bytes;

        if (space == 0) {

            wait_queue_wait(&pipe->write_waiters, &pipe->lock);

            continue;
        }

        to_copy = remaining;
        if (to_copy > space)
            to_copy = space;

        was_empty = (pipe->bytes == 0) ? 1 : 0;

        dst = &pipe->data[pipe->write_pos];

        contiguous = FIFO_PIPE_SIZE - pipe->write_pos;
        if (contiguous >= to_copy) {

            memcpy(dst, src + total, to_copy);
            pipe->write_pos += to_copy;
        } else {

            first_chunk = contiguous;
            memcpy(dst, src + total, first_chunk);

            memcpy(pipe->data, src + total + first_chunk, to_copy - first_chunk);
            pipe->write_pos = to_copy - first_chunk;
        }

        pipe->bytes += to_copy;
        total += to_copy;
        remaining -= to_copy;

        if (was_empty) {
            wait_queue_wake_all(&pipe->read_waiters);
        }
    }

    spin_unlock(&pipe->lock);
    return total;
}

uint32_t fifo_poll(fifo_pipe_t* pipe)
{
    uint32_t events = 0;

    if (!pipe)
        return 0;

    spin_lock(&pipe->lock);

    if (pipe->bytes > 0 || (pipe->flags & FIFO_WRITER_CLOSED))
        events |= 0x01;

    if (pipe->bytes < FIFO_PIPE_SIZE || (pipe->flags & FIFO_READER_CLOSED))
        events |= 0x02;

    if (pipe->flags & (FIFO_READER_CLOSED | FIFO_WRITER_CLOSED))
        events |= 0x04;

    spin_unlock(&pipe->lock);
    return events;
}

void fifo_close_reader(fifo_pipe_t* pipe)
{
    if (!pipe)
        return;

    spin_lock(&pipe->lock);

    pipe->flags |= FIFO_READER_CLOSED;

    wait_queue_wake_all(&pipe->write_waiters);

    spin_unlock(&pipe->lock);

    if (__sync_sub_and_fetch(&pipe->ref_count, 1) == 0) {

        fifo_destroy(pipe);
    }
}

void fifo_close_writer(fifo_pipe_t* pipe)
{
    if (!pipe)
        return;

    spin_lock(&pipe->lock);

    pipe->flags |= FIFO_WRITER_CLOSED;

    wait_queue_wake_all(&pipe->read_waiters);

    spin_unlock(&pipe->lock);

    if (__sync_sub_and_fetch(&pipe->ref_count, 1) == 0) {

        fifo_destroy(pipe);
    }
}
