#include "kapi.h"
#include <string.h>

#define KAPI_SEC_CONTEXT_TABLE_MAX 128
#define KAPI_SEC_AVC_MAX 128

typedef struct {
    int used;
    char name[KAPI_SEC_MODULE_NAME_MAX];
    kapi_sec_ops_t ops;
} kapi_sec_module_entry_t;

typedef struct {
    int used;
    uint32_t sid;
    kapi_sec_context_t ctx;
} kapi_sec_context_entry_t;

typedef struct {
    int used;
    uint32_t ssid;
    uint32_t tsid;
    uint16_t tclass;
    kapi_sec_av_decision_t avd;
} kapi_sec_avc_entry_t;

static kapi_sec_module_entry_t sec_modules[KAPI_SEC_MAX_MODULES];
static kapi_sec_context_entry_t sec_contexts[KAPI_SEC_CONTEXT_TABLE_MAX];
static kapi_sec_avc_entry_t sec_avc[KAPI_SEC_AVC_MAX];
static uint32_t sec_next_sid = 1;
static uint32_t sec_seqno = 1;
static int sec_selinux_enabled = 1;
static int sec_selinux_enforcing = 1;

static int sec_name_equal(const char* a, const char* b)
{
    if (!a || !b) {
        return 0;
    }
    return strncmp(a, b, KAPI_SEC_MODULE_NAME_MAX) == 0;
}

static int sec_context_equal(const kapi_sec_context_t* a, const kapi_sec_context_t* b)
{
    return strncmp(a->user, b->user, sizeof(a->user)) == 0 &&
           strncmp(a->role, b->role, sizeof(a->role)) == 0 &&
           strncmp(a->type, b->type, sizeof(a->type)) == 0 &&
           strncmp(a->level, b->level, sizeof(a->level)) == 0;
}

