/*
 * Kenux Advanced OS Skeleton - Network Statistics implementation
 *
 * Skeleton: per-interface counters + flow tracking + event ring.
 * Sampling depends on timer hooks and netdev stats being wired up.
 */

#include "kapi_netstat.h"
#include "kapi.h"

#include <string.h>

static kapi_netstat_iface_t  kapi_netstat_ifaces[KAPI_NETSTAT_NAME_MAX];
static kapi_netstat_flow_t   kapi_netstat_flows[KAPI_NETSTAT_MAX_FLOWS];
static kapi_netstat_log_entry_t kapi_netstat_log[KAPI_NETSTAT_LOG_ENTRIES];
static uint32_t kapi_netstat_log_head;
static uint32_t kapi_netstat_log_count;
static int kapi_netstat_initialized = 0;

static kapi_netstat_iface_t *netstat_find_iface(const char *name)
{
    /* TODO: index by interface name once netdev stats are aggregated */
    (void)name;
    return NULL;
}

int kapi_netstat_init(void)
{
    if (kapi_netstat_initialized) {
        return KAPI_NETSTAT_OK;
    }
    memset(kapi_netstat_ifaces, 0, sizeof(kapi_netstat_ifaces));
    memset(kapi_netstat_flows, 0, sizeof(kapi_netstat_flows));
    memset(kapi_netstat_log, 0, sizeof(kapi_netstat_log));
    kapi_netstat_log_head = 0;
    kapi_netstat_log_count = 0;
    kapi_netstat_initialized = 1;
    return KAPI_NETSTAT_OK;
}

void kapi_netstat_exit(void)
{
    memset(kapi_netstat_ifaces, 0, sizeof(kapi_netstat_ifaces));
    memset(kapi_netstat_flows, 0, sizeof(kapi_netstat_flows));
    memset(kapi_netstat_log, 0, sizeof(kapi_netstat_log));
    kapi_netstat_initialized = 0;
}

int kapi_netstat_get_iface(const char *name, kapi_netstat_iface_t *out)
{
    if (!name || !out) {
        return KAPI_NETSTAT_EINVAL;
    }
    /* TODO: aggregate counters from kapi_netdev stats */
    memset(out, 0, sizeof(*out));
    return KAPI_NETSTAT_OK;
}

int kapi_netstat_list_ifaces(kapi_netstat_iface_t *out, uint32_t max,
                             uint32_t *count)
{
    if (!out || !count) {
        return KAPI_NETSTAT_EINVAL;
    }
    *count = 0;
    /* TODO: enumerate registered netdevs */
    return KAPI_NETSTAT_OK;
}

int kapi_netstat_flow_enumerate(kapi_netstat_flow_t *out, uint32_t max,
                                uint32_t *count)
{
    if (!out || !count) {
        return KAPI_NETSTAT_EINVAL;
    }
    uint32_t n = 0;
    for (uint32_t i = 0; i < KAPI_NETSTAT_MAX_FLOWS && n < max; i++) {
        if (kapi_netstat_flows[i].proto != 0) {
            out[n++] = kapi_netstat_flows[i];
        }
    }
    *count = n;
    return KAPI_NETSTAT_OK;
}

int kapi_netstat_flow_clear(void)
{
    memset(kapi_netstat_flows, 0, sizeof(kapi_netstat_flows));
    return KAPI_NETSTAT_OK;
}

static void netstat_log_push(kapi_netstat_event_t type, const char *iface,
                             uint64_t value)
{
    kapi_netstat_log_entry_t *e =
        &kapi_netstat_log[kapi_netstat_log_head];
    e->type = type;
    if (iface) {
        strncpy(e->iface, iface, KAPI_NETSTAT_NAME_MAX - 1);
    }
    e->value = value;
    e->timestamp_ms = 0; /* TODO: timer_get_jiffies() */
    kapi_netstat_log_head = (kapi_netstat_log_head + 1)
        % KAPI_NETSTAT_LOG_ENTRIES;
    if (kapi_netstat_log_count < KAPI_NETSTAT_LOG_ENTRIES) {
        kapi_netstat_log_count++;
    }
}

int kapi_netstat_log_get(kapi_netstat_log_entry_t *out, uint32_t max,
                         uint32_t *count)
{
    if (!out || !count) {
        return KAPI_NETSTAT_EINVAL;
    }
    uint32_t n = kapi_netstat_log_count < max ? kapi_netstat_log_count : max;
    /* Read oldest-first from the ring */
    uint32_t start = (kapi_netstat_log_head + KAPI_NETSTAT_LOG_ENTRIES
                      - kapi_netstat_log_count) % KAPI_NETSTAT_LOG_ENTRIES;
    for (uint32_t i = 0; i < n; i++) {
        out[i] = kapi_netstat_log[(start + i) % KAPI_NETSTAT_LOG_ENTRIES];
    }
    *count = n;
    return KAPI_NETSTAT_OK;
}

int kapi_netstat_log_clear(void)
{
    kapi_netstat_log_head = 0;
    kapi_netstat_log_count = 0;
    memset(kapi_netstat_log, 0, sizeof(kapi_netstat_log));
    return KAPI_NETSTAT_OK;
}

int kapi_netstat_sample(void)
{
    /* TODO: pull netdev stats and update counters; detect drops */
    /* Placeholder: log a sample event */
    netstat_log_push(KAPI_NETSTAT_EVENT_CONGESTED, NULL, 0);
    return KAPI_NETSTAT_OK;
}
