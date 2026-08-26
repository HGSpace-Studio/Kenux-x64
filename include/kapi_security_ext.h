#ifndef KAPI_SECURITY_EXT_H
#define KAPI_SECURITY_EXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_CAP_CHOWN            0
#define KAPI_CAP_DAC_OVERRIDE     1
#define KAPI_CAP_DAC_READ_SEARCH  2
#define KAPI_CAP_FOWNER           3
#define KAPI_CAP_FSETID           4
#define KAPI_CAP_KILL             5
#define KAPI_CAP_SETGID           6
#define KAPI_CAP_SETUID           7
#define KAPI_CAP_SETPCAP          8
#define KAPI_CAP_LINUX_IMMUTABLE  9
#define KAPI_CAP_NET_BIND_SERVICE 10
#define KAPI_CAP_NET_BROADCAST    11
#define KAPI_CAP_NET_ADMIN        12
#define KAPI_CAP_NET_RAW          13
#define KAPI_CAP_IPC_LOCK         14
#define KAPI_CAP_IPC_OWNER        15
#define KAPI_CAP_SYS_MODULE       16
#define KAPI_CAP_SYS_RAWIO        17
#define KAPI_CAP_SYS_CHROOT       18
#define KAPI_CAP_SYS_PTRACE       19
#define KAPI_CAP_SYS_PACCT        20
#define KAPI_CAP_SYS_ADMIN        21
#define KAPI_CAP_SYS_BOOT         22
#define KAPI_CAP_SYS_NICE         23
#define KAPI_CAP_SYS_RESOURCE     24
#define KAPI_CAP_SYS_TIME         25
#define KAPI_CAP_SYS_TTY_CONFIG   26
#define KAPI_CAP_MKNOD            27
#define KAPI_CAP_LEASE            28
#define KAPI_AUDIT_WRITE          29
#define KAPI_AUDIT_CONTROL        30
#define KAPI_CAP_SETFCAP          31
#define KAPI_MAC_OVERRIDE        32
#define KAPI_MAC_ADMIN            33
#define KAPI_SYSLOG               34
#define KAPI_WAKE_ALARM           35
#define KAPI_BLOCK_SUSPEND        36
#define KAPI_AUDIT_READ           37
#define KAPI_PERFMON              38
#define KAPI_BPF                  39
#define KAPI_CHECKPOINT_RESTORE   40

#define KAPI_CAP_LAST_CAP         KAPI_CHECKPOINT_RESTORE

#define KAPI_SECCLASS_NONE        0
#define KAPI_SECCLASS_FILE        1
#define KAPI_SECCLASS_DIR         2
#define KAPI_SECCLASS_CHR         3
#define KAPI_SECCLASS_BLK         4
#define KAPI_SECCLASS_FIFO        5
#define KAPI_SECCLASS_SOCK        6
#define KAPI_SECCLASS_IPC         7
#define KAPI_SECCLASS_USER        8
#define KAPI_SECCLASS_PROCESS     9
#define KAPI_SECCLASS_SYSTEM      10
#define KAPI_SECCLASS_SECURITY    11
#define KAPI_SECCLASS_KEY         12
#define KAPI_SECCLASS_NETIF       13
#define KAPI_SECCLASS_NETLINK     14
#define KAPI_SECCLASS_PACKET      15
#define KAPI_SECCLASS_SHM         16
#define KAPI_SEMS                17
#define KAPI_SECCLASS_MSGQ        18
#define KAPI_SECCLASS_MSG         19
#define KAPI_SECCLASS_SHMHPD      20
#define KAPI_SECCLASS_PORT        21
#define KAPI_SECCLASS_NETPORT     22
#define KAPI_SECCLASS_NODE        23
#define KAPI_SECCLASS_OPENSSL     24
#define KAPI_SECCLASS_SOCKREL     25
#define KAPI_SECCLASS_DCCP_SOCK   26
#define KAPI_SECCLASS_NETLINK_TCP 27
#define KAPI_SECCLASS_IBPKEY      28
#define KAPI_SECCLASS_KEY_PERM    29
#define KAPI_SECCLASS_BD          30
#define KAPI_SECCLASS_KERNEL_SERVICE 31
#define KAPI_SECCLASS_ALL         32

