

#include "unixsock.h"
#include "wait.h"
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

extern void* kmalloc(size_t size);
extern void  kfree(void* ptr);
extern void  process_yield(void);

static unix_sock_t* g_unix_sockets[UNIX_SOCK_MAX];
static spinlock_t   g_unix_table_lock;

static unix_sock_t* find_bound_socket(const char* path)
{
    int i;
    for (i = 0; i < UNIX_SOCK_MAX; i++) {
        if (g_unix_sockets[i] && g_unix_sockets[i]->bound) {
            if (strncmp(g_unix_sockets[i]->bind_path, path, UNIX_SOCK_PATH_MAX) == 0) {
                return g_unix_sockets[i];
            }
        }
    }
    return NULL;
}

static int register_socket(unix_sock_t* sock)
{
    int i;
    spin_lock(&g_unix_table_lock);
    for (i = 0; i < UNIX_SOCK_MAX; i++) {
        if (!g_unix_sockets[i]) {
            g_unix_sockets[i] = sock;
            spin_unlock(&g_unix_table_lock);
            return 0;
        }
    }
    spin_unlock(&g_unix_table_lock);
    return -1;
}

static void unregister_socket(unix_sock_t* sock)
{
    int i;
    spin_lock(&g_unix_table_lock);
    for (i = 0; i < UNIX_SOCK_MAX; i++) {
        if (g_unix_sockets[i] == sock) {
            g_unix_sockets[i] = NULL;
            break;
        }
    }
    spin_unlock(&g_unix_table_lock);
}

static uint32_t ring_write(uint8_t* buf, uint32_t size,
                           uint32_t* rpos, uint32_t* wpos, uint32_t* bytes,
                           const uint8_t* data, uint32_t count)
{
    uint32_t space, to_copy, contiguous, first_chunk;

    space = size - *bytes;
    if (count > space)
        count = space;
    if (count == 0)
        return 0;

    contiguous = size - *wpos;
    if (contiguous >= count) {
        memcpy(buf + *wpos, data, count);
        *wpos += count;
    } else {
        first_chunk = contiguous;
        memcpy(buf + *wpos, data, first_chunk);
        memcpy(buf, data + first_chunk, count - first_chunk);
        *wpos = count - first_chunk;
    }

    *bytes += count;
    return count;
}

static uint32_t ring_read(uint8_t* buf, uint32_t size,
                          uint32_t* rpos, uint32_t* wpos, uint32_t* bytes,
                          uint8_t* data, uint32_t count)
{
    uint32_t to_copy, contiguous, first_chunk;

    if (count > *bytes)
        count = *bytes;
    if (count == 0)
        return 0;

    contiguous = size - *rpos;
    if (contiguous >= count) {
        memcpy(data, buf + *rpos, count);
        *rpos += count;
    } else {
        first_chunk = contiguous;
        memcpy(data, buf + *rpos, first_chunk);
        memcpy(data + first_chunk, buf, count - first_chunk);
        *rpos = count - first_chunk;
    }

    *bytes -= count;
    return count;
}

unix_sock_t* unix_socket_create(uint32_t type)
{
    unix_sock_t* sock;

    if (type != UNIX_SOCK_STREAM && type != UNIX_SOCK_DGRAM)
        return NULL;

    sock = (unix_sock_t*)kmalloc(sizeof(unix_sock_t));
    if (!sock)
        return NULL;

    sock->type  = type;
    sock->state = UNIX_SOCK_UNCONNECTED;
    memset(sock->bind_path, 0, UNIX_SOCK_PATH_MAX);
    sock->bound = 0;

    sock->peer = NULL;

    memset(sock->send_buf, 0, UNIX_SOCK_SEND_BUF);
    sock->send_read_pos  = 0;
    sock->send_write_pos = 0;
    sock->send_bytes     = 0;

    memset(sock->recv_buf, 0, UNIX_SOCK_RECV_BUF);
    sock->recv_read_pos  = 0;
    sock->recv_write_pos = 0;
    sock->recv_bytes     = 0;

    spin_init(&sock->lock);
    wait_queue_init(&sock->read_waiters);
    wait_queue_init(&sock->write_waiters);
    wait_queue_init(&sock->connect_waiters);

    sock->ref_count = 1;

    sock->pending_head  = NULL;
    sock->pending_tail  = NULL;
    sock->pending_count = 0;
    sock->backlog       = 0;

    sock->flags = 0;

    if (register_socket(sock) != 0) {
        kfree(sock);
        return NULL;
    }

    return sock;
}

