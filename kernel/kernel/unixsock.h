

#ifndef KERNEL_UNIXSOCK_H
#define KERNEL_UNIXSOCK_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "wait.h"

#define UNIX_SOCK_STREAM    1
#define UNIX_SOCK_DGRAM     2

#define UNIX_SOCK_MAX          128
#define UNIX_SOCK_PATH_MAX     108
#define UNIX_SOCK_BACKLOG_MAX  16
#define UNIX_SOCK_SEND_BUF     4096
#define UNIX_SOCK_RECV_BUF     4096

#define UNIX_SOCK_UNCONNECTED   0
#define UNIX_SOCK_LISTENING     1
#define UNIX_SOCK_CONNECTED     2
#define UNIX_SOCK_CONNECTING    3
#define UNIX_SOCK_CLOSED        4

#define UNIX_SOCK_SHUT_RD       0x01
#define UNIX_SOCK_SHUT_WR       0x02
#define UNIX_SOCK_PEER_CLOSED   0x04

struct unix_sock;

typedef struct unix_pending_conn {
    struct unix_sock*            client;
    struct unix_pending_conn*   next;
} unix_pending_conn_t;

typedef struct unix_sock {

    uint32_t    type;
    uint32_t    state;
    char        bind_path[UNIX_SOCK_PATH_MAX];
    int         bound;

    struct unix_sock* peer;

    uint8_t     send_buf[UNIX_SOCK_SEND_BUF];
    uint32_t    send_read_pos;
    uint32_t    send_write_pos;
    uint32_t    send_bytes;

    uint8_t     recv_buf[UNIX_SOCK_RECV_BUF];
    uint32_t    recv_read_pos;
    uint32_t    recv_write_pos;
    uint32_t    recv_bytes;

    spinlock_t  lock;
    wait_queue_t read_waiters;
    wait_queue_t write_waiters;
    wait_queue_t connect_waiters;

    uint32_t    ref_count;

    unix_pending_conn_t* pending_head;
    unix_pending_conn_t* pending_tail;
    uint32_t             pending_count;
    uint32_t             backlog;

    uint32_t    flags;
} unix_sock_t;

unix_sock_t* unix_socket_create(uint32_t type);

int unix_socket_bind(unix_sock_t* sock, const char* path);

int unix_socket_listen(unix_sock_t* sock, int backlog);

unix_sock_t* unix_socket_accept(unix_sock_t* listen_sock);

int unix_socket_connect(unix_sock_t* sock, const char* server_path);

int64_t unix_socket_send(unix_sock_t* sock, const void* buf, uint64_t count);

int64_t unix_socket_recv(unix_sock_t* sock, void* buf, uint64_t count);

int64_t unix_socket_sendto(unix_sock_t* sock, const void* buf, uint64_t count,
                           const char* dest_path);

int64_t unix_socket_recvfrom(unix_sock_t* sock, void* buf, uint64_t count,
                             char* src_path);

void unix_socket_close(unix_sock_t* sock);

int unix_socket_shutdown(unix_sock_t* sock, int how);

int unix_socket_getpeername(unix_sock_t* sock, char* path);

int unix_socket_getsockname(unix_sock_t* sock, char* path);

#endif