#define KAPI_PERM_READ            0x0001
#define KAPI_PERM_WRITE           0x0002
#define KAPI_PERM_CREATE          0x0004
#define KAPI_PERM_LINK            0x0008
#define KAPI_PERM_UNLINK          0x0010
#define KAPI_PERM_RENAME          0x0020
#define KAPI_PERM_EXECUTE         0x0040
#define KAPI_PERM_APPEND          0x0080
#define KAPI_PERM_GETATTR         0x0100
#define KAPI_PERM_SETATTR         0x0200
#define KAPI_PERM_LOCK            0x0400
#define KAPI_PERM_RELABELFROM     0x0800
#define KAPI_PERM_RELABELTO       0x1000
#define KAPI_PERM_TRANSITION      0x2000
#define KAPI_PERM_MEMBER          0x4000
#define KAPI_PERM_ALL             0xFFFF

typedef uint32_t kapi_security_id_t;
typedef uint32_t kapi_security_class_t;
typedef uint32_t kapi_access_vector_t;

typedef struct {
    const char* name;
    int value;
} kapi_cap_name_t;

typedef struct {
    bool effective[KAPI_CAP_LAST_CAP + 1];
    bool permitted[KAPI_CAP_LAST_CAP + 1];
    bool inheritable[KAPI_CAP_LAST_CAP + 1];
} kapi_cap_t;

typedef struct {
    uid_t uid;
    gid_t gid;
    uid_t euid;
    gid_t egid;
    uid_t suid;
    gid_t sgid;
    uid_t fsuid;
    gid_t fsgid;
    int ngroups;
    gid_t groups[NGROUPS_MAX];
} kapi_cred_t;

typedef struct {
    kapi_security_id_t sid;
    kapi_security_class_t sclass;
    kapi_access_vector_t av;
} kapi_avc_t;

typedef struct {
    kapi_security_id_t source_sid;
    kapi_security_id_t target_sid;
    kapi_security_class_t tclass;
    kapi_access_vector_t requested;
    kapi_access_vector_t decided;
    uint8_t auditdeny;
    uint8_t seqno;
    int result;
} kapi_av_decision_t;

typedef void (*kapi_audit_callback_t)(int type, const char* message, int result);

typedef int (*kapi_check_permission_fn)(kapi_security_id_t ssid,
                                        kapi_security_id_t tsid,
                                        kapi_security_class_t tclass,
                                        kapi_access_vector_t requested,
                                        kapi_av_decision_t* avd);

int kapi_cap_get(kapi_cap_t* caps);

int kapi_cap_set(const kapi_cap_t* caps);

bool kapi_cap_check(int cap);

int kapi_cap_raise(int cap);

int kapi_cap_lower(int cap);

int kapi_cap_set_ambient(int cap);

bool kapi_cap_has_ambient(int cap);

int kapi_cap_clear(void);

const char* kapi_cap_to_name(int cap);

int kapi_cap_from_name(const char* name);

kapi_security_id_t kapi_secctx_to_secid(const char* secctx, size_t seclen);

const char* kapi_secid_to_secctx(kapi_security_id_t secid, size_t* seclen);

int kapi_secid_to_ctx(kapi_security_id_t secid, char** ctx);

void kapi_release_secctx(char* secctx, size_t seclen);

int kapi_secmark_relabel_packet(const char* secctx);

const char* kapi_secmark_get_connlabel(uint32_t conn_label);

int kapi_secmark_set_connlabel(uint32_t conn_label, const char* label);

int kapi_security_compute_av(kapi_security_id_t ssid,
                             kapi_security_id_t tsid,
                             kapi_security_class_t tclass,
                             kapi_access_vector_t requested,
                             kapi_av_decision_t* avd);

int kapi_security_transition_sid(kapi_security_id_t ssid,
                                 kapi_security_id_t tsid,
                                 kapi_security_class_t tclass,
                                 kapi_security_id_t* out_sid);

int kapi_security_member_sid(kapi_security_id_t ssid,
                             kapi_security_id_t tsid,
                             kapi_security_id_t* out_sid);

int kapi_security_sid_to_context(kapi_security_id_t sid,
                                char** context);

int kapi_security_context_to_sid(const char* context,
                                kapi_security_id_t* out_sid);

int kapi_security_port_sid(uint16_t port, uint8_t protocol,
                          kapi_security_id_t* out_sid);

int kapi_security_netif_sid(const char* name,
                           kapi_security_id_t* ifsid);

int kapi_security_node_sid(uint16_t addr_family, void* addr,
                          kapi_security_id_t* out_sid);

int kapi_security_fs_use(const char* path,
                        kapi_security_class_t *behavior,
                        kapi_security_id_t *out_sid);