int unix_socket_bind(unix_sock_t* sock, const char* path)
{
    if (!sock || !path)
        return -1;

    if (sock->bound)
        return -1;

    spin_lock(&g_unix_table_lock);

    if (find_bound_socket(path) != NULL) {
        spin_unlock(&g_unix_table_lock);
        return -1;
    }

    strncpy(sock->bind_path, path, UNIX_SOCK_PATH_MAX - 1);
    sock->bind_path[UNIX_SOCK_PATH_MAX - 1] = '\0';
    sock->bound = 1;

    spin_unlock(&g_unix_table_lock);
    return 0;
}

int unix_socket_listen(unix_sock_t* sock, int backlog)
{
    if (!sock)
        return -1;

    if (sock->type != UNIX_SOCK_STREAM)
        return -1;

    if (!sock->bound)
        return -1;

    spin_lock(&sock->lock);

    if (backlog <= 0 || backlog > UNIX_SOCK_BACKLOG_MAX)
        backlog = UNIX_SOCK_BACKLOG_MAX;

    sock->backlog       = (uint32_t)backlog;
    sock->pending_head  = NULL;
    sock->pending_tail  = NULL;
    sock->pending_count = 0;
    sock->state         = UNIX_SOCK_LISTENING;

    spin_unlock(&sock->lock);
    return 0;
}

unix_sock_t* unix_socket_accept(unix_sock_t* listen_sock)
{
    unix_sock_t*       new_sock;
    unix_pending_conn_t* pending;
    unix_sock_t*       client;

    if (!listen_sock)
        return NULL;

    spin_lock(&listen_sock->lock);

    if (listen_sock->state != UNIX_SOCK_LISTENING) {
        spin_unlock(&listen_sock->lock);
        return NULL;
    }

    while (listen_sock->pending_head == NULL) {

        if (listen_sock->state == UNIX_SOCK_CLOSED) {
            spin_unlock(&listen_sock->lock);
            return NULL;
        }
        wait_queue_wait(&listen_sock->connect_waiters, &listen_sock->lock);
    }

    pending = listen_sock->pending_head;
    listen_sock->pending_head = pending->next;
    if (listen_sock->pending_head == NULL)
        listen_sock->pending_tail = NULL;
    listen_sock->pending_count--;

    client = pending->client;

    spin_unlock(&listen_sock->lock);

    kfree(pending);

    new_sock = unix_socket_create(UNIX_SOCK_STREAM);
    if (!new_sock)
        return NULL;

    spin_lock(&new_sock->lock);
    spin_lock(&client->lock);

    new_sock->peer  = client;
    client->peer    = new_sock;
    new_sock->state = UNIX_SOCK_CONNECTED;
    client->state   = UNIX_SOCK_CONNECTED;

    client->ref_count++;
    new_sock->ref_count++;

    spin_unlock(&client->lock);
    spin_unlock(&new_sock->lock);

    return new_sock;
}

