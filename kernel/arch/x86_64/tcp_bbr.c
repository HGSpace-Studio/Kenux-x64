#include <arch/net.h>
#include <arch/slab.h>
#include <string.h>

/* timer_get_jiffies is in kernel/timer.c */
extern uint64_t timer_get_jiffies(void);

/* Timer tick period in milliseconds */
#define TIMER_TICK_MS 1

#define BBR_HIGH_GAIN       2885
#define BBR_DRAIN_GAIN      733
#define BBR_PROBE_RTT_GAIN  256
#define BBR_UNIT            1024
#define BBR_CYCLE_LEN       8
#define BBR_MIN_RTT_WIN     10000
#define BBR_PROBE_RTT_DUR   200
#define BBR_MIN_RTT_US      1
#define BBR_INF_BW          0xFFFFFFFFFFFFFFFFULL
#define BBR_MAX_BW_WIN      10
#define BBR_RTT_LEN         20
#define BBR_BW_LEN          20
#define BBR_PACING_MARGIN   1

typedef struct bbr_state {
    uint64_t min_rtt_us;
    uint64_t min_rtt_stamp;
    uint64_t probe_rtt_done_stamp;
    uint64_t prior_cwnd;
    uint64_t full_bw;
    uint64_t full_bw_cnt;
    uint64_t pacing_rate;
    uint64_t send_elapsed;
    uint64_t ack_elapsed;
    uint64_t next_rtt_delivered;
    uint64_t cycle_mstamp;
    uint64_t cycle_idx;
    uint64_t target_cwnd;
    uint64_t max_bw;
    uint64_t bw_samples[BBR_BW_LEN];
    uint64_t bw_ts[BBR_BW_LEN];
    uint32_t bw_head;
    uint32_t bw_cnt;
    uint64_t rtt_samples[BBR_RTT_LEN];
    uint32_t rtt_head;
    uint32_t rtt_cnt;
    uint32_t mode;
    uint32_t filled_pipe;
    uint32_t round_start;
    uint32_t idle_restart;
    uint32_t packet_conservation;
    uint32_t prev_ca_state;
    uint32_t loss_round_start;
    uint32_t loss_in_cycle;
    uint32_t lt_use_bw;
    uint64_t lt_bw;
    uint64_t lt_last_delivered;
    uint64_t lt_last_stamp;
    uint64_t lt_rtt_cnt;
    uint32_t has_seen_rtt;
    uint32_t has_seen_bw;
    uint64_t snd_nxt;
    uint64_t delivered;
    uint64_t delivered_mstamp;
    uint64_t app_limited;
    uint64_t last_sent_mstamp;
    tcp_socket_t* sock;
} bbr_state_t;

static inline uint64_t min_u64(uint64_t a, uint64_t b) { return a < b ? a : b; }
static inline uint64_t max_u64(uint64_t a, uint64_t b) { return a > b ? a : b; }

static uint64_t bbr_bw_from_delta(uint64_t delta, uint64_t time)
{
    if (!time) return BBR_INF_BW;
    return (delta * 1000000ULL) / time;
}

static void bbr_update_bw(bbr_state_t* bbr, uint64_t delivered, uint64_t interval_us)
{
    uint64_t bw = bbr_bw_from_delta(delivered, interval_us);
    bbr->bw_samples[bbr->bw_head] = bw;
    bbr->bw_ts[bbr->bw_head] = timer_get_jiffies();
    bbr->bw_head = (bbr->bw_head + 1) % BBR_BW_LEN;
    if (bbr->bw_cnt < BBR_BW_LEN) bbr->bw_cnt++;
    uint64_t maxb = 0;
    for (uint32_t i = 0; i < bbr->bw_cnt; i++) {
        if (bbr->bw_samples[i] > maxb) maxb = bbr->bw_samples[i];
    }
    bbr->max_bw = maxb;
}