static void sec_copy_token(char* dst, size_t dst_size, const char* src, size_t len)
{
    if (dst_size == 0) {
        return;
    }
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static uint32_t sec_class_full_mask(uint16_t tclass)
{
    switch (tclass) {
        case KAPI_SECCLASS_FILE:
        case KAPI_SECCLASS_DIR:
        case KAPI_SECCLASS_FIFO:
        case KAPI_SECCLASS_CHR:
        case KAPI_SECCLASS_BLK:
            return 0x01FF;
        case KAPI_SECCLASS_PROCESS:
            return 0x07FF;
        default:
            return 0xFFFFFFFFU;
    }
}

static int sec_dispatch_int(int (*selector)(kapi_sec_ops_t*, void**, int*), void** args, int* extra)
{
    for (int i = 0; i < KAPI_SEC_MAX_MODULES; i++) {
        if (!sec_modules[i].used) {
            continue;
        }
        int ret = selector(&sec_modules[i].ops, args, extra);
        if (ret != KAPI_OK) {
            return ret;
        }
    }
    return KAPI_OK;
}

static int sel_inode_permission(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->inode_permission ? ops->inode_permission(args[0], extra[0]) : KAPI_OK;
}

static int sel_file_permission(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->file_permission ? ops->file_permission(args[0], extra[0]) : KAPI_OK;
}

static int sel_task_create(kapi_sec_ops_t* ops, void** args, int* extra)
{
    (void)extra;
    return ops->task_create ? ops->task_create(args[0]) : KAPI_OK;
}

static int sel_socket_bind(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->socket_bind ? ops->socket_bind(args[0], args[1], extra[0]) : KAPI_OK;
}

static int sel_socket_connect(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->socket_connect ? ops->socket_connect(args[0], args[1], extra[0]) : KAPI_OK;
}

static int sel_socket_accept(kapi_sec_ops_t* ops, void** args, int* extra)
{
    (void)extra;
    return ops->socket_accept ? ops->socket_accept(args[0], args[1]) : KAPI_OK;
}

static int sel_socket_sendmsg(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->socket_sendmsg ? ops->socket_sendmsg(args[0], args[1], extra[0]) : KAPI_OK;
}

static int sel_socket_recvmsg(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->socket_recvmsg ? ops->socket_recvmsg(args[0], args[1], extra[0], extra[1]) : KAPI_OK;
}

static int sel_capability(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->capability ? ops->capability(args[0], extra[0], extra[1]) : KAPI_OK;
}

static int sel_inode_create(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->inode_create ? ops->inode_create(args[0], args[1], extra[0]) : KAPI_OK;
}

static int sel_inode_unlink(kapi_sec_ops_t* ops, void** args, int* extra)
{
    (void)extra;
    return ops->inode_unlink ? ops->inode_unlink(args[0], args[1]) : KAPI_OK;
}

static int sel_inode_mkdir(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->inode_mkdir ? ops->inode_mkdir(args[0], args[1], extra[0]) : KAPI_OK;
}

static int sel_inode_rmdir(kapi_sec_ops_t* ops, void** args, int* extra)
{
    (void)extra;
    return ops->inode_rmdir ? ops->inode_rmdir(args[0], args[1]) : KAPI_OK;
}

static int sel_inode_rename(kapi_sec_ops_t* ops, void** args, int* extra)
{
    (void)extra;
    return ops->inode_rename ? ops->inode_rename(args[0], args[1], args[2], args[3]) : KAPI_OK;
}

static int sel_file_open(kapi_sec_ops_t* ops, void** args, int* extra)
{
    (void)extra;
    return ops->file_open ? ops->file_open(args[0]) : KAPI_OK;
}

static int sel_task_setuid(kapi_sec_ops_t* ops, void** args, int* extra)
{
    (void)extra;
    return ops->task_setuid ? ops->task_setuid(args[0], args[1]) : KAPI_OK;
}

static int sel_task_setgid(kapi_sec_ops_t* ops, void** args, int* extra)
{
    (void)extra;
    return ops->task_setgid ? ops->task_setgid(args[0], args[1]) : KAPI_OK;
}

static int sel_task_kill(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->task_kill ? ops->task_kill(args[0], extra[0]) : KAPI_OK;
}

static int sel_task_setnice(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->task_setnice ? ops->task_setnice(args[0], extra[0]) : KAPI_OK;
}

static int sel_task_setscheduler(kapi_sec_ops_t* ops, void** args, int* extra)
{
    (void)extra;
    return ops->task_setscheduler ? ops->task_setscheduler(args[0]) : KAPI_OK;
}

static int sel_socket_listen(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->socket_listen ? ops->socket_listen(args[0], extra[0]) : KAPI_OK;
}

static int sel_socket_shutdown(kapi_sec_ops_t* ops, void** args, int* extra)
{
    return ops->socket_shutdown ? ops->socket_shutdown(args[0], extra[0]) : KAPI_OK;
}

int kapi_security_init(void)
{
    memset(sec_modules, 0, sizeof(sec_modules));
    memset(sec_contexts, 0, sizeof(sec_contexts));
    memset(sec_avc, 0, sizeof(sec_avc));
    sec_next_sid = 1;
    sec_seqno = 1;
    sec_selinux_enabled = 1;
    sec_selinux_enforcing = 1;
    return KAPI_OK;
}

int kapi_sec_init(void)
{
    return kapi_security_init();
}

int kapi_sec_register_module(const char* name, kapi_sec_ops_t* ops)
{
    if (!name || !ops || name[0] == '\0') {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_SEC_MAX_MODULES; i++) {
        if (sec_modules[i].used && sec_name_equal(sec_modules[i].name, name)) {
            return KAPI_EBUSY;
        }
    }

    for (int i = 0; i < KAPI_SEC_MAX_MODULES; i++) {
        if (!sec_modules[i].used) {
            memset(&sec_modules[i], 0, sizeof(sec_modules[i]));
            sec_modules[i].used = 1;
            strncpy(sec_modules[i].name, name, sizeof(sec_modules[i].name) - 1);
            memcpy(&sec_modules[i].ops, ops, sizeof(kapi_sec_ops_t));
            return KAPI_OK;
        }
    }

    return KAPI_ENOMEM;
}

int kapi_sec_unregister_module(const char* name)
{
    if (!name) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_SEC_MAX_MODULES; i++) {
        if (sec_modules[i].used && sec_name_equal(sec_modules[i].name, name)) {
            memset(&sec_modules[i], 0, sizeof(sec_modules[i]));
            return KAPI_OK;
        }
    }

    return KAPI_ENOENT;
}