int unix_socket_connect(unix_sock_t* sock, const char* server_path)
{
    unix_sock_t*       server;
    unix_pending_conn_t* node;

    if (!sock || !server_path)
        return -1;

    if (sock->type == UNIX_SOCK_STREAM) {

        spin_lock(&g_unix_table_lock);
        server = find_bound_socket(server_path);
        if (server) {

            server->ref_count++;
        }
        spin_unlock(&g_unix_table_lock);

        if (!server || server->type != UNIX_SOCK_STREAM)
            return -1;

        spin_lock(&server->lock);

        if (server->state != UNIX_SOCK_LISTENING) {
            spin_unlock(&server->lock);
            server->ref_count--;
            return -1;
        }

        if (server->pending_count >= server->backlog) {
            spin_unlock(&server->lock);
            server->ref_count--;
            return -1;
        }

        node = (unix_pending_conn_t*)kmalloc(sizeof(unix_pending_conn_t));
        if (!node) {
            spin_unlock(&server->lock);
            server->ref_count--;
            return -1;
        }
        node->client = sock;
        node->next   = NULL;

        if (server->pending_tail)
            server->pending_tail->next = node;
        else
            server->pending_head = node;
        server->pending_tail = node;
        server->pending_count++;

        sock->state = UNIX_SOCK_CONNECTING;

        wait_queue_wake_one(&server->connect_waiters);

        spin_unlock(&server->lock);

        spin_lock(&sock->lock);
        while (sock->state == UNIX_SOCK_CONNECTING) {
            if (sock->flags & UNIX_SOCK_PEER_CLOSED) {
                spin_unlock(&sock->lock);
                return -1;
            }
            wait_queue_wait(&sock->connect_waiters, &sock->lock);
        }
        spin_unlock(&sock->lock);

        server->ref_count--;

        return (sock->state == UNIX_SOCK_CONNECTED) ? 0 : -1;
    }

    if (sock->type == UNIX_SOCK_DGRAM) {

        spin_lock(&g_unix_table_lock);
        server = find_bound_socket(server_path);
        if (server) {
            server->ref_count++;
        }
        spin_unlock(&g_unix_table_lock);

        if (!server || server->type != UNIX_SOCK_DGRAM) {
            return -1;
        }

        spin_lock(&sock->lock);
        sock->peer  = server;
        sock->state = UNIX_SOCK_CONNECTED;
        spin_unlock(&sock->lock);

        return 0;
    }

    return -1;
}

int64_t unix_socket_send(unix_sock_t* sock, const void* buf, uint64_t count)
{
    if (!sock || !buf || count == 0)
        return -1;

    if (!sock->peer)
        return -1;

    if (sock->flags & UNIX_SOCK_SHUT_WR)
        return -1;

    if (sock->type == UNIX_SOCK_STREAM) {

        unix_sock_t* peer = sock->peer;
        uint32_t written = 0;
        const uint8_t* data = (const uint8_t*)buf;
        uint32_t remaining = (uint32_t)count;

        while (remaining > 0) {
            uint32_t was_empty;

            spin_lock(&peer->lock);

            if (peer->flags & (UNIX_SOCK_PEER_CLOSED | UNIX_SOCK_SHUT_RD)) {

                spin_unlock(&peer->lock);
                spin_lock(&sock->lock);
                sock->flags |= UNIX_SOCK_PEER_CLOSED;
                spin_unlock(&sock->lock);
                return (written > 0) ? (int64_t)written : -1;
            }

            uint32_t space = UNIX_SOCK_RECV_BUF - peer->recv_bytes;
            if (space == 0) {

                wait_queue_wait(&peer->read_waiters, &peer->lock);
                continue;
            }

            was_empty = (peer->recv_bytes == 0) ? 1 : 0;

            uint32_t n = ring_write(peer->recv_buf, UNIX_SOCK_RECV_BUF,
                                    &peer->recv_read_pos, &peer->recv_write_pos,
                                    &peer->recv_bytes,
                                    data + written, remaining);

            written += n;
            remaining -= n;

            if (was_empty && n > 0) {
                wait_queue_wake_all(&peer->read_waiters);
            }

            spin_unlock(&peer->lock);
        }

        return (int64_t)written;
    }

    if (sock->type == UNIX_SOCK_DGRAM) {

        unix_sock_t* peer = sock->peer;

        spin_lock(&peer->lock);

        if (UNIX_SOCK_RECV_BUF - peer->recv_bytes < (uint32_t)count) {
            spin_unlock(&peer->lock);
            return -1;
        }

        uint32_t was_empty = (peer->recv_bytes == 0) ? 1 : 0;
        ring_write(peer->recv_buf, UNIX_SOCK_RECV_BUF,
                   &peer->recv_read_pos, &peer->recv_write_pos,
                   &peer->recv_bytes,
                   (const uint8_t*)buf, (uint32_t)count);

        if (was_empty) {
            wait_queue_wake_all(&peer->read_waiters);
        }

        spin_unlock(&peer->lock);
        return (int64_t)count;
    }

    return -1;
}