static void bbr_update_min_rtt(bbr_state_t* bbr, uint64_t rtt_us)
{
    if (!bbr->has_seen_rtt || rtt_us < bbr->min_rtt_us) {
        bbr->min_rtt_us = rtt_us;
        bbr->min_rtt_stamp = timer_get_jiffies();
        bbr->has_seen_rtt = 1;
    }
    bbr->rtt_samples[bbr->rtt_head] = rtt_us;
    bbr->rtt_head = (bbr->rtt_head + 1) % BBR_RTT_LEN;
    if (bbr->rtt_cnt < BBR_RTT_LEN) bbr->rtt_cnt++;
    uint64_t now = timer_get_jiffies();
    if ((now - bbr->min_rtt_stamp) * TIMER_TICK_MS > BBR_MIN_RTT_WIN) {
        uint64_t minr = bbr->rtt_samples[0];
        for (uint32_t i = 1; i < bbr->rtt_cnt; i++) {
            if (bbr->rtt_samples[i] < minr) minr = bbr->rtt_samples[i];
        }
        bbr->min_rtt_us = minr;
        bbr->min_rtt_stamp = now;
    }
}

static void bbr_init_pacing_rate(bbr_state_t* bbr)
{
    uint64_t srtt = bbr->min_rtt_us ? bbr->min_rtt_us : 2000;
    bbr->pacing_rate = (TCP_MSS * 1000000ULL) / srtt;
}

static void bbr_set_pacing_rate(bbr_state_t* bbr, uint32_t gain)
{
    uint64_t rate = (bbr->max_bw * gain) / BBR_UNIT;
    if (bbr->filled_pipe || rate > bbr->pacing_rate) {
        bbr->pacing_rate = rate;
    }
}

static void bbr_enter_startup(bbr_state_t* bbr)
{
    bbr->mode = 0;
    bbr->cycle_idx = 0;
    bbr->full_bw = 0;
    bbr->full_bw_cnt = 0;
    bbr->filled_pipe = 0;
    bbr->packet_conservation = 0;
    bbr->pacing_rate = 0;
    bbr_init_pacing_rate(bbr);
    bbr->cycle_mstamp = timer_get_jiffies();
}

static void bbr_enter_drain(bbr_state_t* bbr)
{
    bbr->mode = 1;
    bbr->cycle_idx = 0;
}

static void bbr_enter_probe_bw(bbr_state_t* bbr)
{
    bbr->mode = 2;
    bbr->cycle_idx = 1;
    bbr->cycle_mstamp = timer_get_jiffies();
    bbr->filled_pipe = 1;
}

static void bbr_enter_probe_rtt(bbr_state_t* bbr)
{
    bbr->mode = 3;
    bbr->probe_rtt_done_stamp = 0;
}

static void bbr_check_full_bw_reached(bbr_state_t* bbr)
{
    if (bbr->filled_pipe) return;
    if (!bbr->max_bw) return;
    if (bbr->max_bw >= bbr->full_bw * 125 / 100) {
        bbr->full_bw = bbr->max_bw;
        bbr->full_bw_cnt = 0;
        return;
    }
    bbr->full_bw_cnt++;
    if (bbr->full_bw_cnt >= 3) {
        bbr->filled_pipe = 1;
    }
}

static void bbr_check_drain_done(bbr_state_t* bbr, uint32_t packets_out)
{
    if (bbr->mode == 1 && packets_out == 0) {
        bbr_enter_probe_bw(bbr);
    }
}

static void bbr_update_cycle_phase(bbr_state_t* bbr, uint64_t now)
{
    if (bbr->mode != 2) return;
    uint64_t cycle_elapsed = (now - bbr->cycle_mstamp) * TIMER_TICK_MS;
    if (cycle_elapsed > (BBR_MIN_RTT_WIN / BBR_CYCLE_LEN)) {
        bbr->cycle_idx = (bbr->cycle_idx + 1) % BBR_CYCLE_LEN;
        bbr->cycle_mstamp = now;
    }
}

