#ifndef KAPI_NETLINK_H
#define KAPI_NETLINK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_nl_sock kapi_nl_sock_t;

#define KAPI_NETLINK_ROUTE          0
#define KAPI_NETLINK_USERSOCK       2
#define KAPI_NETLINK_FIREWALL       3
#define KAPI_NETLINK_SELINUX        7
#define KAPI_NETLINK_GENERIC        16

#define KAPI_NLMSG_ALIGNTO          4
#define KAPI_NLMSG_HDRLEN           16

#define KAPI_NLMSG_NOOP             0x1
#define KAPI_NLMSG_ERROR            0x2
#define KAPI_NLMSG_DONE             0x3
#define KAPI_NLMSG_OVERRUN          0x4

#define KAPI_NLM_F_REQUEST          0x1
#define KAPI_NLM_F_MULTI            0x2
#define KAPI_NLM_F_ACK              0x4
#define KAPI_NLM_F_ECHO             0x8
#define KAPI_NLM_F_DUMP_INTR        0x10
#define KAPI_NLM_F_DUMP_FILTERED    0x20
#define KAPI_NLM_F_ROOT             0x100
#define KAPI_NLM_F_MATCH            0x200
#define KAPI_NLM_F_ATOMIC           0x400
#define KAPI_NLM_F_DUMP             (KAPI_NLM_F_ROOT | KAPI_NLM_F_MATCH)

typedef struct {
    uint32_t nlmsg_len;
    uint16_t nlmsg_type;
    uint16_t nlmsg_flags;
    uint32_t nlmsg_seq;
    uint32_t nlmsg_pid;
} kapi_nlmsghdr_t;

typedef struct {
    uint16_t nla_len;
    uint16_t nla_type;
} kapi_nlattr_t;

static inline kapi_nlmsghdr_t* kapi_nlmsg_hdr(void* buf)
{
    return (kapi_nlmsghdr_t*)buf;
}

static inline kapi_nlattr_t* kapi_nlmsg_attrdata(kapi_nlmsghdr_t* nlh, int hdrlen)
{
    return (kapi_nlattr_t*)((uint8_t*)nlh + KAPI_NLMSG_ALIGNTO * ((hdrlen + KAPI_NLMSG_ALIGNTO - 1) / KAPI_NLMSG_ALIGNTO));
}

static inline kapi_nlattr_t* kapi_nlmsg_attr(const kapi_nlmsghdr_t* nlh, int hdrlen)
{
    return (kapi_nlattr_t*)((uint8_t*)nlh + KAPI_NLMSG_HDRLEN + KAPI_NLMSG_ALIGNTO * ((hdrlen + KAPI_NLMSG_ALIGNTO - 1) / KAPI_NLMSG_ALIGNTO));
}

static inline int kapi_nlmsg_ok(const kapi_nlmsghdr_t* nlh, int remaining)
{
    return (remaining >= (int)sizeof(kapi_nlmsghdr_t) &&
            nlh->nlmsg_len >= sizeof(kapi_nlmsghdr_t) &&
            (int)nlh->nlmsg_len <= remaining);
}

static inline kapi_nlmsghdr_t* kapi_nlmsg_next(const kapi_nlmsghdr_t* nlh, int* remaining)
{
    int totlen = KAPI_NLMSG_ALIGNTO * ((nlh->nlmsg_len + KAPI_NLMSG_ALIGNTO - 1) / KAPI_NLMSG_ALIGNTO);
    *remaining -= totlen;
    return (kapi_nlmsghdr_t*)((uint8_t*)nlh + totlen);
}

kapi_nl_sock_t* kapi_netlink_socket_create(int protocol);

int kapi_netlink_socket_destroy(kapi_nl_sock_t* sk);

int kapi_netlink_bind(kapi_nl_sock_t* sk, uint32_t groups);

int kapi_netlink_send(kapi_nl_sock_t* sk, kapi_nlmsghdr_t* nlh, size_t len);

int kapi_netlink_recv(kapi_nl_sock_t* sk, void* buf, size_t len);

int kapi_netlink_sendmsg(kapi_nl_sock_t* sk, kapi_nlmsghdr_t* nlh,
                         uint16_t type, uint16_t flags, uint32_t seq, uint32_t pid);

int kapi_netlink_unicast(kapi_nl_sock_t* sk, kapi_nlmsghdr_t* nlh, uint32_t pid);

int kapi_netlink_broadcast(kapi_nl_sock_t* sk, kapi_nlmsghdr_t* nlh, uint32_t pid, uint32_t group);

#ifdef __cplusplus
}
#endif

#endif