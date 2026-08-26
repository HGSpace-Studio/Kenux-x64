#ifndef _ARCH_SELINUX_H
#define _ARCH_SELINUX_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define SELINUX_USER_MAX        32
#define SELINUX_ROLE_MAX        32
#define SELINUX_TYPE_MAX        32
#define SELINUX_LEVEL_MAX       32
#define SELINUX_SID_MAX         256
#define SELINUX_CONTEXT_LEN     128
#define SELINUX_AVC_SLOTS       64
#define SELINUX_TE_RULES_MAX    128
#define SELINUX_CLASS_MAX       32
#define SELINUX_PERM_MAX        64
#define SELINUX_AVD_MAX         8

#define SECINITSID_KERNEL       1
#define SECINITSID_UNLABELED    2
#define SECINITSID_FILE         3
#define SECINITSID_DIR          4
#define SECINITSID_CHR          5
#define SECINITSID_BLK          6
#define SECINITSID_SOCK         7
#define SECINITSID_FIFO         8
#define SECINITSID_SYMLINK      9
#define SECINITSID_PROCESS      10
#define SECINITSID_KERNEL_PROCESS 11
#define SECINITSID_NETIF        12
#define SECINITSID_SOCKET       13
#define SECINITSID_TCP_SOCKET   14
#define SECINITSID_UDP_SOCKET   15
#define SECINITSID_RAW_SOCKET   16
#define SECINITSID_PORT         17
#define SECINITSID_NODE         18
#define SECINITSID_KEY           19
#define SECINITSID_NULL         20
#define SECINITSID_MAX          21

#define SECCLASS_FILE           1
#define SECCLASS_DIR            2
#define SECCLASS_FIFO           3
#define SECCLASS_CHR            4
#define SECCLASS_BLK            5
#define SECCLASS_TCP_SOCKET     6
#define SECCLASS_UDP_SOCKET     7
#define SECCLASS_RAW_SOCKET     8
#define SECCLASS_SOCKET         9
#define SECCLASS_PROCESS        10
#define SECCLASS_SYSTEM         11
#define SECCLASS_KEY            12
#define SECCLASS_NETIF          13
#define SECCLASS_PORT           14
#define SECCLASS_NODE           15
#define SECCLASS_NETLINK_SOCKET 16
#define SECCLASS_PACKET_SOCKET  17
#define SECCLASS_UNIX_STREAM_SOCKET 18
#define SECCLASS_UNIX_DGRAM_SOCKET  19
#define SECCLASS_IPC            20
#define SECCLASS_MSG            21
#define SECCLASS_MSGQ           22
#define SECCLASS_SHM            23
#define SECCLASS_SEM            24
#define SECCLASS_MAX            25

#define FILE__READ              0x0001
#define FILE__WRITE             0x0002
#define FILE__CREATE            0x0004
#define FILE__GETATTR           0x0008
#define FILE__UNLINK            0x0010
#define FILE__APPEND            0x0020
#define FILE__RENAME            0x0040
#define FILE__LOCK              0x0080
#define FILE__EXECUTE           0x0100
#define FILE__SWAPON            0x0200
#define FILE__MOUNTON           0x0400
#define FILE__RELABELFROM       0x0800
#define FILE__RELABELTO         0x1000

#define DIR__READ               0x0001
#define DIR__WRITE              0x0002
#define DIR__CREATE             0x0004
#define DIR__GETATTR            0x0008
#define DIR__REMOVE_NAME        0x0010
#define DIR__ADD_NAME           0x0020
#define DIR__SEARCH             0x0040
#define DIR__LIST               0x0080
#define DIR__RMDIR              0x0100

#define TCP_SOCKET__BIND        0x0001
#define TCP_SOCKET__CONNECT     0x0002
#define TCP_SOCKET__LISTEN      0x0004
#define TCP_SOCKET__ACCEPT      0x0008
#define TCP_SOCKET__SEND        0x0010
#define TCP_SOCKET__RECV        0x0020
#define TCP_SOCKET__READ        0x0040
#define TCP_SOCKET__WRITE       0x0080
#define TCP_SOCKET__SHUTDOWN    0x0100
#define TCP_SOCKET__NODE_BIND   0x0200

