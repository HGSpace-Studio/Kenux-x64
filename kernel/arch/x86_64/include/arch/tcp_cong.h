#ifndef ARCH_TCP_CONG_H
#define ARCH_TCP_CONG_H

#include <arch/types.h>
#include <arch/net.h>

#define TCP_CONG_NAME_MAX       16
#define TCP_CONG_ALGO_MAX       8

#define TCP_CA_OPEN             0
#define TCP_CA_DISORDER         1
#define TCP_CA_CWR              2
#define TCP_CA_RECOVERY         3
#define TCP_CA_LOSS             4

#define TCP_CONG_MAX_CWND       0xffffffff
#define TCP_INIT_CWND           10
#define TCP_INIT_SSTHRESH       0xffff
#define TCP_FRTO_THRESHOLD      1

typedef struct tcp_cong_ops {
    char name[TCP_CONG_NAME_MAX];
    void (*init)(struct tcp_socket* sk);
    uint32_t (*ssthresh)(struct tcp_socket* sk, uint32_t bytes_acked);
    void (*cong_avoid)(struct tcp_socket* sk, uint32_t ack, uint32_t acked);
    void (*cwnd_event)(struct tcp_socket* sk, uint32_t event);
    void (*set_state)(struct tcp_socket* sk, uint8_t new_state);
    uint32_t (*undo_cwnd)(struct tcp_socket* sk);
    struct tcp_cong_ops* next;
} tcp_cong_ops_t;

#define TCP_CONG_EVENT_OPEN        1
#define TCP_CONG_EVENT_RETRANS      2
#define TCP_CONG_EVENT_FRTO         3
#define TCP_CONG_EVENT_LOSS_TIMEOUT 4

void tcp_cong_init(void);
int tcp_cong_register_algorithm(tcp_cong_ops_t* algo);
int tcp_cong_set_default(const char* name);
tcp_cong_ops_t* tcp_cong_get_default(void);
tcp_cong_ops_t* tcp_cong_find(const char* name);

void tcp_cong_init_sock(struct tcp_socket* sk);
void tcp_cong_on_ack(struct tcp_socket* sk, uint32_t ack, uint32_t acked);
void tcp_cong_event(struct tcp_socket* sk, uint32_t event);
void tcp_cong_set_state(struct tcp_socket* sk, uint8_t new_state);
uint32_t tcp_cong_ssthresh(struct tcp_socket* sk, uint32_t bytes_acked);

extern tcp_cong_ops_t tcp_reno_ops;
extern tcp_cong_ops_t tcp_cubic_ops;
extern tcp_cong_ops_t tcp_bbr_ops;

#endif
