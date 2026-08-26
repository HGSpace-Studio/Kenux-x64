#include <arch/tcp_cong.h>
#include <arch/slab.h>
#include <string.h>

static tcp_cong_ops_t* tcp_cong_algos[TCP_CONG_ALGO_MAX];
static tcp_cong_ops_t* tcp_cong_default;
static spinlock_t tcp_cong_lock = SPINLOCK_INIT;

static uint32_t tcp_reno_ssthresh(struct tcp_socket* sk, uint32_t bytes_acked)
{
    (void)bytes_acked;
    return sk->snd_wnd / 2;
}

static void tcp_reno_cong_avoid(struct tcp_socket* sk, uint32_t ack, uint32_t acked)
{
    (void)ack;
    if (!sk->snd_wnd) return;
    if (sk->snd_wnd < TCP_INIT_SSTHRESH) {
        uint32_t new_cwnd = sk->snd_wnd + acked;
        if (new_cwnd > TCP_INIT_SSTHRESH)
            new_cwnd = TCP_INIT_SSTHRESH;
        sk->snd_wnd = new_cwnd;
    } else {
        uint32_t incr = TCP_MSS * acked / sk->snd_wnd;
        if (incr == 0) incr = 1;
        sk->snd_wnd += incr;
    }
}

static void tcp_reno_cwnd_event(struct tcp_socket* sk, uint32_t event)
{
    if (event == TCP_CONG_EVENT_RETRANS || event == TCP_CONG_EVENT_LOSS_TIMEOUT) {
        sk->snd_wnd = 1;
    } else if (event == TCP_CONG_EVENT_FRTO) {
        sk->snd_wnd = TCP_INIT_CWND;
    }
}

static void tcp_reno_set_state(struct tcp_socket* sk, uint8_t new_state)
{
    (void)sk;
    (void)new_state;
}

static uint32_t tcp_reno_undo_cwnd(struct tcp_socket* sk)
{
    (void)sk;
    return TCP_INIT_SSTHRESH;
}

static void tcp_reno_init(struct tcp_socket* sk)
{
    (void)sk;
}

tcp_cong_ops_t tcp_reno_ops = {
    .name = "reno",
    .init = tcp_reno_init,
    .ssthresh = tcp_reno_ssthresh,
    .cong_avoid = tcp_reno_cong_avoid,
    .cwnd_event = tcp_reno_cwnd_event,
    .set_state = tcp_reno_set_state,
    .undo_cwnd = tcp_reno_undo_cwnd,
    .next = NULL,
};

#define TCP_CUBIC_C             410
#define TCP_CUBIC_BETA          717
#define TCP_CUBIC_SCALE         100
#define TCP_CUBIC_SCALE_SHIFT   8
#define TCP_CUBIC_MAX_INTERVAL  30000

