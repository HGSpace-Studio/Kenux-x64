#ifndef SELINUX_H
#define SELINUX_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define SELINUX_CONTEXT_LEN     256
#define SELINUX_USER_MAX        64
#define SELINUX_ROLE_MAX        64
#define SELINUX_TYPE_MAX        64
#define SELINUX_LEVEL_MAX       64
#define SELINUX_SID_MAX         4096
#define SELINUX_AVC_SLOTS       512
#define SELINUX_TE_RULES_MAX    1024
#define SELINUX_CLASS_MAX       64
#define SELINUX_PERM_MAX        256
#define SELINUX_HOOK_MAX        64

typedef uint32_t sid_t;

struct selinux_context {
    char user[SELINUX_USER_MAX];
    char role[SELINUX_ROLE_MAX];
    char type[SELINUX_TYPE_MAX];
    char level[SELINUX_LEVEL_MAX];
    sid_t sid;
};

struct avc_entry {
    sid_t ssid;
    sid_t tsid;
    uint16_t tclass;
    uint32_t allowed;
    uint32_t audited;
    uint32_t seqno;
    uint8_t  valid;
    struct avc_entry* next;
};

struct avc_node {
    struct avc_entry entries[4];
    struct avc_node* next;
};

struct avtab_key {
    sid_t source_type;
    sid_t target_type;
    uint16_t target_class;
};

struct avtab_datum {
    uint32_t allowed;
    uint32_t auditallow;
    uint32_t auditdeny;
};

struct avtab_node {
    struct avtab_key key;
    struct avtab_datum datum;
    struct avtab_node* next;
};

struct class_perm {
    uint16_t class_id;
    uint32_t perm_map;
};

struct te_rule {
    sid_t source_sid;
    sid_t target_sid;
    uint16_t tclass;
    uint32_t perms;
    int rule_type;
};

struct selinux_ss {
    struct selinux_context contexts[SELINUX_SID_MAX];
    uint32_t next_sid;
    spinlock_t sid_lock;
};

struct lsm_hook {
    const char* name;
    int (*hook)(void);
    void* priv;
};

struct selinux_state {
    int enforcing;
    int enabled;
    uint32_t policy_version;
    uint32_t avc_seqno;
    spinlock_t avc_lock;
    spinlock_t te_lock;
    struct avc_node* avc_cache[SELINUX_AVC_SLOTS];
    struct avtab_node* avtab[SELINUX_AVC_SLOTS];
    struct te_rule te_rules[SELINUX_TE_RULES_MAX];
    uint32_t te_rule_count;
    struct selinux_ss ss;
    struct lsm_hook hooks[SELINUX_HOOK_MAX];
    uint32_t hook_count;
};

#define AVC_CALLBACK_GRANT      1
#define AVC_CALLBACK_TRY_REVOKE 2
#define AVC_CALLBACK_REVOKE     4
#define AVC_CALLBACK_RESET      8
#define AVC_CALLBACK_AUDITALLOW_ENABLE  16
#define AVC_CALLBACK_AUDITALLOW_DISABLE 32
#define AVC_CALLBACK_AUDITDENY_ENABLE   64
#define AVC_CALLBACK_AUDITDENY_DISABLE  128

void selinux_init(void);
int selinux_enabled(void);
int selinux_enforcing(void);
void selinux_set_enforcing(int val);

sid_t selinux_context_to_sid(const struct selinux_context* ctx);
int selinux_sid_to_context(sid_t sid, struct selinux_context* ctx);
int selinux_parse_context(const char* str, struct selinux_context* ctx);
int selinux_context_to_string(const struct selinux_context* ctx, char* buf, uint64_t size);

uint32_t selinux_avc_lookup(sid_t ssid, sid_t tsid, uint16_t tclass);
uint32_t selinux_avc_compute(sid_t ssid, sid_t tsid, uint16_t tclass);
void selinux_avc_insert(sid_t ssid, sid_t tsid, uint16_t tclass, uint32_t perms);
void selinux_avc_flush(void);

int selinux_te_allow(sid_t ssid, sid_t tsid, uint16_t tclass, uint32_t perms);
int selinux_te_deny(sid_t ssid, sid_t tsid, uint16_t tclass, uint32_t perms);
int selinux_te_auditallow(sid_t ssid, sid_t tsid, uint16_t tclass, uint32_t perms);
int selinux_te_auditdeny(sid_t ssid, sid_t tsid, uint16_t tclass, uint32_t perms);

int selinux_access_allowed(sid_t ssid, sid_t tsid, uint16_t tclass, uint32_t requested);
int selinux_task_create(void* task);
int selinux_task_exec(void* task, const char* filename);
int selinux_file_permission(void* file, int mask);
int selinux_inode_permission(void* inode, int mask);
int selinux_socket_create(int family, int type, int protocol);

int selinux_lsm_register_hook(const char* name, int (*hook)(void));
void selinux_lsm_call_hooks(void);

extern struct selinux_state selinux_state;

#endif
