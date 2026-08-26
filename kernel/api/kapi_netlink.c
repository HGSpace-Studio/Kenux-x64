#include "kapi.h"
#include "kapi_memory.h"
#include <string.h>

#define KAPI_NL_MAX_SOCKS 64
#define KAPI_NL_QUEUE_MAX 32

typedef struct kapi_nl_msg {
    size_t len;
    uint8_t* data;
    struct kapi_nl_msg* next;
} kapi_nl_msg_t;

struct kapi_nl_sock {
    int used;
    int protocol;
    uint32_t pid;
    uint32_t groups;
    uint32_t qlen;
    kapi_nl_msg_t* head;
    kapi_nl_msg_t* tail;
};

static struct kapi_nl_sock nl_socks[KAPI_NL_MAX_SOCKS];
static uint32_t nl_next_pid = 1;

static uint32_t nl_align(uint32_t value)
{
    return (value + KAPI_NLMSG_ALIGNTO - 1) & ~(KAPI_NLMSG_ALIGNTO - 1);
}

static int nl_valid_protocol(int protocol)
{
    return protocol == KAPI_NETLINK_ROUTE ||
           protocol == KAPI_NETLINK_USERSOCK ||
           protocol == KAPI_NETLINK_FIREWALL ||
           protocol == KAPI_NETLINK_SELINUX ||
           protocol == KAPI_NETLINK_GENERIC;
}

static int nl_enqueue(struct kapi_nl_sock* sk, const void* data, size_t len)
{
    if (!sk || !sk->used || !data || len < sizeof(kapi_nlmsghdr_t)) {
        return KAPI_EINVAL;
    }
    if (sk->qlen >= KAPI_NL_QUEUE_MAX) {
        return KAPI_EAGAIN;
    }

    kapi_nl_msg_t* msg = (kapi_nl_msg_t*)kapi_malloc(sizeof(kapi_nl_msg_t));
    if (!msg) {
        return KAPI_ENOMEM;
    }

    msg->data = (uint8_t*)kapi_malloc(len);
    if (!msg->data) {
        kapi_free(msg);
        return KAPI_ENOMEM;
    }

    memcpy(msg->data, data, len);
    msg->len = len;
    msg->next = NULL;

    if (sk->tail) {
        sk->tail->next = msg;
    } else {
        sk->head = msg;
    }
    sk->tail = msg;
    sk->qlen++;
    return (int)len;
}

static void nl_flush(struct kapi_nl_sock* sk)
{
    if (!sk) {
        return;
    }

    kapi_nl_msg_t* msg = sk->head;
    while (msg) {
        kapi_nl_msg_t* next = msg->next;
        kapi_free(msg->data);
        kapi_free(msg);
        msg = next;
    }

    sk->head = NULL;
    sk->tail = NULL;
    sk->qlen = 0;
}

static struct kapi_nl_sock* nl_find_pid(uint32_t pid, int protocol)
{
    for (int i = 0; i < KAPI_NL_MAX_SOCKS; i++) {
        if (nl_socks[i].used && nl_socks[i].pid == pid && nl_socks[i].protocol == protocol) {
            return &nl_socks[i];
        }
    }
    return NULL;
}

int kapi_netlink_init(void)
{
    memset(nl_socks, 0, sizeof(nl_socks));
    nl_next_pid = 1;
    return KAPI_OK;
}

kapi_nl_sock_t* kapi_netlink_socket_create(int protocol)
{
    if (!nl_valid_protocol(protocol)) {
        return NULL;
    }

    for (int i = 0; i < KAPI_NL_MAX_SOCKS; i++) {
        if (!nl_socks[i].used) {
            memset(&nl_socks[i], 0, sizeof(nl_socks[i]));
            nl_socks[i].used = 1;
            nl_socks[i].protocol = protocol;
            nl_socks[i].pid = nl_next_pid++;
            if (nl_next_pid == 0) {
                nl_next_pid = 1;
            }
            return &nl_socks[i];
        }
    }

    return NULL;
}