static uint64_t cubic_root(uint64_t x)
{
    if (x == 0) return 0;
    uint64_t lo = 0, hi = 2097152;
    while (lo < hi) {
        uint64_t mid = (lo + hi + 1) / 2;
        if (mid * mid * mid <= x)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

typedef struct {
    uint32_t w_last;
    uint32_t w_max;
    uint32_t epoch_start;
    uint32_t ack_cnt;
    uint32_t tcp_jiffies;
    uint32_t bictcp_origin_point;
    uint32_t bictcp_K;
    uint32_t delayed_ack;
    uint32_t cnt;
    uint32_t last_cwnd;
    uint32_t last_max;
    uint32_t last_wnd;
    uint8_t  bictcp_found;
    struct tcp_socket* sk;
} cubic_state_t;

#define TCP_CUBIC_SLOT_MAX 64
static cubic_state_t cubic_slots[TCP_CUBIC_SLOT_MAX];
static spinlock_t cubic_lock = SPINLOCK_INIT;
static int cubic_slot_next = 0;

static cubic_state_t* cubic_get_slot(void)
{
    spin_lock(&cubic_lock);
    int idx = cubic_slot_next % TCP_CUBIC_SLOT_MAX;
    cubic_slot_next++;
    spin_unlock(&cubic_lock);
    return &cubic_slots[idx];
}

static cubic_state_t* cubic_find_state(struct tcp_socket* sk)
{
    for (int i = 0; i < TCP_CUBIC_SLOT_MAX; i++) {
        if (cubic_slots[i].sk == sk)
            return &cubic_slots[i];
    }
    return NULL;
}

static void tcp_cubic_init(struct tcp_socket* sk)
{
    cubic_state_t* ca = cubic_get_slot();
    ca->sk = sk;
    ca->w_last = 0;
    ca->w_max = TCP_INIT_SSTHRESH;
    ca->epoch_start = 0;
    ca->ack_cnt = 0;
    ca->tcp_jiffies = 0;
    ca->bictcp_origin_point = 0;
    ca->bictcp_K = 0;
    ca->delayed_ack = 2;
    ca->cnt = 0;
    ca->last_cwnd = TCP_INIT_CWND;
    ca->last_max = 0;
    ca->last_wnd = TCP_INIT_CWND;
    ca->bictcp_found = 0;
}

static uint32_t tcp_cubic_ssthresh(struct tcp_socket* sk, uint32_t bytes_acked)
{
    (void)bytes_acked;
    return sk->snd_wnd * (uint32_t)TCP_CUBIC_BETA / 1000;
}

static void tcp_cubic_cong_avoid(struct tcp_socket* sk, uint32_t ack, uint32_t acked)
{
    (void)ack;
    if (sk->snd_wnd < TCP_INIT_SSTHRESH) {
        uint32_t new_cwnd = sk->snd_wnd + acked;
        if (new_cwnd > TCP_INIT_SSTHRESH)
            new_cwnd = TCP_INIT_SSTHRESH;
        sk->snd_wnd = new_cwnd;
        return;
    }
    cubic_state_t* ca = cubic_find_state(sk);
    if (!ca) return;
    uint32_t delta = 0;
    if (ca->epoch_start == 0) {
        ca->epoch_start = ack;
        ca->bictcp_origin_point = sk->snd_wnd;
        if (sk->snd_wnd < ca->w_max) {
            ca->bictcp_K = cubic_root((ca->w_max - sk->snd_wnd) * 1000000 * TCP_CUBIC_SCALE / TCP_CUBIC_C);
        } else {
            ca->bictcp_K = 0;
        }
        ca->ack_cnt = 1;
        ca->tcp_jiffies = ack;
    } else {
        ca->ack_cnt++;
    }
    if (ca->bictcp_found) {
        sk->snd_wnd = ca->last_wnd + 1;
        ca->bictcp_found = 0;
        return;
    }
    uint32_t t = (ack - ca->tcp_jiffies) + ca->bictcp_K;
    if (t < ca->tcp_jiffies)
        t = ca->tcp_jiffies + 1;
    uint64_t target = (uint64_t)ca->bictcp_origin_point +
        ((uint64_t)TCP_CUBIC_C * (uint64_t)(t - ca->tcp_jiffies) * (uint64_t)(t - ca->tcp_jiffies)) / 1000000ULL;
    if (target > 0xffffffff)
        target = 0xffffffff;
    if (target < (uint64_t)sk->snd_wnd) {
        delta = (uint32_t)((sk->snd_wnd - (uint32_t)target) * TCP_CUBIC_SCALE / ca->delayed_ack / TCP_MSS);
    } else {
        delta = (uint32_t)((target - sk->snd_wnd) * TCP_CUBIC_SCALE / ca->delayed_ack / TCP_MSS);
    }
    if (delta < 1) delta = 1;
    ca->ack_cnt %= (uint32_t)(TCP_MSS / delta);
    if (ca->ack_cnt == 0) {
        if (sk->snd_wnd < TCP_CONG_MAX_CWND)
            sk->snd_wnd++;
        ca->last_wnd = sk->snd_wnd;
    }
}

static void tcp_cubic_cwnd_event(struct tcp_socket* sk, uint32_t event)
{
    if (event == TCP_CONG_EVENT_RETRANS || event == TCP_CONG_EVENT_LOSS_TIMEOUT) {
        cubic_state_t* ca = cubic_find_state(sk);
        if (!ca) return;
        if (sk->snd_wnd < ca->w_max)
            ca->w_max = sk->snd_wnd * (uint32_t)TCP_CUBIC_BETA / 1000;
        else
            ca->w_max = sk->snd_wnd;
        sk->snd_wnd = TCP_INIT_CWND;
        ca->epoch_start = 0;
    }
}

static void tcp_cubic_set_state(struct tcp_socket* sk, uint8_t new_state)
{
    (void)sk;
    (void)new_state;
}

static uint32_t tcp_cubic_undo_cwnd(struct tcp_socket* sk)
{
    (void)sk;
    return TCP_INIT_SSTHRESH;
}

tcp_cong_ops_t tcp_cubic_ops = {
    .name = "cubic",
    .init = tcp_cubic_init,
    .ssthresh = tcp_cubic_ssthresh,
    .cong_avoid = tcp_cubic_cong_avoid,
    .cwnd_event = tcp_cubic_cwnd_event,
    .set_state = tcp_cubic_set_state,
    .undo_cwnd = tcp_cubic_undo_cwnd,
    .next = NULL,
};

#define BBR_UNIT            1024
#define BBR_HIGH_GAIN       2885
#define BBR_DRAIN_GAIN      733
#define BBR_PROBE_RTT_GAIN  256
#define BBR_CYCLE_LEN       8
#define BBR_MIN_RTT_WIN     10000
#define BBR_PROBE_RTT_DUR   200

typedef struct {
    uint64_t min_rtt_us;
    uint64_t min_rtt_stamp;
    uint64_t probe_rtt_done_stamp;
    uint64_t pacing_rate;
    uint64_t max_bw;
    uint64_t target_cwnd;
    uint64_t delivered;
    uint64_t delivered_mstamp;
    uint64_t cycle_mstamp;
    uint64_t full_bw;
    uint64_t full_bw_cnt;
    uint32_t cycle_idx;
    uint32_t mode;
    uint32_t filled_pipe;
    uint32_t bw_cnt;
    uint64_t bw_samples[20];
    uint32_t bw_head;
    uint32_t has_seen_rtt;
    struct tcp_socket* sk;
} bbr_cong_state_t;

#define BBR_CONG_SLOT_MAX 64
static bbr_cong_state_t bbr_cong_slots[BBR_CONG_SLOT_MAX];
static spinlock_t bbr_cong_lock = SPINLOCK_INIT;
static int bbr_cong_slot_next = 0;

static bbr_cong_state_t* bbr_cong_get_slot(void)
{
    spin_lock(&bbr_cong_lock);
    int idx = bbr_cong_slot_next % BBR_CONG_SLOT_MAX;
    bbr_cong_slot_next++;
    spin_unlock(&bbr_cong_lock);
    return &bbr_cong_slots[idx];
}

static bbr_cong_state_t* bbr_find_state(struct tcp_socket* sk)
{
    for (int i = 0; i < BBR_CONG_SLOT_MAX; i++) {
        if (bbr_cong_slots[i].sk == sk)
            return &bbr_cong_slots[i];
    }
    return NULL;
}

static uint64_t bbr_bw_from_delta(uint64_t delta, uint64_t time_us)
{
    if (!time_us) return 0xffffffffffffffffULL;
    return (delta * 1000000ULL) / time_us;
}

static void bbr_update_bw(bbr_cong_state_t* bbr, uint64_t delivered, uint64_t interval_us)
{
    uint64_t bw = bbr_bw_from_delta(delivered, interval_us);
    bbr->bw_samples[bbr->bw_head] = bw;
    bbr->bw_head = (bbr->bw_head + 1) % 20;
    if (bbr->bw_cnt < 20) bbr->bw_cnt++;
    uint64_t maxb = 0;
    for (uint32_t i = 0; i < bbr->bw_cnt; i++) {
        if (bbr->bw_samples[i] > maxb) maxb = bbr->bw_samples[i];
    }
    bbr->max_bw = maxb;
}

static uint64_t bbr_bdp(bbr_cong_state_t* bbr)
{
    uint64_t rtt = bbr->min_rtt_us ? bbr->min_rtt_us : 2000;
    return (bbr->max_bw * rtt) / 1000000ULL;
}

static void bbr_update_target_cwnd(bbr_cong_state_t* bbr)
{
    uint64_t cwnd = 0;
    if (bbr->mode == 0) {
        cwnd = (bbr_bdp(bbr) * BBR_HIGH_GAIN) / BBR_UNIT;
    } else if (bbr->mode == 1) {
        cwnd = (bbr_bdp(bbr) * BBR_DRAIN_GAIN) / BBR_UNIT;
    } else if (bbr->mode == 2) {
        static const uint32_t gains[BBR_CYCLE_LEN] = {
            BBR_UNIT * 5 / 4, BBR_UNIT, BBR_UNIT, BBR_UNIT,
            BBR_UNIT, BBR_UNIT, BBR_UNIT * 3 / 4, BBR_UNIT
        };
        uint32_t g = gains[bbr->cycle_idx % BBR_CYCLE_LEN];
        cwnd = (bbr_bdp(bbr) * g) / BBR_UNIT;
    } else {
        cwnd = TCP_MSS * 4;
    }
    if (cwnd < TCP_MSS * 4) cwnd = TCP_MSS * 4;
    bbr->target_cwnd = cwnd;
}

static void tcp_bbr_init(struct tcp_socket* sk)
{
    bbr_cong_state_t* bbr = bbr_cong_get_slot();
    memset(bbr, 0, sizeof(bbr_cong_state_t));
    bbr->sk = sk;
    bbr->mode = 0;
    bbr->cycle_idx = 0;
    bbr->filled_pipe = 0;
    bbr->pacing_rate = 0;
    bbr->target_cwnd = TCP_MSS * 4;
}

static uint32_t tcp_bbr_ssthresh(struct tcp_socket* sk, uint32_t bytes_acked)
{
    (void)bytes_acked;
    bbr_cong_state_t* bbr = bbr_find_state(sk);
    if (!bbr) return TCP_INIT_SSTHRESH;
    if (bbr->max_bw && bbr->min_rtt_us) {
        uint64_t bdp = bbr_bdp(bbr);
        return (uint32_t)(bdp * BBR_DRAIN_GAIN / BBR_UNIT);
    }
    return TCP_INIT_SSTHRESH;
}

static void tcp_bbr_cong_avoid(struct tcp_socket* sk, uint32_t ack, uint32_t acked)
{
    bbr_cong_state_t* bbr = bbr_find_state(sk);
    (void)ack;
    if (!bbr) return;
    if (acked == 0) return;
    bbr->delivered += acked;
    uint64_t now = 0;
    uint64_t interval = (now - bbr->delivered_mstamp);
    if (interval == 0) interval = 1;
    bbr_update_bw(bbr, acked, interval);
    bbr->delivered_mstamp = now;
    if (!bbr->max_bw) return;
    if (!bbr->filled_pipe) {
        if (bbr->max_bw >= bbr->full_bw * 125 / 100) {
            bbr->full_bw = bbr->max_bw;
            bbr->full_bw_cnt = 0;
        } else {
            bbr->full_bw_cnt++;
            if (bbr->full_bw_cnt >= 3)
                bbr->filled_pipe = 1;
        }
    }
    if (bbr->mode == 0 && bbr->filled_pipe) {
        bbr->mode = 1;
        bbr->cycle_idx = 0;
    }
    if (bbr->mode == 1 && sk->snd_wnd == 0) {
        bbr->mode = 2;
        bbr->cycle_idx = 1;
    }
    bbr_update_target_cwnd(bbr);
    if (bbr->target_cwnd > TCP_CONG_MAX_CWND)
        bbr->target_cwnd = TCP_CONG_MAX_CWND;
    sk->snd_wnd = (uint32_t)bbr->target_cwnd;
}

static void tcp_bbr_cwnd_event(struct tcp_socket* sk, uint32_t event)
{
    if (event == TCP_CONG_EVENT_RETRANS || event == TCP_CONG_EVENT_LOSS_TIMEOUT) {
        bbr_cong_state_t* bbr = bbr_find_state(sk);
        if (!bbr) return;
        bbr->mode = 0;
        bbr->filled_pipe = 0;
        bbr->full_bw = 0;
        bbr->full_bw_cnt = 0;
        sk->snd_wnd = TCP_INIT_CWND;
    }
}

static void tcp_bbr_set_state(struct tcp_socket* sk, uint8_t new_state)
{
    (void)sk;
    (void)new_state;
}

static uint32_t tcp_bbr_undo_cwnd(struct tcp_socket* sk)
{
    (void)sk;
    return TCP_INIT_SSTHRESH;
}

tcp_cong_ops_t tcp_bbr_ops = {
    .name = "bbr",
    .init = tcp_bbr_init,
    .ssthresh = tcp_bbr_ssthresh,
    .cong_avoid = tcp_bbr_cong_avoid,
    .cwnd_event = tcp_bbr_cwnd_event,
    .set_state = tcp_bbr_set_state,
    .undo_cwnd = tcp_bbr_undo_cwnd,
    .next = NULL,
};

void tcp_cong_init(void)
{
    spin_init(&tcp_cong_lock);
    for (int i = 0; i < TCP_CONG_ALGO_MAX; i++) {
        tcp_cong_algos[i] = NULL;
    }
    tcp_cong_register_algorithm(&tcp_reno_ops);
    tcp_cong_register_algorithm(&tcp_cubic_ops);
    tcp_cong_register_algorithm(&tcp_bbr_ops);
    tcp_cong_set_default("cubic");
}

int tcp_cong_register_algorithm(tcp_cong_ops_t* algo)
{
    if (!algo) return -1;
    spin_lock(&tcp_cong_lock);
    for (int i = 0; i < TCP_CONG_ALGO_MAX; i++) {
        if (tcp_cong_algos[i] == NULL) {
            tcp_cong_algos[i] = algo;
            algo->next = NULL;
            spin_unlock(&tcp_cong_lock);
            return 0;
        }
    }
    spin_unlock(&tcp_cong_lock);
    return -1;
}

int tcp_cong_set_default(const char* name)
{
    if (!name) return -1;
    spin_lock(&tcp_cong_lock);
    for (int i = 0; i < TCP_CONG_ALGO_MAX; i++) {
        if (tcp_cong_algos[i] && strcmp(tcp_cong_algos[i]->name, name) == 0) {
            tcp_cong_default = tcp_cong_algos[i];
            spin_unlock(&tcp_cong_lock);
            return 0;
        }
    }
    spin_unlock(&tcp_cong_lock);
    return -1;
}

tcp_cong_ops_t* tcp_cong_get_default(void)
{
    return tcp_cong_default;
}

tcp_cong_ops_t* tcp_cong_find(const char* name)
{
    if (!name) return NULL;
    spin_lock(&tcp_cong_lock);
    for (int i = 0; i < TCP_CONG_ALGO_MAX; i++) {
        if (tcp_cong_algos[i] && strcmp(tcp_cong_algos[i]->name, name) == 0) {
            spin_unlock(&tcp_cong_lock);
            return tcp_cong_algos[i];
        }
    }
    spin_unlock(&tcp_cong_lock);
    return NULL;
}

void tcp_cong_init_sock(struct tcp_socket* sk)
{
    if (!sk) return;
    tcp_cong_ops_t* ops = tcp_cong_get_default();
    if (!ops) return;
    sk->snd_wnd = TCP_INIT_CWND;
    if (ops->init)
        ops->init(sk);
}

void tcp_cong_on_ack(struct tcp_socket* sk, uint32_t ack, uint32_t acked)
{
    if (!sk) return;
    tcp_cong_ops_t* ops = tcp_cong_get_default();
    if (!ops || !ops->cong_avoid) return;
    ops->cong_avoid(sk, ack, acked);
}

void tcp_cong_event(struct tcp_socket* sk, uint32_t event)
{
    if (!sk) return;
    tcp_cong_ops_t* ops = tcp_cong_get_default();
    if (!ops || !ops->cwnd_event) return;
    ops->cwnd_event(sk, event);
}

void tcp_cong_set_state(struct tcp_socket* sk, uint8_t new_state)
{
    if (!sk) return;
    tcp_cong_ops_t* ops = tcp_cong_get_default();
    if (!ops || !ops->set_state) return;
    ops->set_state(sk, new_state);
}

uint32_t tcp_cong_ssthresh(struct tcp_socket* sk, uint32_t bytes_acked)
{
    if (!sk) return TCP_INIT_SSTHRESH;
    tcp_cong_ops_t* ops = tcp_cong_get_default();
    if (!ops || !ops->ssthresh) return TCP_INIT_SSTHRESH;
    return ops->ssthresh(sk, bytes_acked);
}
