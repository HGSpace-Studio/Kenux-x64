#include <arch/selinux.h>
#include <arch/lsm.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>

#define SELINUX_DBG(c) do { __asm__ volatile ("outb %0, %1" : : "a"((char)(c)), "d"((unsigned short)0x3F8)); } while (0)

selinux_state_t selinux_state;

static void selinux_sidtab_init(void)
{
    SELINUX_DBG('1');
    memset(&selinux_state, 0, sizeof(selinux_state));
    SELINUX_DBG('2');
    selinux_state.next_sid = SECINITSID_MAX;
    selinux_state.policy_version = 1;
    selinux_state.policy_seqno = 0;
    selinux_state.enforcing = 1;
    selinux_state.enabled = 1;
    selinux_state.avc_seqno = 0;
    spin_init(&selinux_state.sid_lock);
    spin_init(&selinux_state.avc_lock);

    SELINUX_DBG('3');
    memset(selinux_state.avc_cache, 0, sizeof(selinux_state.avc_cache));
    memset(selinux_state.avtab.entries, 0, sizeof(selinux_state.avtab.entries));
    selinux_state.avtab.nentries = 0;
    spin_init(&selinux_state.avtab.lock);

    SELINUX_DBG('4');

    strncpy(selinux_state.contexts[SECINITSID_KERNEL].user, "system_u", SELINUX_USER_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_KERNEL].role, "system_r", SELINUX_ROLE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_KERNEL].type, "kernel_t", SELINUX_TYPE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_KERNEL].level, "s0", SELINUX_LEVEL_MAX - 1);
    selinux_state.contexts[SECINITSID_KERNEL].sid = SECINITSID_KERNEL;

    strncpy(selinux_state.contexts[SECINITSID_UNLABELED].user, "system_u", SELINUX_USER_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_UNLABELED].role, "object_r", SELINUX_ROLE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_UNLABELED].type, "unlabeled_t", SELINUX_TYPE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_UNLABELED].level, "s0", SELINUX_LEVEL_MAX - 1);
    selinux_state.contexts[SECINITSID_UNLABELED].sid = SECINITSID_UNLABELED;

    strncpy(selinux_state.contexts[SECINITSID_FILE].user, "system_u", SELINUX_USER_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_FILE].role, "object_r", SELINUX_ROLE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_FILE].type, "file_t", SELINUX_TYPE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_FILE].level, "s0", SELINUX_LEVEL_MAX - 1);
    selinux_state.contexts[SECINITSID_FILE].sid = SECINITSID_FILE;

    strncpy(selinux_state.contexts[SECINITSID_DIR].user, "system_u", SELINUX_USER_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_DIR].role, "object_r", SELINUX_ROLE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_DIR].type, "dir_t", SELINUX_TYPE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_DIR].level, "s0", SELINUX_LEVEL_MAX - 1);
    selinux_state.contexts[SECINITSID_DIR].sid = SECINITSID_DIR;

    strncpy(selinux_state.contexts[SECINITSID_PROCESS].user, "system_u", SELINUX_USER_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_PROCESS].role, "system_r", SELINUX_ROLE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_PROCESS].type, "kernel_t", SELINUX_TYPE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_PROCESS].level, "s0", SELINUX_LEVEL_MAX - 1);
    selinux_state.contexts[SECINITSID_PROCESS].sid = SECINITSID_PROCESS;

    strncpy(selinux_state.contexts[SECINITSID_TCP_SOCKET].user, "system_u", SELINUX_USER_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_TCP_SOCKET].role, "object_r", SELINUX_ROLE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_TCP_SOCKET].type, "tcp_socket_t", SELINUX_TYPE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_TCP_SOCKET].level, "s0", SELINUX_LEVEL_MAX - 1);
    selinux_state.contexts[SECINITSID_TCP_SOCKET].sid = SECINITSID_TCP_SOCKET;

    strncpy(selinux_state.contexts[SECINITSID_SOCKET].user, "system_u", SELINUX_USER_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_SOCKET].role, "object_r", SELINUX_ROLE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_SOCKET].type, "socket_t", SELINUX_TYPE_MAX - 1);
    strncpy(selinux_state.contexts[SECINITSID_SOCKET].level, "s0", SELINUX_LEVEL_MAX - 1);
    selinux_state.contexts[SECINITSID_SOCKET].sid = SECINITSID_SOCKET;
}

