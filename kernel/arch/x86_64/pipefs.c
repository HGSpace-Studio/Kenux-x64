#include <arch/fs.h>
#include <arch/memory.h>
#include <string.h>

#define PIPEFS_MAX_PIPES   256
#define PIPE_BUF_SIZE      4096

typedef struct {
    uint8_t  buffer[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
    uint32_t readers;
    uint32_t writers;
    uint64_t read_pid;
    uint64_t write_pid;
    spinlock_t lock;
    int      nonblocking;
} pipefs_pipe_t;

static pipefs_pipe_t pipefs_pipes[PIPEFS_MAX_PIPES];
static vfs_node_t pipefs_root;
static spinlock_t pipefs_global_lock = SPINLOCK_INIT;

static int pipefs_find_free(void)
{
    for (int i = 0; i < PIPEFS_MAX_PIPES; i++) {
        if (pipefs_pipes[i].readers == 0 && pipefs_pipes[i].writers == 0) return i;
    }
    return -1;
}

static int pipefs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t size)
{
    if (!node || !buf) return -1;
    int idx = (int)(uintptr_t)node->impl_data;
    if (idx < 0 || idx >= PIPEFS_MAX_PIPES) return -2;

    pipefs_pipe_t* pipe = &pipefs_pipes[idx];
    spinlock_acquire(&pipe->lock);

    if (pipe->count == 0) {
        if (pipe->writers == 0) {
            spinlock_release(&pipe->lock);
            return 0;
        }
        if (pipe->nonblocking) {
            spinlock_release(&pipe->lock);
            return -3;
        }
        spinlock_release(&pipe->lock);
        return -4;
    }

    uint64_t to_read = size;
    if (to_read > pipe->count) to_read = pipe->count;

    uint8_t* dst = (uint8_t*)buf;
    for (uint64_t i = 0; i < to_read; i++) {
        dst[i] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUF_SIZE;
    }
    pipe->count -= to_read;

    spinlock_release(&pipe->lock);
    return (int)to_read;
}

static int pipefs_write(vfs_node_t* node, uint64_t offset, const void* buf, uint64_t size)
{
    if (!node || !buf) return -1;
    int idx = (int)(uintptr_t)node->impl_data;
    if (idx < 0 || idx >= PIPEFS_MAX_PIPES) return -2;

    pipefs_pipe_t* pipe = &pipefs_pipes[idx];
    spinlock_acquire(&pipe->lock);

    if (pipe->readers == 0) {
        spinlock_release(&pipe->lock);
        return -3;
    }

    uint64_t available = PIPE_BUF_SIZE - pipe->count;
    uint64_t to_write = size;
    if (to_write > available) {
        if (pipe->nonblocking) {
            to_write = available;
            if (to_write == 0) {
                spinlock_release(&pipe->lock);
                return -4;
            }
        } else {
            to_write = available;
        }
    }

    const uint8_t* src = (const uint8_t*)buf;
    for (uint64_t i = 0; i < to_write; i++) {
        pipe->buffer[pipe->write_pos] = src[i];
        pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUF_SIZE;
    }
    pipe->count += to_write;

    spinlock_release(&pipe->lock);
    return (int)to_write;
}

static int pipefs_open(vfs_node_t* node, int flags)
{
    if (!node) return -1;
    int idx = (int)(uintptr_t)node->impl_data;
    if (idx < 0 || idx >= PIPEFS_MAX_PIPES) return -2;

    pipefs_pipe_t* pipe = &pipefs_pipes[idx];
    spinlock_acquire(&pipe->lock);

    if (flags & O_RDONLY) pipe->readers++;
    if (flags & O_WRONLY) pipe->writers++;
    if (flags & O_NONBLOCK) pipe->nonblocking = 1;

    spinlock_release(&pipe->lock);
    return 0;
}

static int pipefs_close(vfs_node_t* node)
{
    if (!node) return -1;
    int idx = (int)(uintptr_t)node->impl_data;
    if (idx < 0 || idx >= PIPEFS_MAX_PIPES) return -2;

    pipefs_pipe_t* pipe = &pipefs_pipes[idx];
    spinlock_acquire(&pipe->lock);
    pipe->readers--;
    pipe->writers--;
    if (pipe->readers == 0 && pipe->writers == 0) {
        pipe->read_pos = 0;
        pipe->write_pos = 0;
        pipe->count = 0;
    }
    spinlock_release(&pipe->lock);
    return 0;
}

int pipefs_init(void)
{
    memset(&pipefs_root, 0, sizeof(vfs_node_t));
    strncpy(pipefs_root.name, "pipefs", FS_MAX_NAME - 1);
    pipefs_root.type = FS_TYPE_DIR;
    pipefs_root.nlink = 2;

    memset(pipefs_pipes, 0, sizeof(pipefs_pipes));
    for (int i = 0; i < PIPEFS_MAX_PIPES; i++) {
        pipefs_pipes[i].lock = SPINLOCK_INIT;
    }

    return 0;
}

int pipefs_create_pipe(int* read_fd, int* write_fd)
{
    spinlock_acquire(&pipefs_global_lock);
    int idx = pipefs_find_free();
    if (idx < 0) {
        spinlock_release(&pipefs_global_lock);
        return -1;
    }

    pipefs_pipe_t* pipe = &pipefs_pipes[idx];
    memset(pipe, 0, sizeof(pipefs_pipe_t));
    pipe->lock = SPINLOCK_INIT;
    pipe->readers = 1;
    pipe->writers = 1;

    spinlock_release(&pipefs_global_lock);

    if (read_fd) *read_fd = idx * 2;
    if (write_fd) *write_fd = idx * 2 + 1;
    return 0;
}

int pipefs_dup(int fd, int* new_fd)
{
    if (!new_fd) return -1;
    *new_fd = fd;
    return 0;
}

vfs_node_t* pipefs_get_root(void)
{
    return &pipefs_root;
}