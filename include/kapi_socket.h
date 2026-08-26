#ifndef KAPI_SOCKET_H
#define KAPI_SOCKET_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int kapi_sockfd_t;

#define KAPI_AF_UNIX         1
#define KAPI_AF_INET         2

#define KAPI_SOCK_STREAM     1
#define KAPI_SOCK_DGRAM      2
#define KAPI_SOCK_RAW        3

#define KAPI_IPPROTO_TCP     6
#define KAPI_IPPROTO_UDP     17

#define KAPI_SOL_SOCKET      1

#define KAPI_SO_REUSEADDR     2
#define KAPI_SO_KEEPALIVE     9
#define KAPI_SO_BROADCAST     6
#define KAPI_SO_RCVBUF        8
#define KAPI_SO_SNDBUF        7
#define KAPI_SO_RCVTIMEO      20
#define KAPI_SO_SNDTIMEO      21
#define KAPI_SO_ERROR         4

#define KAPI_SHUT_RD           0
#define KAPI_SHUT_WR           1
#define KAPI_SHUT_RDWR         2

#define KAPI_MSG_PEEK          0x02
#define KAPI_MSG_DONTWAIT      0x40
#define KAPI_MSG_WAITALL       0x100

typedef struct {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t  sin_zero[8];
} kapi_sockaddr_in_t;

typedef struct {
    uint16_t sun_family;
    char     sun_path[108];
} kapi_sockaddr_un_t;

typedef union {
    kapi_sockaddr_in_t in;
    kapi_sockaddr_un_t un;
} kapi_sockaddr_t;

kapi_sockfd_t kapi_socket(int domain, int type, int protocol);

int kapi_bind(kapi_sockfd_t sockfd, const kapi_sockaddr_t* addr, size_t addrlen);

int kapi_listen(kapi_sockfd_t sockfd, int backlog);

kapi_sockfd_t kapi_accept(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen);

int kapi_connect(kapi_sockfd_t sockfd, const kapi_sockaddr_t* addr, size_t addrlen);

int64_t kapi_send(kapi_sockfd_t sockfd, const void* buf, size_t len, int flags);

int64_t kapi_recv(kapi_sockfd_t sockfd, void* buf, size_t len, int flags);

int64_t kapi_sendto(kapi_sockfd_t sockfd, const void* buf, size_t len, int flags,
                    const kapi_sockaddr_t* dest_addr, size_t addrlen);

int64_t kapi_recvfrom(kapi_sockfd_t sockfd, void* buf, size_t len, int flags,
                      kapi_sockaddr_t* src_addr, size_t* addrlen);

int kapi_setsockopt(kapi_sockfd_t sockfd, int level, int optname,
                    const void* optval, size_t optlen);

int kapi_getsockopt(kapi_sockfd_t sockfd, int level, int optname,
                    void* optval, size_t* optlen);

int kapi_shutdown(kapi_sockfd_t sockfd, int how);

int kapi_close_socket(kapi_sockfd_t sockfd);

int kapi_getsockname(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen);

int kapi_getpeername(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen);

/* ===== Fused LeonOS net.h socket layer + http.h ===== */
#define KAPI_NET_HOSTNAME_LEN      128U
#define KAPI_NET_HTTP_PATH_LEN     256U
#define KAPI_NET_HTTP_RESPONSE_MAX 4096U
#define KAPI_NET_SOCKET_MAX        16U
#define KAPI_NET_TCP_CLOSED        0U
#define KAPI_NET_TCP_ESTABLISHED   2U

#define KAPI_HTTP_URL_LEN             256U
#define KAPI_HTTP_CONTENT_TYPE_LEN    64U
#define KAPI_HTTP_DEFAULT_TIMEOUT_MS  10000U
#define KAPI_HTTP_DEFAULT_REDIRECTS   5U
#define KAPI_HTTP_NO_REDIRECTS        0xffffffffU
#define KAPI_HTTP_FLAG_TRUNCATED      0x00000001U
#define KAPI_HTTP_FLAG_CHUNKED        0x00000002U
#define KAPI_HTTP_FLAG_REDIRECTED     0x00000004U
#define KAPI_HTTP_FLAG_CONTENT_LENGTH 0x00000008U

typedef int (*kapi_http_progress_fn)(uint32_t received, uint32_t total, void* context);

typedef struct {
    int32_t  socket;
    char     host[KAPI_NET_HOSTNAME_LEN];
    uint32_t port;
    uint32_t timeout_ms;
    uint32_t status;
    uint32_t remote_ip;
    uint32_t local_ip;
    uint32_t local_port;
} kapi_net_socket_connect_t;