#define PROCESS__FORK           0x0001
#define PROCESS__TRANSITION     0x0002
#define PROCESS__SIGCHLD        0x0004
#define PROCESS__SIGKILL        0x0008
#define PROCESS__SIGNAL         0x0010
#define PROCESS__GETATTR        0x0020
#define PROCESS__SETSID         0x0040
#define PROCESS__EXECMEM        0x0080
#define PROCESS__EXECSTACK      0x0100
#define PROCESS__DYNTRANSITION  0x0200
#define PROCESS__SETCURRENT     0x0400

#define SYSTEM__IPC_INFO        0x0001
#define SYSTEM__SYSLOG          0x0002
#define SYSTEM__MODIFY          0x0004

typedef u32 selinux_sid_t;

typedef struct selinux_context {
    char user[SELINUX_USER_MAX];
    char role[SELINUX_ROLE_MAX];
    char type[SELINUX_TYPE_MAX];
    char level[SELINUX_LEVEL_MAX];
    selinux_sid_t sid;
} selinux_context_t;

typedef struct selinux_av_decision {
    u32 allowed;
    u32 auditallow;
    u32 auditdeny;
    u32 seqno;
    u8 decided;
    u8 auditallow_decided;
    u8 auditdeny_decided;
} selinux_av_decision_t;

typedef struct {
    u16 source_type;
    u16 target_type;
    u16 target_class;
} avtab_key_t;

typedef struct {
    u32 allowed;
    u32 auditallow;
    u32 auditdeny;
} avtab_datum_t;

typedef struct avtab_node {
    avtab_key_t key;
    avtab_datum_t datum;
    u8 used;
} avtab_node_t;

typedef struct avtab {
    avtab_node_t entries[SELINUX_TE_RULES_MAX];
    u32 nentries;
    spinlock_t lock;
} avtab_t;

typedef struct {
    selinux_sid_t ssid;
    selinux_sid_t tsid;
    u16 tclass;
    u32 allowed;
    u32 auditallow;
    u32 auditdeny;
    u32 seqno;
    u8 valid;
} avc_entry_t;

typedef struct avc_cache_slot {
    avc_entry_t entries[SELINUX_AVD_MAX];
    u8 used;
} avc_cache_slot_t;

typedef struct {
    selinux_sid_t sid;
    selinux_context_t ctx;
} sid_context_map_t;

typedef struct {
    selinux_context_t contexts[SELINUX_SID_MAX];
    u32 next_sid;
    spinlock_t sid_lock;
    avtab_t avtab;
    u32 policy_version;
    u32 policy_seqno;
    int enforcing;
    int enabled;
    spinlock_t avc_lock;
    u32 avc_seqno;
    avc_cache_slot_t avc_cache[SELINUX_AVC_SLOTS];
} selinux_state_t;

void selinux_init(void);
int selinux_enabled(void);
int selinux_enforcing(void);
void selinux_set_enforcing(int val);

int selinux_context_to_sid(const selinux_context_t *ctx, selinux_sid_t *sid);
int selinux_sid_to_context(selinux_sid_t sid, selinux_context_t *ctx);
int selinux_parse_context(const char *str, selinux_context_t *ctx);
int selinux_context_to_string(const selinux_context_t *ctx, char *buf, u64 size);

int selinux_compute_av(selinux_sid_t ssid, selinux_sid_t tsid, u16 tclass, selinux_av_decision_t *avd);

int selinux_inode_permission(void *inode, int mask);
int selinux_socket_bind(void *sock, void *addr, int addr_len);
int selinux_socket_connect(void *sock, void *addr, int addr_len);
int selinux_socket_sendmsg(void *sock, void *msg, int size);
int selinux_socket_recvmsg(void *sock, void *msg, int size, int flags);
int selinux_task_create(void *task);
int selinux_task_setuid(void *new_creds, void *old_creds);
int selinux_task_setgid(void *new_creds, void *old_creds);
int selinux_task_kill(void *task, int sig);
int selinux_node_perm(void *addr, int family, u32 type);

int selinux_avc_lookup(selinux_sid_t ssid, selinux_sid_t tsid, u16 tclass, selinux_av_decision_t *avd);
void selinux_avc_insert(selinux_sid_t ssid, selinux_sid_t tsid, u16 tclass, selinux_av_decision_t *avd);
void selinux_avc_flush(void);

extern selinux_state_t selinux_state;

#endif
