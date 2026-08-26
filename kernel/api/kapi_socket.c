#include "kapi.h"

#include <arch/net.h>
#include <string.h>

typedef struct {
    int used;
    int so_reuseaddr;
    int so_keepalive;
    int so_broadcast;
    int so_error;
    uint32_t rcvbuf;
    uint32_t sndbuf;
    uint32_t rcvtimeo;
    uint32_t sndtimeo;
} kapi_socket_opt_t;

static kapi_socket_opt_t kapi_socket_opts[128];

int kapi_socket_init(void)
{
    memset(kapi_socket_opts, 0, sizeof(kapi_socket_opts));
    return KAPI_OK;
}

static int map_domain(int domain)
{
    if (domain == KAPI_AF_INET) return AF_INET;
    if (domain == KAPI_AF_UNIX) return AF_UNIX;
    return -1;
}

static int map_type(int type)
{
    if (type == KAPI_SOCK_STREAM) return SOCK_STREAM;
    if (type == KAPI_SOCK_DGRAM) return SOCK_DGRAM;
    if (type == KAPI_SOCK_RAW) return SOCK_RAW;
    return -1;
}

static int map_proto(int proto)
{
    if (proto == 0) return 0;
    if (proto == KAPI_IPPROTO_TCP) return IPPROTO_TCP;
    if (proto == KAPI_IPPROTO_UDP) return IPPROTO_UDP;
    return proto;
}

static void copy_addr_to_arch(sockaddr_in_t* dst, const kapi_sockaddr_t* src)
{
    memset(dst, 0, sizeof(*dst));
    dst->sin_family = src->in.sin_family;
    dst->sin_port = src->in.sin_port;
    dst->sin_addr = src->in.sin_addr;
}

static void copy_addr_from_arch(kapi_sockaddr_t* dst, const sockaddr_in_t* src)
{
    memset(dst, 0, sizeof(*dst));
    dst->in.sin_family = src->sin_family;
    dst->in.sin_port = src->sin_port;
    dst->in.sin_addr = src->sin_addr;
}

kapi_sockfd_t kapi_socket(int domain, int type, int protocol)
{
    int d = map_domain(domain);
    int t = map_type(type);
    int p = map_proto(protocol);
    if (d < 0 || t < 0) return KAPI_EINVAL;
    int fd = sys_socket(d, t, p);
    if (fd >= 0 && fd < 128) {
        memset(&kapi_socket_opts[fd], 0, sizeof(kapi_socket_opts[fd]));
        kapi_socket_opts[fd].used = 1;
        kapi_socket_opts[fd].rcvbuf = 8192;
        kapi_socket_opts[fd].sndbuf = 8192;
    }
    return fd;
}

int kapi_bind(kapi_sockfd_t sockfd, const kapi_sockaddr_t* addr, size_t addrlen)
{
    if (!addr || addrlen < sizeof(kapi_sockaddr_in_t)) return KAPI_EINVAL;
    sockaddr_in_t a;
    copy_addr_to_arch(&a, addr);
    return sys_bind(sockfd, &a, (uint32_t)sizeof(a)) == 0 ? KAPI_OK : KAPI_ERROR;
}

int kapi_listen(kapi_sockfd_t sockfd, int backlog)
{
    return sys_listen(sockfd, backlog) == 0 ? KAPI_OK : KAPI_ERROR;
}

kapi_sockfd_t kapi_accept(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen)
{
    sockaddr_in_t a;
    uint32_t len = sizeof(a);
    int fd = sys_accept(sockfd, &a, &len);
    if (fd >= 0 && addr) copy_addr_from_arch(addr, &a);
    if (fd >= 0 && addrlen) *addrlen = sizeof(kapi_sockaddr_in_t);
    if (fd >= 0 && fd < 128) {
        memset(&kapi_socket_opts[fd], 0, sizeof(kapi_socket_opts[fd]));
        kapi_socket_opts[fd].used = 1;
    }
    return fd;
}