static void selinux_avtab_insert(u16 source, u16 target, u16 tclass, u32 allowed, u32 auditallow, u32 auditdeny)
{
    spin_lock(&selinux_state.avtab.lock);
    if (selinux_state.avtab.nentries >= SELINUX_TE_RULES_MAX) {
        spin_unlock(&selinux_state.avtab.lock);
        return;
    }
    avtab_node_t *node = &selinux_state.avtab.entries[selinux_state.avtab.nentries++];
    node->key.source_type = source;
    node->key.target_type = target;
    node->key.target_class = tclass;
    node->datum.allowed = allowed;
    node->datum.auditallow = auditallow;
    node->datum.auditdeny = auditdeny;
    node->used = 1;
    spin_unlock(&selinux_state.avtab.lock);
}

static void selinux_load_default_policy(void)
{
    SELINUX_DBG('a');
    selinux_avtab_insert(SECINITSID_KERNEL, SECINITSID_FILE, SECCLASS_FILE,
        FILE__READ | FILE__WRITE | FILE__GETATTR | FILE__CREATE |
        FILE__UNLINK | FILE__APPEND | FILE__RENAME | FILE__LOCK |
        FILE__EXECUTE | FILE__MOUNTON, 0, ~(u32)0);

    SELINUX_DBG('b');

    selinux_avtab_insert(SECINITSID_KERNEL, SECINITSID_DIR, SECCLASS_DIR,
        DIR__READ | DIR__WRITE | DIR__CREATE | DIR__GETATTR |
        DIR__REMOVE_NAME | DIR__ADD_NAME | DIR__SEARCH |
        DIR__LIST | DIR__RMDIR, 0, ~(u32)0);

    selinux_avtab_insert(SECINITSID_PROCESS, SECINITSID_PROCESS, SECCLASS_PROCESS,
        PROCESS__FORK | PROCESS__TRANSITION | PROCESS__SIGCHLD |
        PROCESS__SIGKILL | PROCESS__SIGNAL | PROCESS__GETATTR |
        PROCESS__SETSID, 0, ~(u32)0);

    selinux_avtab_insert(SECINITSID_KERNEL, SECINITSID_PROCESS, SECCLASS_PROCESS,
        PROCESS__FORK | PROCESS__TRANSITION | PROCESS__SIGCHLD |
        PROCESS__SIGKILL | PROCESS__SIGNAL | PROCESS__GETATTR |
        PROCESS__SETSID | PROCESS__EXECMEM | PROCESS__DYNTRANSITION, 0, ~(u32)0);

    selinux_avtab_insert(SECINITSID_KERNEL, SECINITSID_TCP_SOCKET, SECCLASS_TCP_SOCKET,
        TCP_SOCKET__BIND | TCP_SOCKET__CONNECT | TCP_SOCKET__LISTEN |
        TCP_SOCKET__ACCEPT | TCP_SOCKET__SEND | TCP_SOCKET__RECV |
        TCP_SOCKET__READ | TCP_SOCKET__WRITE | TCP_SOCKET__SHUTDOWN |
        TCP_SOCKET__NODE_BIND, 0, ~(u32)0);

    selinux_avtab_insert(SECINITSID_KERNEL, SECINITSID_SOCKET, SECCLASS_SOCKET,
        TCP_SOCKET__BIND | TCP_SOCKET__CONNECT | TCP_SOCKET__LISTEN |
        TCP_SOCKET__ACCEPT | TCP_SOCKET__SEND | TCP_SOCKET__RECV |
        TCP_SOCKET__READ | TCP_SOCKET__WRITE | TCP_SOCKET__SHUTDOWN, 0, ~(u32)0);

    selinux_avtab_insert(SECINITSID_KERNEL, SECINITSID_KERNEL, SECCLASS_SYSTEM,
        SYSTEM__IPC_INFO | SYSTEM__SYSLOG | SYSTEM__MODIFY, 0, ~(u32)0);
}

void selinux_init(void)
{
    SELINUX_DBG('X');
    selinux_sidtab_init();
    SELINUX_DBG('Y');
    selinux_load_default_policy();
    SELINUX_DBG('Z');
}

int selinux_enabled(void)
{
    return selinux_state.enabled;
}

int selinux_enforcing(void)
{
    return selinux_state.enforcing;
}