static void bbr_update_mode(bbr_state_t* bbr, uint32_t packets_out)
{
    uint64_t now = timer_get_jiffies();
    if (bbr->mode == 0 && bbr->filled_pipe) {
        bbr_enter_drain(bbr);
    }
    if (bbr->mode == 1) {
        bbr_check_drain_done(bbr, packets_out);
    }
    if (bbr->mode == 2 || bbr->mode == 3) {
        if ((now - bbr->min_rtt_stamp) * TIMER_TICK_MS > BBR_MIN_RTT_WIN) {
            bbr_enter_probe_rtt(bbr);
        }
    }
    if (bbr->mode == 3) {
        if (!bbr->probe_rtt_done_stamp) {
            bbr->probe_rtt_done_stamp = now + (BBR_PROBE_RTT_DUR / TIMER_TICK_MS);
        } else if (now >= bbr->probe_rtt_done_stamp) {
            bbr->min_rtt_stamp = now;
            if (bbr->filled_pipe) {
                bbr_enter_probe_bw(bbr);
            } else {
                bbr_enter_startup(bbr);
            }
        }
    }
}

static uint64_t bbr_bdp(bbr_state_t* bbr, uint64_t bw)
{
    uint64_t rtt = bbr->min_rtt_us ? bbr->min_rtt_us : 2000;
    return (bw * rtt) / 1000000ULL;
}

static uint64_t bbr_quantization_budget(bbr_state_t* bbr, uint64_t cwnd)
{
    if (cwnd < TCP_MSS * 4) cwnd = TCP_MSS * 4;
    return cwnd;
}

static uint64_t bbr_inflight(bbr_state_t* bbr, uint32_t gain)
{
    uint64_t bdp = bbr_bdp(bbr, bbr->max_bw);
    return bbr_quantization_budget(bbr, (bdp * gain) / BBR_UNIT);
}

static void bbr_update_target_cwnd(bbr_state_t* bbr)
{
    uint64_t cwnd = 0;
    if (bbr->mode == 0) {
        cwnd = bbr_inflight(bbr, BBR_HIGH_GAIN);
    } else if (bbr->mode == 1) {
        cwnd = bbr_inflight(bbr, BBR_DRAIN_GAIN);
    } else if (bbr->mode == 2) {
        uint32_t gain = BBR_UNIT;
        if (bbr->cycle_idx == 0) gain = BBR_HIGH_GAIN;
        else if (bbr->cycle_idx == 1) gain = BBR_UNIT * 3 / 4;
        cwnd = bbr_inflight(bbr, gain);
    } else if (bbr->mode == 3) {
        cwnd = TCP_MSS * 4;
    }
    bbr->target_cwnd = cwnd;
}

static uint32_t bbr_cycle_gain(bbr_state_t* bbr)
{
    static const uint32_t gain_cycle[BBR_CYCLE_LEN] = {
        BBR_UNIT * 5 / 4, BBR_UNIT, BBR_UNIT, BBR_UNIT,
        BBR_UNIT, BBR_UNIT, BBR_UNIT * 3 / 4, BBR_UNIT
    };
    if (bbr->mode == 0) return BBR_HIGH_GAIN;
    if (bbr->mode == 1) return BBR_DRAIN_GAIN;
    if (bbr->mode == 3) return BBR_PROBE_RTT_GAIN;
    return gain_cycle[bbr->cycle_idx % BBR_CYCLE_LEN];
}

static void bbr_handle_ack(bbr_state_t* bbr, uint64_t acked_bytes,
                           uint64_t rtt_sample_us, uint64_t interval_us)
{
    bbr_update_bw(bbr, acked_bytes, interval_us);
    if (rtt_sample_us) {
        bbr_update_min_rtt(bbr, rtt_sample_us);
    }
    bbr->round_start = 1;
    bbr_check_full_bw_reached(bbr);
    uint64_t now = timer_get_jiffies();
    bbr_update_cycle_phase(bbr, now);
    bbr_update_mode(bbr, 0);
    uint32_t gain = bbr_cycle_gain(bbr);
    bbr_set_pacing_rate(bbr, gain);
    bbr_update_target_cwnd(bbr);
}

