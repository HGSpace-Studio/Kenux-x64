#ifndef KAPI_SECURITY_H
#define KAPI_SECURITY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_SEC_MODULE_NAME_MAX  32
#define KAPI_SEC_MAX_MODULES      8
#define KAPI_SEC_CONTEXT_LEN      256
#define KAPI_SEC_SID_MAX          4096

#define KAPI_SEC_MAY_EXEC    1
#define KAPI_SEC_MAY_WRITE   2
#define KAPI_SEC_MAY_READ    4
#define KAPI_SEC_MAY_APPEND  8
#define KAPI_SEC_MAY_ACCESS  16

#define KAPI_SECCLASS_FILE           1
#define KAPI_SECCLASS_DIR            2
#define KAPI_SECCLASS_FIFO           3
#define KAPI_SECCLASS_CHR            4
#define KAPI_SECCLASS_BLK            5
#define KAPI_SECCLASS_TCP_SOCKET     6
#define KAPI_SECCLASS_UDP_SOCKET     7
#define KAPI_SECCLASS_RAW_SOCKET     8
#define KAPI_SECCLASS_SOCKET         9
#define KAPI_SECCLASS_PROCESS        10
#define KAPI_SECCLASS_SYSTEM         11
#define KAPI_SECCLASS_KEY            12
#define KAPI_SECCLASS_NETIF          13
#define KAPI_SECCLASS_PORT           14
#define KAPI_SECCLASS_NODE           15
#define KAPI_SECCLASS_NETLINK_SOCKET 16
#define KAPI_SECCLASS_PACKET_SOCKET  17
#define KAPI_SECCLASS_UNIX_STREAM_SOCKET 18
#define KAPI_SECCLASS_UNIX_DGRAM_SOCKET  19
#define KAPI_SECCLASS_IPC            20
#define KAPI_SECCLASS_MSG            21
#define KAPI_SECCLASS_MSGQ           22
#define KAPI_SECCLASS_SHM            23
#define KAPI_SECCLASS_SEM            24

#define KAPI_SEC_FILE__READ          0x0001
#define KAPI_SEC_FILE__WRITE         0x0002
#define KAPI_SEC_FILE__CREATE        0x0004
#define KAPI_SEC_FILE__GETATTR       0x0008
#define KAPI_SEC_FILE__UNLINK        0x0010
#define KAPI_SEC_FILE__APPEND        0x0020
#define KAPI_SEC_FILE__RENAME        0x0040
#define KAPI_SEC_FILE__LOCK          0x0080
#define KAPI_SEC_FILE__EXECUTE       0x0100

#define KAPI_SEC_PROCESS__FORK       0x0001
#define KAPI_SEC_PROCESS__TRANSITION 0x0002
#define KAPI_SEC_PROCESS__SIGCHLD    0x0004
#define KAPI_SEC_PROCESS__SIGKILL    0x0008
#define KAPI_SEC_PROCESS__SIGNAL     0x0010
#define KAPI_SEC_PROCESS__GETATTR    0x0020
#define KAPI_SEC_PROCESS__SETSID     0x0040
#define KAPI_SEC_PROCESS__EXECMEM    0x0080
#define KAPI_SEC_PROCESS__EXECSTACK  0x0100
#define KAPI_SEC_PROCESS__DYNTRANSITION 0x0200
#define KAPI_SEC_PROCESS__SETCURRENT 0x0400

typedef struct {
    char user[64];
    char role[64];
    char type[64];
    char level[64];
    uint32_t sid;
} kapi_sec_context_t;

typedef struct {
    uint32_t allowed;
    uint32_t auditallow;
    uint32_t auditdeny;
    uint32_t seqno;
    uint8_t  decided;
    uint8_t  auditallow_decided;
    uint8_t  auditdeny_decided;
} kapi_sec_av_decision_t;