int64_t unix_socket_recv(unix_sock_t* sock, void* buf, uint64_t count)
{
    uint32_t n;

    if (!sock || !buf || count == 0)
        return -1;

    if (sock->flags & UNIX_SOCK_SHUT_RD)
        return -1;

    spin_lock(&sock->lock);

    while (sock->recv_bytes == 0) {

        if (sock->flags & UNIX_SOCK_PEER_CLOSED) {
            spin_unlock(&sock->lock);
            return 0;
        }

        wait_queue_wait(&sock->read_waiters, &sock->lock);
    }

    n = ring_read(sock->recv_buf, UNIX_SOCK_RECV_BUF,
                  &sock->recv_read_pos, &sock->recv_write_pos,
                  &sock->recv_bytes,
                  (uint8_t*)buf, (uint32_t)count);

    if (n > 0) {
        wait_queue_wake_all(&sock->write_waiters);
    }

    spin_unlock(&sock->lock);
    return (int64_t)n;
}

int64_t unix_socket_sendto(unix_sock_t* sock, const void* buf, uint64_t count,
                           const char* dest_path)
{
    unix_sock_t* dest;

    if (!sock || !buf || count == 0 || !dest_path)
        return -1;

    if (sock->type != UNIX_SOCK_DGRAM)
        return -1;

    spin_lock(&g_unix_table_lock);
    dest = find_bound_socket(dest_path);
    if (dest) {
        dest->ref_count++;
    }
    spin_unlock(&g_unix_table_lock);

    if (!dest) {
        return -1;
    }

    spin_lock(&dest->lock);

    if (UNIX_SOCK_RECV_BUF - dest->recv_bytes < (uint32_t)count) {
        spin_unlock(&dest->lock);
        dest->ref_count--;
        return -1;
    }

    uint32_t was_empty = (dest->recv_bytes == 0) ? 1 : 0;
    ring_write(dest->recv_buf, UNIX_SOCK_RECV_BUF,
               &dest->recv_read_pos, &dest->recv_write_pos,
               &dest->recv_bytes,
               (const uint8_t*)buf, (uint32_t)count);

    if (was_empty) {
        wait_queue_wake_all(&dest->read_waiters);
    }

    spin_unlock(&dest->lock);

    dest->ref_count--;
    return (int64_t)count;
}

int64_t unix_socket_recvfrom(unix_sock_t* sock, void* buf, uint64_t count,
                             char* src_path)
{
    uint32_t n;

    if (!sock || !buf || count == 0)
        return -1;

    if (sock->type != UNIX_SOCK_DGRAM)
        return -1;

    spin_lock(&sock->lock);

    while (sock->recv_bytes == 0) {

        if (sock->flags & UNIX_SOCK_PEER_CLOSED) {
            spin_unlock(&sock->lock);
            return 0;
        }
        wait_queue_wait(&sock->read_waiters, &sock->lock);
    }

    n = ring_read(sock->recv_buf, UNIX_SOCK_RECV_BUF,
                  &sock->recv_read_pos, &sock->recv_write_pos,
                  &sock->recv_bytes,
                  (uint8_t*)buf, (uint32_t)count);

    if (n > 0) {
        wait_queue_wake_all(&sock->write_waiters);
    }

    spin_unlock(&sock->lock);

    if (src_path && sock->peer && sock->peer->bound) {
        strncpy(src_path, sock->peer->bind_path, UNIX_SOCK_PATH_MAX);
    } else if (src_path) {
        src_path[0] = '\0';
    }

    return (int64_t)n;
}