int kapi_security_getprocattr(int field, char** value);

int kapi_security_setprocattr(const char* name, void* value, size_t size);

int kapi_security_create(const char* filename, mode_t mode);

int kapi_security_access(const char* filename, int mask);

int kapi_security_link(const char* oldname, const char* newname);

int kapi_security_unlink(const char* filename);

int kapi_security_rename(const char* oldname, const char* newname);

int kapi_security_mkdir(const char* dirname, mode_t mode);

int kapi_security_rmdir(const char* dirname);

int kapi_security_mknod(const char* filename, mode_t mode, dev_t dev);

int kapi_security_chmod(struct dentry *dentry, struct vfsmnt *mnt, mode_t mode);

int kapi_security_chown(struct dentry *dentry, struct vfsmnt *mnt,
                       uid_t user, gid_t group);

int kapi_security_truncate(struct dentry *dentry, loff_t length);

int kapi_security_getattr(struct vfsmnt *mnt, struct dentry *dentry);

int kapi_security_setattr(struct dentry *dentry, struct iattr *iattr);

int kapi_security_setxattr(struct dentry *dentry, const char *name,
                          const void *value, size_t size, int flags);

int kapi_security_removexattr(struct dentry *dentry, const char *name);

int kapi_security_getxattr(struct dentry *dentry, const char *name,
                          void *value, size_t size);

int kapi_security_listxattr(struct dentry *dentry, char *list, size_t list_size);

int kapi_security_socket_create(int family, int type, int protocol, int kern);

int kapi_security_socket_post_create(struct socket *sock, int family,
                                    int type, int protocol, int kern);

int kapi_security_socket_bind(struct socket *sock,
                             struct sockaddr *address, int addrlen);

int kapi_security_socket_connect(struct socket *sock,
                               struct sockaddr *address, int addrlen);

int kapi_security_socket_listen(struct socket *sock, int backlog);

int kapi_security_socket_accept(struct socket *sock, struct socket *newsock);

int kapi_security_socket_sendmsg(struct socket *sock, struct msghdr *msg,
                                int size);

int kapi_security_socket_recvmsg(struct socket *sock, struct msghdr *msg,
                                int size, int flags);

int kapi_security_socket_getsockname(struct socket *sock);

int kapi_security_socket_getpeername(struct socket *sock);

int kapi_security_socket_setsockopt(struct socket *sock, int level,
                                   int optname);

int kapi_security_socket_getsockopt(struct socket *sock, int level,
                                   int optname);

int kapi_security_socket_shutdown(struct socket *sock, int how);

int kapi_task_create(unsigned long clone_flags);

int kapi_task_alloc_security(struct task_struct *p);

void kapi_task_free_security(struct task_struct *p);

int kapi_task_setuid(uid_t id0, uid_t id1, uid_t id2, uid_t id3,
                    struct cred *new);

int kapi_task_setgid(gid_t id0, gid_t id1, gid_t id2, gid_t id3,
                    struct cred *new);

int kapi_task_setpgid(struct task_struct *p, pid_t pgid);

int kapi_task_getpgid(struct task_struct *p);

int kapi_task_getsid(struct task_struct *p);

void kapi_task_getsecid(struct task_struct *p, u32 *secid);

int kapi_task_setnice(struct task_struct *p, int nice);

int kapi_task_setscheduler(struct task_struct *p, int policy,
                         struct sched_param *lp);

int kapi_task_getscheduler(struct task_struct *p);

int kapi_task_movememory(struct task_struct *p);

int kapi_task_kill(struct task_struct *p, struct siginfo *info,
                  int sig, u32 secid);

int kapi_task_wait(struct task_struct *p);

int kapi_task_prctl(int option, unsigned long arg2, unsigned long arg3,
                   unsigned long arg4, unsigned long arg5);

int kapi_audit_init(void);

void kapi_audit_log(int type, const char* fmt, ...);

void kapi_audit_syscall_entry(int arch, int major, unsigned long a0,
                             unsigned long a1, unsigned long a2,
                             unsigned long a3);

void kapi_audit_syscall_exit(int success, long return_code);

int kapi_register_audit_callback(kapi_audit_callback_t callback);

int kapi_unregister_audit_callback(kapi_audit_callback_t callback);

int kapi_enable_audit(bool enable);

bool kapi_is_audit_enabled(void);

int kapi_set_audit_filter(int type, int result, const char* pattern);

int kapi_clear_audit_filters(void);

#ifdef __cplusplus
}
#endif

#endif