void selinux_set_enforcing(int val)
{
    selinux_state.enforcing = val ? 1 : 0;
}

int selinux_context_to_sid(const selinux_context_t *ctx, selinux_sid_t *sid)
{
    if (!ctx || !sid) return -1;
    spin_lock(&selinux_state.sid_lock);
    for (u32 i = 1; i < selinux_state.next_sid && i < SELINUX_SID_MAX; i++) {
        selinux_context_t *c = &selinux_state.contexts[i];
        if (strncmp(c->user, ctx->user, SELINUX_USER_MAX) == 0 &&
            strncmp(c->role, ctx->role, SELINUX_ROLE_MAX) == 0 &&
            strncmp(c->type, ctx->type, SELINUX_TYPE_MAX) == 0 &&
            strncmp(c->level, ctx->level, SELINUX_LEVEL_MAX) == 0) {
            *sid = i;
            spin_unlock(&selinux_state.sid_lock);
            return 0;
        }
    }
    if (selinux_state.next_sid >= SELINUX_SID_MAX) {
        spin_unlock(&selinux_state.sid_lock);
        return -1;
    }
    u32 new_sid = selinux_state.next_sid++;
    if (new_sid < SELINUX_SID_MAX) {
        memcpy(&selinux_state.contexts[new_sid], ctx, sizeof(selinux_context_t));
        selinux_state.contexts[new_sid].sid = new_sid;
        *sid = new_sid;
    } else {
        spin_unlock(&selinux_state.sid_lock);
        return -1;
    }
    spin_unlock(&selinux_state.sid_lock);
    return 0;
}

int selinux_sid_to_context(selinux_sid_t sid, selinux_context_t *ctx)
{
    if (!ctx) return -1;
    if (sid == 0 || sid >= selinux_state.next_sid || sid >= SELINUX_SID_MAX) return -1;
    spin_lock(&selinux_state.sid_lock);
    memcpy(ctx, &selinux_state.contexts[sid], sizeof(selinux_context_t));
    spin_unlock(&selinux_state.sid_lock);
    return 0;
}

int selinux_parse_context(const char *str, selinux_context_t *ctx)
{
    if (!str || !ctx) return -1;
    memset(ctx, 0, sizeof(selinux_context_t));

    const char *p = str;
    const char *user_start = p;
    while (*p && *p != ':') p++;
    u64 ulen = p - user_start;
    if (ulen >= SELINUX_USER_MAX) return -1;
    memcpy(ctx->user, user_start, ulen);
    ctx->user[ulen] = '\0';

    if (*p == ':') p++;
    const char *role_start = p;
    while (*p && *p != ':') p++;
    u64 rlen = p - role_start;
    if (rlen >= SELINUX_ROLE_MAX) return -1;
    memcpy(ctx->role, role_start, rlen);
    ctx->role[rlen] = '\0';

    if (*p == ':') p++;
    const char *type_start = p;
    while (*p && *p != ':') p++;
    u64 tlen = p - type_start;
    if (tlen >= SELINUX_TYPE_MAX) return -1;
    memcpy(ctx->type, type_start, tlen);
    ctx->type[tlen] = '\0';

    if (*p == ':') p++;
    const char *level_start = p;
    while (*p) p++;
    u64 llen = p - level_start;
    if (llen >= SELINUX_LEVEL_MAX) llen = SELINUX_LEVEL_MAX - 1;
    memcpy(ctx->level, level_start, llen);
    ctx->level[llen] = '\0';

    return 0;
}

int selinux_context_to_string(const selinux_context_t *ctx, char *buf, u64 size)
{
    if (!ctx || !buf || size == 0) return -1;
    s64 remaining = (s64)size - 1;
    u64 pos = 0;
    u64 len;

    len = strlen(ctx->user);
    if (pos + len + 3 > (u64)size) return -1;
    memcpy(buf + pos, ctx->user, len); pos += len;
    buf[pos++] = ':';

    len = strlen(ctx->role);
    if (pos + len + 2 > (u64)size) return -1;
    memcpy(buf + pos, ctx->role, len); pos += len;
    buf[pos++] = ':';

    len = strlen(ctx->type);
    if (pos + len + 2 > (u64)size) return -1;
    memcpy(buf + pos, ctx->type, len); pos += len;
    buf[pos++] = ':';

    len = strlen(ctx->level);
    if (pos + len + 1 > (u64)size) { buf[pos] = '\0'; return (int)pos; }
    memcpy(buf + pos, ctx->level, len); pos += len;
    buf[pos] = '\0';
    return (int)pos;
}

