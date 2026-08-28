#include <arch/fs.h>
#include <arch/net.h>
#include <arch/memory.h>
#include <string.h>

#define SOCKETFS_MAX_SOCKETS 256

typedef struct {
    int domain;
    int type;
    int protocol;
    int state;
    uint64_t owner_pid;
    uint32_t recv_buf_len;
    uint32_t send_buf_len;
    vfs_node_t* node;
} socketfs_entry_t;

#define SOCKET_STATE_UNCONNECTED  0
#define SOCKET_STATE_CONNECTING   1
#define SOCKET_STATE_CONNECTED    2
#define SOCKET_STATE_LISTENING    3
#define SOCKET_STATE_CLOSING      4

static socketfs_entry_t socketfs_entries[SOCKETFS_MAX_SOCKETS];
static vfs_node_t socketfs_root;
static spinlock_t socketfs_lock = SPINLOCK_INIT;
static uint32_t socketfs_count = 0;

static int socketfs_find_free(void)
{
    for (int i = 0; i < SOCKETFS_MAX_SOCKETS; i++) {
        if (socketfs_entries[i].node == NULL) return i;
    }
    return -1;
}

static int socketfs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t size)
{
    if (!node || !buf) return -1;
    int idx = (int)(uintptr_t)node->impl_data;
    if (idx < 0 || idx >= SOCKETFS_MAX_SOCKETS) return -2;

    socketfs_entry_t* entry = &socketfs_entries[idx];
    if (entry->state != SOCKET_STATE_CONNECTED) return -3;

    return 0;
}

static int socketfs_write(vfs_node_t* node, uint64_t offset, const void* buf, uint64_t size)
{
    if (!node || !buf) return -1;
    int idx = (int)(uintptr_t)node->impl_data;
    if (idx < 0 || idx >= SOCKETFS_MAX_SOCKETS) return -2;

    socketfs_entry_t* entry = &socketfs_entries[idx];
    if (entry->state != SOCKET_STATE_CONNECTED) return -3;

    return (int)size;
}

int socketfs_init(void)
{
    memset(&socketfs_root, 0, sizeof(vfs_node_t));
    strncpy(socketfs_root.name, "socketfs", FS_MAX_NAME - 1);
    socketfs_root.type = FS_TYPE_DIR;
    socketfs_root.nlink = 2;

    memset(socketfs_entries, 0, sizeof(socketfs_entries));
    socketfs_count = 0;

    return 0;
}

int socketfs_create(int domain, int type, int protocol, int* fd)
{
    if (!fd) return -1;

    spinlock_acquire(&socketfs_lock);
    int idx = socketfs_find_free();
    if (idx < 0) {
        spinlock_release(&socketfs_lock);
        return -2;
    }

    socketfs_entry_t* entry = &socketfs_entries[idx];
    entry->domain = domain;
    entry->type = type;
    entry->protocol = protocol;
    entry->state = SOCKET_STATE_UNCONNECTED;
    entry->owner_pid = process_get_current_id();
    entry->recv_buf_len = 0;
    entry->send_buf_len = 0;

    entry->node = (vfs_node_t*)memory_alloc(sizeof(vfs_node_t));
    if (!entry->node) {
        spinlock_release(&socketfs_lock);
        return -3;
    }
    memset(entry->node, 0, sizeof(vfs_node_t));
    entry->node->type = FS_TYPE_SOCKET;
    entry->node->impl_data = (void*)(uintptr_t)idx;
    entry->node->ops.read = socketfs_read;
    entry->node->ops.write = socketfs_write;
    entry->node->nlink = 1;

    socketfs_count++;
    *fd = idx;
    spinlock_release(&socketfs_lock);
    return 0;
}

int socketfs_bind(int fd, const void* addr, uint32_t addr_len)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS) return -1;
    if (!socketfs_entries[fd].node) return -2;
    (void)addr; (void)addr_len;
    return 0;
}

int socketfs_listen(int fd, int backlog)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS) return -1;
    socketfs_entry_t* entry = &socketfs_entries[fd];
    if (!entry->node) return -2;
    entry->state = SOCKET_STATE_LISTENING;
    (void)backlog;
    return 0;
}

int socketfs_accept(int fd, void* addr, uint32_t* addr_len, int* new_fd)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS || !new_fd) return -1;
    socketfs_entry_t* entry = &socketfs_entries[fd];
    if (entry->state != SOCKET_STATE_LISTENING) return -2;

    return socketfs_create(entry->domain, entry->type, entry->protocol, new_fd);
}

int socketfs_connect(int fd, const void* addr, uint32_t addr_len)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS) return -1;
    socketfs_entry_t* entry = &socketfs_entries[fd];
    if (!entry->node) return -2;
    entry->state = SOCKET_STATE_CONNECTED;
    (void)addr; (void)addr_len;
    return 0;
}

int socketfs_send(int fd, const void* buf, uint32_t len, int flags)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS) return -1;
    socketfs_entry_t* entry = &socketfs_entries[fd];
    if (entry->state != SOCKET_STATE_CONNECTED) return -2;
    (void)buf; (void)flags;
    return (int)len;
}

int socketfs_recv(int fd, void* buf, uint32_t len, int flags)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS) return -1;
    socketfs_entry_t* entry = &socketfs_entries[fd];
    if (entry->state != SOCKET_STATE_CONNECTED) return -2;
    (void)buf; (void)len; (void)flags;
    return 0;
}

int socketfs_close(int fd)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS) return -1;

    spinlock_acquire(&socketfs_lock);
    socketfs_entry_t* entry = &socketfs_entries[fd];
    if (entry->node) {
        memory_free(entry->node);
        entry->node = NULL;
    }
    entry->state = SOCKET_STATE_CLOSING;
    socketfs_count--;
    spinlock_release(&socketfs_lock);
    return 0;
}

int socketfs_getsockopt(int fd, int level, int optname, void* optval, uint32_t* optlen)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS) return -1;
    (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}

int socketfs_setsockopt(int fd, int level, int optname, const void* optval, uint32_t optlen)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS) return -1;
    (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}

int socketfs_poll(int fd, int events, int* revents)
{
    if (fd < 0 || fd >= SOCKETFS_MAX_SOCKETS) return -1;
    if (revents) *revents = events;
    return 1;
}

vfs_node_t* socketfs_get_root(void)
{
    return &socketfs_root;
}