int kapi_sec_module_active(const char* name)
{
    if (!name) {
        return 0;
    }

    for (int i = 0; i < KAPI_SEC_MAX_MODULES; i++) {
        if (sec_modules[i].used && sec_name_equal(sec_modules[i].name, name)) {
            return 1;
        }
    }

    return 0;
}

int kapi_sec_inode_permission(void* inode, int mask)
{
    void* args[1] = { inode };
    int extra[1] = { mask };
    return sec_dispatch_int(sel_inode_permission, args, extra);
}

int kapi_sec_file_permission(void* file, int mask)
{
    void* args[1] = { file };
    int extra[1] = { mask };
    return sec_dispatch_int(sel_file_permission, args, extra);
}

int kapi_sec_task_create(void* task)
{
    void* args[1] = { task };
    return sec_dispatch_int(sel_task_create, args, NULL);
}

void kapi_sec_task_free(void* task)
{
    for (int i = 0; i < KAPI_SEC_MAX_MODULES; i++) {
        if (sec_modules[i].used && sec_modules[i].ops.task_free) {
            sec_modules[i].ops.task_free(task);
        }
    }
}

int kapi_sec_socket_bind(void* sock, void* addr, int addr_len)
{
    void* args[2] = { sock, addr };
    int extra[1] = { addr_len };
    return sec_dispatch_int(sel_socket_bind, args, extra);
}

int kapi_sec_socket_connect(void* sock, void* addr, int addr_len)
{
    void* args[2] = { sock, addr };
    int extra[1] = { addr_len };
    return sec_dispatch_int(sel_socket_connect, args, extra);
}

int kapi_sec_socket_accept(void* sock, void* newsock)
{
    void* args[2] = { sock, newsock };
    return sec_dispatch_int(sel_socket_accept, args, NULL);
}

int kapi_sec_socket_sendmsg(void* sock, void* msg, int size)
{
    void* args[2] = { sock, msg };
    int extra[1] = { size };
    return sec_dispatch_int(sel_socket_sendmsg, args, extra);
}

int kapi_sec_socket_recvmsg(void* sock, void* msg, int size, int flags)
{
    void* args[2] = { sock, msg };
    int extra[2] = { size, flags };
    return sec_dispatch_int(sel_socket_recvmsg, args, extra);
}

int kapi_sec_capability(void* task, int cap, int audit)
{
    void* args[1] = { task };
    int extra[2] = { cap, audit };
    return sec_dispatch_int(sel_capability, args, extra);
}

int kapi_sec_inode_create(void* dir, void* dentry, int mode)
{
    void* args[2] = { dir, dentry };
    int extra[1] = { mode };
    return sec_dispatch_int(sel_inode_create, args, extra);
}

int kapi_sec_inode_unlink(void* dir, void* dentry)
{
    void* args[2] = { dir, dentry };
    return sec_dispatch_int(sel_inode_unlink, args, NULL);
}

int kapi_sec_inode_mkdir(void* dir, void* dentry, int mode)
{
    void* args[2] = { dir, dentry };
    int extra[1] = { mode };
    return sec_dispatch_int(sel_inode_mkdir, args, extra);
}