static avtab_node_t *avtab_search(selinux_sid_t ssid, selinux_sid_t tsid, u16 tclass)
{
    for (u32 i = 0; i < selinux_state.avtab.nentries; i++) {
        avtab_node_t *node = &selinux_state.avtab.entries[i];
        if (!node->used) continue;
        if (node->key.source_type == ssid &&
            node->key.target_type == tsid &&
            node->key.target_class == tclass) {
            return node;
        }
    }
    return NULL;
}

int selinux_compute_av(selinux_sid_t ssid, selinux_sid_t tsid, u16 tclass, selinux_av_decision_t *avd)
{
    if (!avd) return -1;
    memset(avd, 0, sizeof(selinux_av_decision_t));

    if (ssid < SECINITSID_KERNEL || ssid >= SELINUX_SID_MAX ||
        tsid < SECINITSID_KERNEL || tsid >= SELINUX_SID_MAX) {
        avd->allowed = 0;
        avd->decided = 1;
        return 0;
    }

    spin_lock(&selinux_state.avtab.lock);
    avtab_node_t *node = avtab_search(ssid, tsid, tclass);
    if (node) {
        avd->allowed = node->datum.allowed;
        avd->auditallow = node->datum.auditallow;
        avd->auditdeny = node->datum.auditdeny;
        avd->seqno = selinux_state.policy_seqno;
        avd->decided = 1;
        avd->auditallow_decided = 1;
        avd->auditdeny_decided = 1;
    } else {
        avd->allowed = 0;
        avd->decided = 0;
    }
    spin_unlock(&selinux_state.avtab.lock);
    return 0;
}

int selinux_avc_lookup(selinux_sid_t ssid, selinux_sid_t tsid, u16 tclass, selinux_av_decision_t *avd)
{
    if (!avd) return -1;

    spin_lock(&selinux_state.avc_lock);
    for (int i = 0; i < SELINUX_AVC_SLOTS; i++) {
        avc_cache_slot_t *slot = &selinux_state.avc_cache[i];
        if (!slot->used) continue;
        for (int j = 0; j < SELINUX_AVD_MAX; j++) {
            if (slot->entries[j].valid &&
                slot->entries[j].ssid == ssid &&
                slot->entries[j].tsid == tsid &&
                slot->entries[j].tclass == tclass) {
                avd->allowed = slot->entries[j].allowed;
                avd->auditallow = slot->entries[j].auditallow;
                avd->auditdeny = slot->entries[j].auditdeny;
                avd->seqno = slot->entries[j].seqno;
                avd->decided = 1;
                spin_unlock(&selinux_state.avc_lock);
                return 0;
            }
        }
    }
    spin_unlock(&selinux_state.avc_lock);

    return -1;
}

void selinux_avc_insert(selinux_sid_t ssid, selinux_sid_t tsid, u16 tclass, selinux_av_decision_t *avd)
{
    spin_lock(&selinux_state.avc_lock);
    for (int i = 0; i < SELINUX_AVC_SLOTS; i++) {
        avc_cache_slot_t *slot = &selinux_state.avc_cache[i];
        if (!slot->used) {
            memset(slot->entries, 0, sizeof(slot->entries));
            slot->entries[0].ssid = ssid;
            slot->entries[0].tsid = tsid;
            slot->entries[0].tclass = tclass;
            slot->entries[0].allowed = avd->allowed;
            slot->entries[0].auditallow = avd->auditallow;
            slot->entries[0].auditdeny = avd->auditdeny;
            slot->entries[0].seqno = avd->seqno;
            slot->entries[0].valid = 1;
            slot->used = 1;
            spin_unlock(&selinux_state.avc_lock);
            return;
        }
    }
    spin_unlock(&selinux_state.avc_lock);
}

void selinux_avc_flush(void)
{
    spin_lock(&selinux_state.avc_lock);
    memset(selinux_state.avc_cache, 0, sizeof(selinux_state.avc_cache));
    spin_unlock(&selinux_state.avc_lock);
}