typedef struct {
    char     host[KAPI_NET_HOSTNAME_LEN];
    char     path[KAPI_NET_HTTP_PATH_LEN];
    uint32_t port;
    uint32_t timeout_ms;
    uint32_t status;
    uint32_t remote_ip;
    uint32_t http_status;
    uint32_t response_len;
    char     response[KAPI_NET_HTTP_RESPONSE_MAX];
} kapi_net_http_get_t;

typedef struct {
    int32_t  socket;
    uint32_t owner_pid;
    uint32_t state;
    uint32_t status;
    uint32_t local_ip;
    uint32_t remote_ip;
    uint32_t local_port;
    uint32_t remote_port;
    uint32_t age_ms;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
} kapi_net_connection_info_t;

typedef struct {
    const char* url;
    const char* method;
    const char* extra_headers;
    const char* request_body;
    uint32_t    request_body_len;
    uint32_t    timeout_ms;
    uint32_t    max_redirects;
    char*       response_body;
    uint32_t    response_body_capacity;
    char*       response_headers;
    uint32_t    response_headers_capacity;
} kapi_http_request_t;

typedef struct {
    uint32_t net_status;
    uint32_t http_status;
    uint32_t flags;
    uint32_t body_len;
    uint32_t headers_len;
    uint32_t content_length;
    uint32_t redirect_count;
    char     content_type[KAPI_HTTP_CONTENT_TYPE_LEN];
    char     final_url[KAPI_HTTP_URL_LEN];
} kapi_http_response_t;

/* Fused socket helpers (delegate to kapi_socket/kapi_connect/kapi_send/...) */
int   kapi_socket_tcp(void);
int   kapi_socket_connect_host(int socket, const char* host, uint32_t port,
                               uint32_t timeout_ms,
                               kapi_net_socket_connect_t* result);
long  kapi_socket_send_timed(int socket, const void* buffer, uint32_t length,
                             uint32_t timeout_ms, uint32_t* status);
long  kapi_socket_recv_timed(int socket, void* buffer, uint32_t length,
                             uint32_t timeout_ms, uint32_t* status);
int   kapi_socket_close_timed(int socket);
int   kapi_net_connections(kapi_net_connection_info_t* entries, uint32_t capacity,
                           uint32_t* out_count);
int   kapi_net_http_get(const char* host, const char* path, uint32_t port,
                        uint32_t timeout_ms, kapi_net_http_get_t* result);

/* HTTP layer */
int   kapi_http_request(const kapi_http_request_t* request,
                        kapi_http_response_t* response);
int   kapi_http_get(const char* url, uint32_t timeout_ms, char* response_body,
                    uint32_t response_body_capacity, char* response_headers,
                    uint32_t response_headers_capacity,
                    kapi_http_response_t* response);
int   kapi_http_download(const char* url, const char* output_path,
                         uint32_t timeout_ms, kapi_http_progress_fn progress,
                         void* context, kapi_http_response_t* response);
int   kapi_http_resolve_url(const char* base_url, const char* location,
                            char* out, uint32_t capacity);

/* LeonOS compat aliases (thin wrappers over fused API) */
#define KAPI_Socket_TCP()            kapi_socket_tcp()
#define KAPI_Socket_Close(s)         kapi_socket_close_timed((s))
#define KAPI_Net_Connections(e,c,o)  kapi_net_connections((e),(c),(o))
#define KAPI_Net_HTTPGet(h,p,po,t,r) kapi_net_http_get((h),(p),(po),(t),(r))
#define KAPI_Socket_Connect(s,h,po,t,r) kapi_socket_connect_host((s),(h),(po),(t),(r))
#define KAPI_Socket_Send(s,b,l,t,st)    kapi_socket_send_timed((s),(b),(l),(t),(st))
#define KAPI_Socket_Recv(s,b,l,t,st)    kapi_socket_recv_timed((s),(b),(l),(t),(st))
#define KAPI_HTTP_Request(r,rs)      kapi_http_request((r),(rs))
#define KAPI_HTTP_Get(u,t,b,bc,h,hc,r) kapi_http_get((u),(t),(b),(bc),(h),(hc),(r))
#define KAPI_HTTP_Download(u,o,t,p,c,r) kapi_http_download((u),(o),(t),(p),(c),(r))
#define KAPI_HTTP_ResolveURL(b,l,o,c)   kapi_http_resolve_url((b),(l),(o),(c))
typedef kapi_http_progress_fn          KAPI_HTTP_DOWNLOAD_PROGRESS_FN;
typedef kapi_net_socket_connect_t       KAPI_NET_SOCKET_CONNECT;
typedef kapi_net_http_get_t             KAPI_NET_HTTP_GET;
typedef kapi_net_connection_info_t      KAPI_NET_CONNECTION_INFO;
typedef kapi_http_request_t             KAPI_HTTP_REQUEST;
typedef kapi_http_response_t            KAPI_HTTP_RESPONSE;

#ifdef __cplusplus
}
#endif

#endif