int kapi_connect(kapi_sockfd_t sockfd, const kapi_sockaddr_t* addr, size_t addrlen)
{
    if (!addr || addrlen < sizeof(kapi_sockaddr_in_t)) return KAPI_EINVAL;
    sockaddr_in_t a;
    copy_addr_to_arch(&a, addr);
    return sys_connect(sockfd, &a, (uint32_t)sizeof(a)) == 0 ? KAPI_OK : KAPI_ERROR;
}

int64_t kapi_send(kapi_sockfd_t sockfd, const void* buf, size_t len, int flags)
{
    if (!buf && len) return KAPI_EINVAL;
    return sys_send(sockfd, buf, (uint64_t)len, flags);
}

int64_t kapi_recv(kapi_sockfd_t sockfd, void* buf, size_t len, int flags)
{
    if (!buf && len) return KAPI_EINVAL;
    return sys_recv(sockfd, buf, (uint64_t)len, flags);
}

int64_t kapi_sendto(kapi_sockfd_t sockfd, const void* buf, size_t len, int flags,
                    const kapi_sockaddr_t* dest_addr, size_t addrlen)
{
    if (!buf && len) return KAPI_EINVAL;
    if (!dest_addr || addrlen < sizeof(kapi_sockaddr_in_t)) return KAPI_EINVAL;
    sockaddr_in_t a;
    copy_addr_to_arch(&a, dest_addr);
    return sys_sendto(sockfd, buf, (uint64_t)len, flags, &a, (uint32_t)sizeof(a));
}

int64_t kapi_recvfrom(kapi_sockfd_t sockfd, void* buf, size_t len, int flags,
                      kapi_sockaddr_t* src_addr, size_t* addrlen)
{
    if (!buf && len) return KAPI_EINVAL;
    sockaddr_in_t a;
    uint32_t alen = sizeof(a);
    int ret = sys_recvfrom(sockfd, buf, (uint64_t)len, flags, src_addr ? &a : NULL, src_addr ? &alen : NULL);
    if (ret >= 0 && src_addr) copy_addr_from_arch(src_addr, &a);
    if (ret >= 0 && addrlen) *addrlen = sizeof(kapi_sockaddr_in_t);
    return ret;
}

int kapi_setsockopt(kapi_sockfd_t sockfd, int level, int optname,
                    const void* optval, size_t optlen)
{
    if (sockfd < 0 || sockfd >= 128 || !optval || optlen < sizeof(uint32_t)) return KAPI_EINVAL;
    if (level != KAPI_SOL_SOCKET) return KAPI_ENOSYS;
    uint32_t v = *(const uint32_t*)optval;
    kapi_socket_opts[sockfd].used = 1;
    switch (optname) {
        case KAPI_SO_REUSEADDR: kapi_socket_opts[sockfd].so_reuseaddr = v ? 1 : 0; return KAPI_OK;
        case KAPI_SO_KEEPALIVE: kapi_socket_opts[sockfd].so_keepalive = v ? 1 : 0; return KAPI_OK;
        case KAPI_SO_BROADCAST: kapi_socket_opts[sockfd].so_broadcast = v ? 1 : 0; return KAPI_OK;
        case KAPI_SO_RCVBUF: kapi_socket_opts[sockfd].rcvbuf = v; return KAPI_OK;
        case KAPI_SO_SNDBUF: kapi_socket_opts[sockfd].sndbuf = v; return KAPI_OK;
        case KAPI_SO_RCVTIMEO: kapi_socket_opts[sockfd].rcvtimeo = v; return KAPI_OK;
        case KAPI_SO_SNDTIMEO: kapi_socket_opts[sockfd].sndtimeo = v; return KAPI_OK;
        default: return KAPI_ENOSYS;
    }
}

