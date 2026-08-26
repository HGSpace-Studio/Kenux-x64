

#ifndef KERNEL_FIFO_H
#define KERNEL_FIFO_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "wait.h"

#define FIFO_PIPE_SIZE    4096
#define FIFO_MAX_PIPES    64

#define FIFO_READER_CLOSED  0x01
#define FIFO_WRITER_CLOSED  0x02

typedef struct {
    uint8_t     data[FIFO_PIPE_SIZE];
    uint32_t    read_pos;
    uint32_t    write_pos;
    uint32_t    bytes;
    spinlock_t  lock;
    wait_queue_t read_waiters;
    wait_queue_t write_waiters;
    uint32_t    ref_count;
    uint32_t    flags;
} fifo_pipe_t;

fifo_pipe_t* fifo_create(void);

void fifo_destroy(fifo_pipe_t* pipe);

int64_t fifo_read(fifo_pipe_t* pipe, void* buf, uint64_t count);

int64_t fifo_write(fifo_pipe_t* pipe, const void* buf, uint64_t count);

uint32_t fifo_poll(fifo_pipe_t* pipe);

void fifo_close_reader(fifo_pipe_t* pipe);

void fifo_close_writer(fifo_pipe_t* pipe);

#endif