static bbr_state_t* bbr_states[TCP_MAX_SOCKETS];
static spinlock_t bbr_lock = SPINLOCK_INIT;

void tcp_bbr_init_all(void)
{
    spin_init(&bbr_lock);
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        bbr_states[i] = NULL;
    }
}

void tcp_bbr_attach(tcp_socket_t* sock)
{
    if (!sock) return;
    int idx = -1;
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!bbr_states[i]) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;
    bbr_state_t* bbr = (bbr_state_t*)kzalloc(sizeof(bbr_state_t));
    if (!bbr) return;
    bbr->min_rtt_us = 0;
    bbr->min_rtt_stamp = 0;
    bbr->max_bw = 0;
    bbr->pacing_rate = 0;
    bbr->target_cwnd = TCP_MSS * 4;
    bbr->snd_nxt = sock->snd_nxt;
    bbr->app_limited = 0;
    bbr->last_sent_mstamp = 0;
    bbr->sock = sock;
    bbr_enter_startup(bbr);
    spin_lock(&bbr_lock);
    bbr_states[idx] = bbr;
    spin_unlock(&bbr_lock);
}

void tcp_bbr_detach(tcp_socket_t* sock)
{
    if (!sock) return;
    spin_lock(&bbr_lock);
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (bbr_states[i] && bbr_states[i]->sock == sock) {
            kfree(bbr_states[i]);
            bbr_states[i] = NULL;
            break;
        }
    }
    spin_unlock(&bbr_lock);
}

void tcp_bbr_on_ack(tcp_socket_t* sock, uint64_t acked_bytes, uint64_t rtt_us)
{
    if (!sock) return;
    bbr_state_t* bbr = NULL;
    spin_lock(&bbr_lock);
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (bbr_states[i] && bbr_states[i]->sock == sock) {
            bbr = bbr_states[i];
            break;
        }
    }
    spin_unlock(&bbr_lock);
    if (!bbr) return;
    bbr->delivered += acked_bytes;
    uint64_t now = timer_get_jiffies();
    uint64_t interval = (now - bbr->delivered_mstamp) * TIMER_TICK_MS;
    if (interval == 0) interval = 1;
    bbr_handle_ack(bbr, acked_bytes, rtt_us, interval * 1000);
    bbr->delivered_mstamp = now;
}

uint64_t tcp_bbr_pacing_delay_us(tcp_socket_t* sock, uint32_t segment_size)
{
    if (!sock) return 0;
    bbr_state_t* bbr = NULL;
    spin_lock(&bbr_lock);
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (bbr_states[i] && bbr_states[i]->sock == sock) {
            bbr = bbr_states[i];
            break;
        }
    }
    spin_unlock(&bbr_lock);
    if (!bbr || !bbr->pacing_rate) return 0;
    return (segment_size * 1000000ULL) / bbr->pacing_rate;
}

uint64_t tcp_bbr_get_cwnd(tcp_socket_t* sock)
{
    if (!sock) return TCP_MSS * 4;
    bbr_state_t* bbr = NULL;
    spin_lock(&bbr_lock);
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (bbr_states[i] && bbr_states[i]->sock == sock) {
            bbr = bbr_states[i];
            break;
        }
    }
    spin_unlock(&bbr_lock);
    if (!bbr) return TCP_MSS * 4;
    return bbr->target_cwnd;
}

uint64_t tcp_bbr_get_pacing_rate(tcp_socket_t* sock)
{
    if (!sock) return 0;
    bbr_state_t* bbr = NULL;
    spin_lock(&bbr_lock);
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (bbr_states[i] && bbr_states[i]->sock == sock) {
            bbr = bbr_states[i];
            break;
        }
    }
    spin_unlock(&bbr_lock);
    if (!bbr) return 0;
    return bbr->pacing_rate;
}