int kapi_getsockopt(kapi_sockfd_t sockfd, int level, int optname,
                    void* optval, size_t* optlen)
{
    if (sockfd < 0 || sockfd >= 128 || !optval || !optlen || *optlen < sizeof(uint32_t)) return KAPI_EINVAL;
    if (level != KAPI_SOL_SOCKET) return KAPI_ENOSYS;
    uint32_t v;
    switch (optname) {
        case KAPI_SO_REUSEADDR: v = (uint32_t)kapi_socket_opts[sockfd].so_reuseaddr; break;
        case KAPI_SO_KEEPALIVE: v = (uint32_t)kapi_socket_opts[sockfd].so_keepalive; break;
        case KAPI_SO_BROADCAST: v = (uint32_t)kapi_socket_opts[sockfd].so_broadcast; break;
        case KAPI_SO_RCVBUF: v = kapi_socket_opts[sockfd].rcvbuf; break;
        case KAPI_SO_SNDBUF: v = kapi_socket_opts[sockfd].sndbuf; break;
        case KAPI_SO_RCVTIMEO: v = kapi_socket_opts[sockfd].rcvtimeo; break;
        case KAPI_SO_SNDTIMEO: v = kapi_socket_opts[sockfd].sndtimeo; break;
        case KAPI_SO_ERROR: v = (uint32_t)kapi_socket_opts[sockfd].so_error; break;
        default: return KAPI_ENOSYS;
    }
    *(uint32_t*)optval = v;
    *optlen = sizeof(uint32_t);
    return KAPI_OK;
}

int kapi_shutdown(kapi_sockfd_t sockfd, int how)
{
    return sys_shutdown(sockfd, how) == 0 ? KAPI_OK : KAPI_ERROR;
}

int kapi_close_socket(kapi_sockfd_t sockfd)
{
    int ret = sys_close_socket(sockfd);
    if (ret == 0 && sockfd >= 0 && sockfd < 128) {
        memset(&kapi_socket_opts[sockfd], 0, sizeof(kapi_socket_opts[sockfd]));
    }
    return ret == 0 ? KAPI_OK : KAPI_ERROR;
}

int kapi_getsockname(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen)
{
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return KAPI_ENOSYS;
}

int kapi_getpeername(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen)
{
    (void)sockfd;
    (void)addr;
    (void)addrlen;
    return KAPI_ENOSYS;
}