typedef struct {
    int (*inode_permission)(void* inode, int mask);
    int (*file_permission)(void* file, int mask);
    int (*task_create)(void* task);
    void (*task_free)(void* task);
    int (*socket_bind)(void* sock, void* addr, int addr_len);
    int (*socket_connect)(void* sock, void* addr, int addr_len);
    int (*socket_accept)(void* sock, void* newsock);
    int (*socket_sendmsg)(void* sock, void* msg, int size);
    int (*socket_recvmsg)(void* sock, void* msg, int size, int flags);
    int (*inode_setxattr)(void* dentry, const char* name, const void* value, int64_t size, int flags);
    int (*inode_getxattr)(void* dentry, const char* name);
    int (*inode_removexattr)(void* dentry, const char* name);
    int (*capability)(void* task, int cap, int audit);
    int (*mmap_file)(void* file, unsigned long reqprot, unsigned long prot, unsigned long flags);
    int (*inode_create)(void* dir, void* dentry, int mode);
    int (*inode_link)(void* old_dentry, void* dir, void* new_dentry);
    int (*inode_unlink)(void* dir, void* dentry);
    int (*inode_symlink)(void* dir, void* dentry, const char* name);
    int (*inode_mkdir)(void* dir, void* dentry, int mode);
    int (*inode_rmdir)(void* dir, void* dentry);
    int (*inode_rename)(void* old_dir, void* old_dentry, void* new_dir, void* new_dentry);
    int (*inode_setattr)(void* dentry, int iattr);
    int (*inode_getattr)(void* path);
    int (*file_open)(void* file);
    int (*file_receive)(void* file);
    int (*task_setuid)(void* new_creds, void* old_creds);
    int (*task_setgid)(void* new_creds, void* old_creds);
    int (*task_kill)(void* task, int sig);
    int (*task_setnice)(void* task, int nice);
    int (*task_setscheduler)(void* task);
    int (*ipc_permission)(void* ipc_perm, int flag);
    int (*socket_listen)(void* sock, int backlog);
    int (*socket_shutdown)(void* sock, int how);
    int (*socket_getsockname)(void* sock);
    int (*socket_getpeername)(void* sock);
    void* reserved[16];
} kapi_sec_ops_t;

int kapi_sec_register_module(const char* name, kapi_sec_ops_t* ops);
int kapi_sec_unregister_module(const char* name);
int kapi_sec_module_active(const char* name);

int kapi_sec_inode_permission(void* inode, int mask);
int kapi_sec_file_permission(void* file, int mask);
int kapi_sec_task_create(void* task);
void kapi_sec_task_free(void* task);
int kapi_sec_socket_bind(void* sock, void* addr, int addr_len);
int kapi_sec_socket_connect(void* sock, void* addr, int addr_len);
int kapi_sec_socket_accept(void* sock, void* newsock);
int kapi_sec_socket_sendmsg(void* sock, void* msg, int size);
int kapi_sec_socket_recvmsg(void* sock, void* msg, int size, int flags);
int kapi_sec_capability(void* task, int cap, int audit);
int kapi_sec_inode_create(void* dir, void* dentry, int mode);
int kapi_sec_inode_unlink(void* dir, void* dentry);
int kapi_sec_inode_mkdir(void* dir, void* dentry, int mode);
int kapi_sec_inode_rmdir(void* dir, void* dentry);
int kapi_sec_inode_rename(void* old_dir, void* old_dentry, void* new_dir, void* new_dentry);
int kapi_sec_file_open(void* file);
int kapi_sec_task_setuid(void* new_creds, void* old_creds);
int kapi_sec_task_setgid(void* new_creds, void* old_creds);
int kapi_sec_task_kill(void* task, int sig);
int kapi_sec_task_setnice(void* task, int nice);
int kapi_sec_task_setscheduler(void* task);
int kapi_sec_socket_listen(void* sock, int backlog);
int kapi_sec_socket_shutdown(void* sock, int how);

int kapi_sec_selinux_enabled(void);
int kapi_sec_selinux_enforcing(void);
void kapi_sec_selinux_set_enforcing(int val);

