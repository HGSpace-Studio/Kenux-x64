#ifndef KAPI_NETSTAT_H
#define KAPI_NETSTAT_H

/*
 * Kenux Advanced OS Skeleton - Network Statistics / Monitoring
 *
 * Real-time per-interface and per-flow traffic counters, bandwidth
 * estimation, and event log hooks for diagnostics. Skeleton: API only.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_NETSTAT_NAME_MAX     32
#define KAPI_NETSTAT_MAX_FLOWS    4096
#define KAPI_NETSTAT_LOG_ENTRIES  256

typedef enum {
    KAPI_NETSTAT_OK            = 0,
    KAPI_NETSTAT_EINVAL        = -1,
    KAPI_NETSTAT_ENOMEM        = -2,
    KAPI_NETSTAT_ENOENT       = -3
} kapi_netstat_err_t;

typedef struct {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_drops;
    uint64_t tx_drops;
    uint64_t bandwidth_rx_bps;
    uint64_t bandwidth_tx_bps;
    uint64_t timestamp_ms;
} kapi_netstat_iface_t;

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  proto;
    uint8_t  state;     /* simplified TCP state */
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t last_seen_ms;
} kapi_netstat_flow_t;

typedef enum {
    KAPI_NETSTAT_EVENT_LINK_UP   = 1,
    KAPI_NETSTAT_EVENT_LINK_DOWN = 2,
    KAPI_NETSTAT_EVENT_CONGESTED = 3,
    KAPI_NETSTAT_EVENT_DROPS     = 4
} kapi_netstat_event_t;

typedef struct {
    kapi_netstat_event_t type;
    char     iface[KAPI_NETSTAT_NAME_MAX];
    uint64_t value;
    uint64_t timestamp_ms;
} kapi_netstat_log_entry_t;

/* Subsystem lifecycle */
int kapi_netstat_init(void);
void kapi_netstat_exit(void);

/* Per-interface stats */
int kapi_netstat_get_iface(const char *name, kapi_netstat_iface_t *out);
int kapi_netstat_list_ifaces(kapi_netstat_iface_t *out, uint32_t max,
                             uint32_t *count);

/* Flow tracking */
int kapi_netstat_flow_enumerate(kapi_netstat_flow_t *out, uint32_t max,
                                uint32_t *count);
int kapi_netstat_flow_clear(void);

/* Event log */
int kapi_netstat_log_get(kapi_netstat_log_entry_t *out, uint32_t max,
                         uint32_t *count);
int kapi_netstat_log_clear(void);

/* Polling / sampling hook (called periodically from timer) */
int kapi_netstat_sample(void);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_NETSTAT_H */