int kapi_sec_inode_rmdir(void* dir, void* dentry)
{
    void* args[2] = { dir, dentry };
    return sec_dispatch_int(sel_inode_rmdir, args, NULL);
}

int kapi_sec_inode_rename(void* old_dir, void* old_dentry, void* new_dir, void* new_dentry)
{
    void* args[4] = { old_dir, old_dentry, new_dir, new_dentry };
    return sec_dispatch_int(sel_inode_rename, args, NULL);
}

int kapi_sec_file_open(void* file)
{
    void* args[1] = { file };
    return sec_dispatch_int(sel_file_open, args, NULL);
}

int kapi_sec_task_setuid(void* new_creds, void* old_creds)
{
    void* args[2] = { new_creds, old_creds };
    return sec_dispatch_int(sel_task_setuid, args, NULL);
}

int kapi_sec_task_setgid(void* new_creds, void* old_creds)
{
    void* args[2] = { new_creds, old_creds };
    return sec_dispatch_int(sel_task_setgid, args, NULL);
}

int kapi_sec_task_kill(void* task, int sig)
{
    void* args[1] = { task };
    int extra[1] = { sig };
    return sec_dispatch_int(sel_task_kill, args, extra);
}

int kapi_sec_task_setnice(void* task, int nice)
{
    void* args[1] = { task };
    int extra[1] = { nice };
    return sec_dispatch_int(sel_task_setnice, args, extra);
}

int kapi_sec_task_setscheduler(void* task)
{
    void* args[1] = { task };
    return sec_dispatch_int(sel_task_setscheduler, args, NULL);
}

int kapi_sec_socket_listen(void* sock, int backlog)
{
    void* args[1] = { sock };
    int extra[1] = { backlog };
    return sec_dispatch_int(sel_socket_listen, args, extra);
}

int kapi_sec_socket_shutdown(void* sock, int how)
{
    void* args[1] = { sock };
    int extra[1] = { how };
    return sec_dispatch_int(sel_socket_shutdown, args, extra);
}

int kapi_sec_selinux_enabled(void)
{
    return sec_selinux_enabled;
}

int kapi_sec_selinux_enforcing(void)
{
    return sec_selinux_enforcing;
}

void kapi_sec_selinux_set_enforcing(int val)
{
    sec_selinux_enforcing = val ? 1 : 0;
}

int kapi_sec_selinux_context_to_sid(const kapi_sec_context_t* ctx, uint32_t* sid)
{
    if (!ctx || !sid) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_SEC_CONTEXT_TABLE_MAX; i++) {
        if (sec_contexts[i].used && sec_context_equal(&sec_contexts[i].ctx, ctx)) {
            *sid = sec_contexts[i].sid;
            return KAPI_OK;
        }
    }

    for (int i = 0; i < KAPI_SEC_CONTEXT_TABLE_MAX; i++) {
        if (!sec_contexts[i].used) {
            sec_contexts[i].used = 1;
            sec_contexts[i].sid = sec_next_sid++;
            if (sec_next_sid == 0 || sec_next_sid > KAPI_SEC_SID_MAX) {
                sec_next_sid = 1;
            }
            memcpy(&sec_contexts[i].ctx, ctx, sizeof(*ctx));
            sec_contexts[i].ctx.sid = sec_contexts[i].sid;
            *sid = sec_contexts[i].sid;
            return KAPI_OK;
        }
    }

    return KAPI_ENOMEM;
}

int kapi_sec_selinux_sid_to_context(uint32_t sid, kapi_sec_context_t* ctx)
{
    if (!ctx || sid == 0) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_SEC_CONTEXT_TABLE_MAX; i++) {
        if (sec_contexts[i].used && sec_contexts[i].sid == sid) {
            memcpy(ctx, &sec_contexts[i].ctx, sizeof(*ctx));
            ctx->sid = sid;
            return KAPI_OK;
        }
    }

    return KAPI_ENOENT;
}