int kapi_sec_selinux_context_to_sid(const kapi_sec_context_t* ctx, uint32_t* sid);
int kapi_sec_selinux_sid_to_context(uint32_t sid, kapi_sec_context_t* ctx);
int kapi_sec_selinux_parse_context(const char* str, kapi_sec_context_t* ctx);
int kapi_sec_selinux_context_to_string(const kapi_sec_context_t* ctx, char* buf, size_t size);

int kapi_sec_selinux_compute_av(uint32_t ssid, uint32_t tsid, uint16_t tclass, kapi_sec_av_decision_t* avd);
int kapi_sec_selinux_avc_lookup(uint32_t ssid, uint32_t tsid, uint16_t tclass, kapi_sec_av_decision_t* avd);
void kapi_sec_selinux_avc_flush(void);

int kapi_sec_init(void);

/* ===== Fused LeonOS auth.h (user/login/elevate) ===== */
#define KAPI_AUTH_MAX_USERS    32U
#define KAPI_AUTH_USERNAME_LEN 32U
#define KAPI_AUTH_PASSWORD_LEN 64U
#define KAPI_AUTH_HOME_LEN     96U

#define KAPI_AUTH_ROLE_NONE  0U
#define KAPI_AUTH_ROLE_USER  1U
#define KAPI_AUTH_ROLE_ADMIN 2U

#define KAPI_AUTH_USER_DISABLED 0x00000001U

#define KAPI_AUTH_UPDATE_ROLE  0x00000001U
#define KAPI_AUTH_UPDATE_FLAGS 0x00000002U

typedef struct {
    uint32_t uid;
    uint32_t role;
    uint32_t flags;
    uint32_t reserved;
    char     username[KAPI_AUTH_USERNAME_LEN];
    char     home[KAPI_AUTH_HOME_LEN];
} kapi_user_info_t;

typedef struct {
    uint32_t user_count;
    uint32_t has_admin;
    uint32_t reserved0;
    uint32_t reserved1;
} kapi_auth_status_t;

int kapi_auth_status(kapi_auth_status_t* status);
int kapi_auth_current(kapi_user_info_t* user);
int kapi_auth_list_users(kapi_user_info_t* users, uint32_t capacity,
                         uint32_t include_disabled, uint32_t* out_count);
int kapi_auth_login(const char* username, const char* password,
                    kapi_user_info_t* user);
int kapi_auth_elevate_admin(const char* username, const char* password,
                            kapi_user_info_t* user);
int kapi_auth_delegate_elevation(uint32_t child_pid);
int kapi_auth_logout(void);
int kapi_auth_create_user(const char* username, const char* password,
                          uint32_t role, kapi_user_info_t* user);
int kapi_auth_update_user(uint32_t uid, uint32_t mask, uint32_t role,
                          uint32_t flags);
int kapi_auth_change_password(uint32_t uid, const char* old_password,
                              const char* new_password);
int kapi_admin_elevate(void);

/* LeonOS compat aliases */
#define KAPI_Auth_Status(p)           kapi_auth_status((p))
#define KAPI_Auth_Current(p)          kapi_auth_current((p))
#define KAPI_Auth_ListUsers(u,c,i,o)  kapi_auth_list_users((u),(c),(i),(o))
#define KAPI_Auth_Login(u,p,r)        kapi_auth_login((u),(p),(r))
#define KAPI_Auth_ElevateAdmin(u,p,r) kapi_auth_elevate_admin((u),(p),(r))
#define KAPI_Auth_DelegateElevation(p) kapi_auth_delegate_elevation((p))
#define KAPI_Auth_Logout()            kapi_auth_logout()
#define KAPI_Auth_CreateUser(u,p,r,o) kapi_auth_create_user((u),(p),(r),(o))
#define KAPI_Auth_UpdateUser(u,m,r,f) kapi_auth_update_user((u),(m),(r),(f))
#define KAPI_Auth_ChangePassword(u,o,n) kapi_auth_change_password((u),(o),(n))
#define KAPI_Admin_Elevate()          kapi_admin_elevate()
typedef kapi_user_info_t    KAPI_USER_INFO;
typedef kapi_auth_status_t  KAPI_AUTH_STATUS;

#ifdef __cplusplus
}
#endif

#endif