static void kapi_socket_copy_string(char* dst, size_t cap, const char* src)
{
    size_t i = 0;
    if (!dst || cap == 0) return;
    if (!src) src = "";
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int kapi_socket_starts_with(const char* s, const char* prefix)
{
    size_t i = 0;
    if (!s || !prefix) return 0;
    while (prefix[i]) {
        if (s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

int kapi_socket_tcp(void)
{
    return kapi_socket(KAPI_AF_INET, KAPI_SOCK_STREAM, KAPI_IPPROTO_TCP);
}

int kapi_socket_connect_host(int socket, const char* host, uint32_t port,
                             uint32_t timeout_ms,
                             kapi_net_socket_connect_t* result)
{
    if (!host || !result) return KAPI_EINVAL;
    memset(result, 0, sizeof(*result));
    result->socket = socket;
    kapi_socket_copy_string(result->host, sizeof(result->host), host);
    result->port = port;
    result->timeout_ms = timeout_ms;
    result->status = KAPI_NET_STATUS_PROTOCOL_UNSUPPORTED;
    return KAPI_ENOSYS;
}

long kapi_socket_send_timed(int socket, const void* buffer, uint32_t length,
                            uint32_t timeout_ms, uint32_t* status)
{
    (void)timeout_ms;
    int64_t ret = kapi_send(socket, buffer, length, 0);
    if (status) *status = ret >= 0 ? KAPI_NET_STATUS_OK : KAPI_NET_STATUS_SOCKET_CLOSED;
    return (long)ret;
}

long kapi_socket_recv_timed(int socket, void* buffer, uint32_t length,
                            uint32_t timeout_ms, uint32_t* status)
{
    (void)timeout_ms;
    int64_t ret = kapi_recv(socket, buffer, length, 0);
    if (status) *status = ret >= 0 ? KAPI_NET_STATUS_OK : KAPI_NET_STATUS_SOCKET_CLOSED;
    return (long)ret;
}

int kapi_socket_close_timed(int socket)
{
    return kapi_close_socket(socket);
}

int kapi_net_connections(kapi_net_connection_info_t* entries, uint32_t capacity,
                         uint32_t* out_count)
{
    if (!entries && capacity > 0) return KAPI_EINVAL;
    if (entries && capacity) memset(entries, 0, sizeof(*entries) * capacity);
    if (out_count) *out_count = 0;
    return KAPI_OK;
}

int kapi_net_http_get(const char* host, const char* path, uint32_t port,
                      uint32_t timeout_ms, kapi_net_http_get_t* result)
{
    if (!host || !path || !result) return KAPI_EINVAL;
    memset(result, 0, sizeof(*result));
    kapi_socket_copy_string(result->host, sizeof(result->host), host);
    kapi_socket_copy_string(result->path, sizeof(result->path), path);
    result->port = port;
    result->timeout_ms = timeout_ms;
    result->status = KAPI_NET_STATUS_NO_DEVICE;
    return KAPI_ENOSYS;
}

int kapi_http_request(const kapi_http_request_t* request,
                      kapi_http_response_t* response)
{
    if (!request || !request->url) return KAPI_EINVAL;
    if (response) {
        memset(response, 0, sizeof(*response));
        response->net_status = KAPI_NET_STATUS_NO_DEVICE;
        kapi_socket_copy_string(response->final_url, sizeof(response->final_url), request->url);
    }
    if (request->response_body && request->response_body_capacity) {
        request->response_body[0] = '\0';
    }
    if (request->response_headers && request->response_headers_capacity) {
        request->response_headers[0] = '\0';
    }
    return KAPI_ENOSYS;
}

int kapi_http_get(const char* url, uint32_t timeout_ms, char* response_body,
                  uint32_t response_body_capacity, char* response_headers,
                  uint32_t response_headers_capacity,
                  kapi_http_response_t* response)
{
    kapi_http_request_t req;
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = "GET";
    req.timeout_ms = timeout_ms;
    req.max_redirects = KAPI_HTTP_DEFAULT_REDIRECTS;
    req.response_body = response_body;
    req.response_body_capacity = response_body_capacity;
    req.response_headers = response_headers;
    req.response_headers_capacity = response_headers_capacity;
    return kapi_http_request(&req, response);
}

int kapi_http_download(const char* url, const char* output_path,
                       uint32_t timeout_ms, kapi_http_progress_fn progress,
                       void* context, kapi_http_response_t* response)
{
    (void)output_path;
    if (progress) progress(0, 1, context);
    int rc = kapi_http_get(url, timeout_ms, NULL, 0, NULL, 0, response);
    if (progress) progress(1, 1, context);
    return rc;
}

int kapi_http_resolve_url(const char* base_url, const char* location,
                          char* out, uint32_t capacity)
{
    if (!location || !out || capacity == 0) return KAPI_EINVAL;
    if (kapi_socket_starts_with(location, "http://") ||
        kapi_socket_starts_with(location, "https://")) {
        kapi_socket_copy_string(out, capacity, location);
        return KAPI_OK;
    }
    if (!base_url) return KAPI_EINVAL;

    uint32_t pos = 0;
    if (location[0] == '/') {
        const char* slash = base_url;
        uint32_t slash_count = 0;
        while (*slash && pos + 1 < capacity) {
            out[pos++] = *slash;
            if (*slash == '/') {
                slash_count++;
                if (slash_count == 3) break;
            }
            slash++;
        }
    } else {
        uint32_t last_slash = 0;
        while (base_url[pos] && pos + 1 < capacity) {
            out[pos] = base_url[pos];
            if (base_url[pos] == '/') last_slash = pos;
            pos++;
        }
        pos = last_slash + 1;
    }
    for (uint32_t i = 0; location[i] && pos + 1 < capacity; i++) {
        out[pos++] = location[i];
    }
    out[pos] = '\0';
    return KAPI_OK;
}