int kapi_sec_selinux_parse_context(const char* str, kapi_sec_context_t* ctx)
{
    if (!str || !ctx) {
        return KAPI_EINVAL;
    }

    memset(ctx, 0, sizeof(*ctx));
    const char* p = str;
    const char* c1 = strchr(p, ':');
    if (!c1) {
        return KAPI_EINVAL;
    }
    const char* c2 = strchr(c1 + 1, ':');
    if (!c2) {
        return KAPI_EINVAL;
    }
    const char* c3 = strchr(c2 + 1, ':');
    if (!c3) {
        return KAPI_EINVAL;
    }

    sec_copy_token(ctx->user, sizeof(ctx->user), p, (size_t)(c1 - p));
    sec_copy_token(ctx->role, sizeof(ctx->role), c1 + 1, (size_t)(c2 - c1 - 1));
    sec_copy_token(ctx->type, sizeof(ctx->type), c2 + 1, (size_t)(c3 - c2 - 1));
    strncpy(ctx->level, c3 + 1, sizeof(ctx->level) - 1);
    return kapi_sec_selinux_context_to_sid(ctx, &ctx->sid);
}

int kapi_sec_selinux_context_to_string(const kapi_sec_context_t* ctx, char* buf, size_t size)
{
    if (!ctx || !buf || size == 0) {
        return KAPI_EINVAL;
    }

    size_t pos = 0;
    const char* parts[7] = { ctx->user, ":", ctx->role, ":", ctx->type, ":", ctx->level };
    buf[0] = '\0';

    for (int i = 0; i < 7; i++) {
        size_t len = strlen(parts[i]);
        if (pos + len + 1 > size) {
            return KAPI_EINVAL;
        }
        memcpy(buf + pos, parts[i], len);
        pos += len;
        buf[pos] = '\0';
    }

    return KAPI_OK;
}

int kapi_sec_selinux_compute_av(uint32_t ssid, uint32_t tsid, uint16_t tclass, kapi_sec_av_decision_t* avd)
{
    if (!avd || ssid == 0 || tsid == 0) {
        return KAPI_EINVAL;
    }

    memset(avd, 0, sizeof(*avd));
    avd->seqno = sec_seqno;
    avd->decided = 1;
    avd->auditallow_decided = 1;
    avd->auditdeny_decided = 1;

    uint32_t full = sec_class_full_mask(tclass);
    if (!sec_selinux_enabled || !sec_selinux_enforcing || ssid == tsid) {
        avd->allowed = full;
    } else {
        switch (tclass) {
            case KAPI_SECCLASS_FILE:
            case KAPI_SECCLASS_DIR:
                avd->allowed = KAPI_SEC_FILE__READ | KAPI_SEC_FILE__GETATTR;
                break;
            case KAPI_SECCLASS_PROCESS:
                avd->allowed = KAPI_SEC_PROCESS__GETATTR | KAPI_SEC_PROCESS__SIGNAL;
                break;
            case KAPI_SECCLASS_TCP_SOCKET:
            case KAPI_SECCLASS_UDP_SOCKET:
            case KAPI_SECCLASS_SOCKET:
            case KAPI_SECCLASS_NETIF:
            case KAPI_SECCLASS_NETLINK_SOCKET:
                avd->allowed = full;
                break;
            default:
                avd->allowed = 0;
                break;
        }
    }

    avd->auditdeny = full & ~avd->allowed;
    return KAPI_OK;
}