static int selinux_avc_has_perm(selinux_sid_t ssid, selinux_sid_t tsid, u16 tclass, u32 requested)
{
    selinux_av_decision_t avd;
    int rc;

    rc = selinux_avc_lookup(ssid, tsid, tclass, &avd);
    if (rc == 0) {
        goto check;
    }

    rc = selinux_compute_av(ssid, tsid, tclass, &avd);
    if (rc == 0) {
        selinux_avc_insert(ssid, tsid, tclass, &avd);
    }

check:
    if ((avd.allowed & requested) == requested) {
        return 0;
    }

    if (!selinux_state.enforcing) {
        return 0;
    }

    return -1;
}

static int selinux_inode_perm(selinux_sid_t ssid, selinux_sid_t tsid, u16 tclass, int mask)
{
    u32 perms = 0;
    if (tclass == SECCLASS_FILE) {
        if (mask & 1) perms |= FILE__READ;
        if (mask & 2) perms |= FILE__WRITE;
        if (mask & 4) perms |= FILE__EXECUTE;
        perms |= FILE__GETATTR;
    } else if (tclass == SECCLASS_DIR) {
        if (mask & 1) perms |= DIR__READ;
        if (mask & 2) perms |= DIR__WRITE;
        perms |= DIR__SEARCH | DIR__GETATTR;
    } else {
        perms = (u32)mask;
    }
    return selinux_avc_has_perm(ssid, tsid, tclass, perms);
}

int selinux_inode_permission(void *inode, int mask)
{
    (void)inode;
    return selinux_inode_perm(SECINITSID_KERNEL, SECINITSID_FILE, SECCLASS_FILE, mask);
}

int selinux_socket_bind(void *sock, void *addr, int addr_len)
{
    (void)sock;
    (void)addr;
    (void)addr_len;
    return selinux_avc_has_perm(SECINITSID_KERNEL, SECINITSID_SOCKET, SECCLASS_SOCKET,
        TCP_SOCKET__BIND | TCP_SOCKET__NODE_BIND);
}

int selinux_socket_connect(void *sock, void *addr, int addr_len)
{
    (void)sock;
    (void)addr;
    (void)addr_len;
    return selinux_avc_has_perm(SECINITSID_KERNEL, SECINITSID_TCP_SOCKET, SECCLASS_TCP_SOCKET,
        TCP_SOCKET__CONNECT);
}

int selinux_socket_sendmsg(void *sock, void *msg, int size)
{
    (void)sock;
    (void)msg;
    (void)size;
    return selinux_avc_has_perm(SECINITSID_KERNEL, SECINITSID_TCP_SOCKET, SECCLASS_TCP_SOCKET,
        TCP_SOCKET__SEND);
}

int selinux_socket_recvmsg(void *sock, void *msg, int size, int flags)
{
    (void)sock;
    (void)msg;
    (void)size;
    (void)flags;
    return selinux_avc_has_perm(SECINITSID_KERNEL, SECINITSID_TCP_SOCKET, SECCLASS_TCP_SOCKET,
        TCP_SOCKET__RECV);
}

int selinux_task_create(void *task)
{
    (void)task;
    return selinux_avc_has_perm(SECINITSID_PROCESS, SECINITSID_PROCESS, SECCLASS_PROCESS,
        PROCESS__FORK);
}

int selinux_task_setuid(void *new_creds, void *old_creds)
{
    (void)new_creds;
    (void)old_creds;
    return selinux_avc_has_perm(SECINITSID_KERNEL, SECINITSID_PROCESS, SECCLASS_PROCESS,
        PROCESS__DYNTRANSITION);
}

int selinux_task_setgid(void *new_creds, void *old_creds)
{
    (void)new_creds;
    (void)old_creds;
    return selinux_avc_has_perm(SECINITSID_KERNEL, SECINITSID_PROCESS, SECCLASS_PROCESS,
        PROCESS__DYNTRANSITION);
}

int selinux_task_kill(void *task, int sig)
{
    (void)task;
    (void)sig;
    return selinux_avc_has_perm(SECINITSID_KERNEL, SECINITSID_PROCESS, SECCLASS_PROCESS,
        PROCESS__SIGKILL | PROCESS__SIGNAL);
}

int selinux_node_perm(void *addr, int family, u32 type)
{
    (void)addr;
    (void)family;
    (void)type;
    return selinux_avc_has_perm(SECINITSID_KERNEL, SECINITSID_NODE, SECCLASS_NODE, 0x01);
}