int unix_socket_shutdown(unix_sock_t* sock, int how)
{
    if (!sock)
        return -1;

    spin_lock(&sock->lock);

    sock->flags |= (how & (UNIX_SOCK_SHUT_RD | UNIX_SOCK_SHUT_WR));

    if (sock->peer) {
        spin_lock(&sock->peer->lock);
        sock->peer->flags |= UNIX_SOCK_PEER_CLOSED;

        wait_queue_wake_all(&sock->peer->read_waiters);
        wait_queue_wake_all(&sock->peer->write_waiters);
        spin_unlock(&sock->peer->lock);
    }

    wait_queue_wake_all(&sock->read_waiters);
    wait_queue_wake_all(&sock->write_waiters);

    spin_unlock(&sock->lock);
    return 0;
}

void unix_socket_close(unix_sock_t* sock)
{
    unix_pending_conn_t* node, *next_node;

    if (!sock)
        return;

    spin_lock(&sock->lock);

    sock->state = UNIX_SOCK_CLOSED;

    if (sock->peer) {
        spin_lock(&sock->peer->lock);
        sock->peer->flags |= UNIX_SOCK_PEER_CLOSED;
        wait_queue_wake_all(&sock->peer->read_waiters);
        wait_queue_wake_all(&sock->peer->write_waiters);

        if (__sync_sub_and_fetch(&sock->peer->ref_count, 1) == 0) {

            sock->peer->state = UNIX_SOCK_CLOSED;
        }
        sock->peer = NULL;
        spin_unlock(&sock->peer->lock);
    }

    wait_queue_wake_all(&sock->read_waiters);
    wait_queue_wake_all(&sock->write_waiters);
    wait_queue_wake_all(&sock->connect_waiters);

    node = sock->pending_head;
    while (node) {
        next_node = node->next;

        if (node->client) {
            spin_lock(&node->client->lock);
            node->client->state = UNIX_SOCK_UNCONNECTED;
            node->client->flags |= UNIX_SOCK_PEER_CLOSED;
            wait_queue_wake_all(&node->client->connect_waiters);
            spin_unlock(&node->client->lock);
        }
        kfree(node);
        node = next_node;
    }
    sock->pending_head  = NULL;
    sock->pending_tail  = NULL;
    sock->pending_count = 0;

    spin_unlock(&sock->lock);

    unregister_socket(sock);

    if (__sync_sub_and_fetch(&sock->ref_count, 1) == 0) {
        kfree(sock);
    }
}

int unix_socket_getpeername(unix_sock_t* sock, char* path)
{
    if (!sock || !path)
        return -1;

    spin_lock(&sock->lock);

    if (!sock->peer || !sock->peer->bound) {
        spin_unlock(&sock->lock);
        return -1;
    }

    strncpy(path, sock->peer->bind_path, UNIX_SOCK_PATH_MAX);

    spin_unlock(&sock->lock);
    return 0;
}

int unix_socket_getsockname(unix_sock_t* sock, char* path)
{
    if (!sock || !path)
        return -1;

    spin_lock(&sock->lock);

    if (!sock->bound) {
        spin_unlock(&sock->lock);
        return -1;
    }

    strncpy(path, sock->bind_path, UNIX_SOCK_PATH_MAX);

    spin_unlock(&sock->lock);
    return 0;
}