int kapi_sec_selinux_avc_lookup(uint32_t ssid, uint32_t tsid, uint16_t tclass, kapi_sec_av_decision_t* avd)
{
    if (!avd) {
        return KAPI_EINVAL;
    }

    for (int i = 0; i < KAPI_SEC_AVC_MAX; i++) {
        if (sec_avc[i].used && sec_avc[i].ssid == ssid && sec_avc[i].tsid == tsid && sec_avc[i].tclass == tclass) {
            memcpy(avd, &sec_avc[i].avd, sizeof(*avd));
            return KAPI_OK;
        }
    }

    kapi_sec_av_decision_t computed;
    int ret = kapi_sec_selinux_compute_av(ssid, tsid, tclass, &computed);
    if (ret != KAPI_OK) {
        return ret;
    }

    uint32_t slot = (ssid ^ (tsid << 5) ^ tclass) % KAPI_SEC_AVC_MAX;
    sec_avc[slot].used = 1;
    sec_avc[slot].ssid = ssid;
    sec_avc[slot].tsid = tsid;
    sec_avc[slot].tclass = tclass;
    memcpy(&sec_avc[slot].avd, &computed, sizeof(computed));
    memcpy(avd, &computed, sizeof(*avd));
    return KAPI_OK;
}

void kapi_sec_selinux_avc_flush(void)
{
    memset(sec_avc, 0, sizeof(sec_avc));
    sec_seqno++;
    if (sec_seqno == 0) {
        sec_seqno = 1;
    }
}

static void kapi_auth_copy_string(char* dst, size_t cap, const char* src)
{
    size_t i = 0;
    if (!dst || cap == 0) return;
    if (!src) src = "";
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int kapi_auth_name_equal(const char* a, const char* b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int kapi_auth_status(kapi_auth_status_t* status)
{
    if (!status) return KAPI_EINVAL;
    memset(status, 0, sizeof(*status));
    status->user_count = 1;
    status->has_admin = 1;
    return KAPI_OK;
}

int kapi_auth_current(kapi_user_info_t* user)
{
    if (!user) return KAPI_EINVAL;
    memset(user, 0, sizeof(*user));
    user->uid = 0;
    user->role = KAPI_AUTH_ROLE_ADMIN;
    kapi_auth_copy_string(user->username, sizeof(user->username), "root");
    kapi_auth_copy_string(user->home, sizeof(user->home), "/");
    return KAPI_OK;
}

int kapi_auth_list_users(kapi_user_info_t* users, uint32_t capacity,
                         uint32_t include_disabled, uint32_t* out_count)
{
    (void)include_disabled;
    if (!users || capacity == 0) return KAPI_EINVAL;
    int rc = kapi_auth_current(&users[0]);
    if (out_count) *out_count = rc == KAPI_OK ? 1U : 0U;
    return rc;
}

int kapi_auth_login(const char* username, const char* password,
                    kapi_user_info_t* user)
{
    (void)password;
    if (!username || !user) return KAPI_EINVAL;
    if (!kapi_auth_name_equal(username, "root")) return KAPI_EACCES;
    return kapi_auth_current(user);
}

int kapi_auth_elevate_admin(const char* username, const char* password,
                            kapi_user_info_t* user)
{
    return kapi_auth_login(username ? username : "root", password, user);
}

int kapi_auth_delegate_elevation(uint32_t child_pid)
{
    (void)child_pid;
    return KAPI_ENOSYS;
}

int kapi_auth_logout(void)
{
    return KAPI_OK;
}

int kapi_auth_create_user(const char* username, const char* password,
                          uint32_t role, kapi_user_info_t* user)
{
    (void)username;
    (void)password;
    (void)role;
    (void)user;
    return KAPI_ENOSYS;
}

int kapi_auth_update_user(uint32_t uid, uint32_t mask, uint32_t role,
                          uint32_t flags)
{
    (void)uid;
    (void)mask;
    (void)role;
    (void)flags;
    return KAPI_ENOSYS;
}

int kapi_auth_change_password(uint32_t uid, const char* old_password,
                              const char* new_password)
{
    (void)uid;
    (void)old_password;
    (void)new_password;
    return KAPI_ENOSYS;
}

int kapi_admin_elevate(void)
{
    return KAPI_ENOSYS;
}