int kapi_netlink_socket_destroy(kapi_nl_sock_t* sk)
{
    if (!sk || !sk->used) {
        return KAPI_EINVAL;
    }

    nl_flush(sk);
    memset(sk, 0, sizeof(*sk));
    return KAPI_OK;
}

int kapi_netlink_bind(kapi_nl_sock_t* sk, uint32_t groups)
{
    if (!sk || !sk->used) {
        return KAPI_EINVAL;
    }

    sk->groups = groups;
    return KAPI_OK;
}

int kapi_netlink_send(kapi_nl_sock_t* sk, kapi_nlmsghdr_t* nlh, size_t len)
{
    if (!sk || !sk->used || !nlh || len < sizeof(kapi_nlmsghdr_t)) {
        return KAPI_EINVAL;
    }
    if (nlh->nlmsg_len < sizeof(kapi_nlmsghdr_t) || nlh->nlmsg_len > len) {
        return KAPI_EINVAL;
    }

    nlh->nlmsg_pid = sk->pid;
    return nl_enqueue(sk, nlh, nl_align(nlh->nlmsg_len));
}

int kapi_netlink_recv(kapi_nl_sock_t* sk, void* buf, size_t len)
{
    if (!sk || !sk->used || !buf) {
        return KAPI_EINVAL;
    }
    if (!sk->head) {
        return KAPI_EAGAIN;
    }

    kapi_nl_msg_t* msg = sk->head;
    if (len < msg->len) {
        return KAPI_EINVAL;
    }

    memcpy(buf, msg->data, msg->len);
    sk->head = msg->next;
    if (!sk->head) {
        sk->tail = NULL;
    }
    sk->qlen--;

    int ret = (int)msg->len;
    kapi_free(msg->data);
    kapi_free(msg);
    return ret;
}

int kapi_netlink_sendmsg(kapi_nl_sock_t* sk, kapi_nlmsghdr_t* nlh,
                         uint16_t type, uint16_t flags, uint32_t seq, uint32_t pid)
{
    if (!sk || !sk->used || !nlh) {
        return KAPI_EINVAL;
    }

    nlh->nlmsg_type = type;
    nlh->nlmsg_flags = flags;
    nlh->nlmsg_seq = seq;
    nlh->nlmsg_pid = pid ? pid : sk->pid;
    return nl_enqueue(sk, nlh, nl_align(nlh->nlmsg_len));
}

int kapi_netlink_unicast(kapi_nl_sock_t* sk, kapi_nlmsghdr_t* nlh, uint32_t pid)
{
    if (!sk || !sk->used || !nlh) {
        return KAPI_EINVAL;
    }

    struct kapi_nl_sock* dst = nl_find_pid(pid, sk->protocol);
    if (!dst) {
        return KAPI_ENOENT;
    }

    nlh->nlmsg_pid = sk->pid;
    return nl_enqueue(dst, nlh, nl_align(nlh->nlmsg_len));
}

int kapi_netlink_broadcast(kapi_nl_sock_t* sk, kapi_nlmsghdr_t* nlh, uint32_t pid, uint32_t group)
{
    if (!sk || !sk->used || !nlh || group == 0) {
        return KAPI_EINVAL;
    }

    int delivered = 0;
    int first_error = KAPI_OK;
    nlh->nlmsg_pid = sk->pid;

    for (int i = 0; i < KAPI_NL_MAX_SOCKS; i++) {
        if (!nl_socks[i].used || nl_socks[i].protocol != sk->protocol) {
            continue;
        }
        if (pid != 0 && nl_socks[i].pid == pid) {
            continue;
        }
        if ((nl_socks[i].groups & group) == 0) {
            continue;
        }

        int ret = nl_enqueue(&nl_socks[i], nlh, nl_align(nlh->nlmsg_len));
        if (ret >= 0) {
            delivered++;
        } else if (first_error == KAPI_OK) {
            first_error = ret;
        }
    }

    if (delivered > 0) {
        return delivered;
    }
    return first_error != KAPI_OK ? first_error : KAPI_ENOENT;
}
