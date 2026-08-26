#ifndef _ARCH_LSM_H
#define _ARCH_LSM_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define LSM_ORDER_FIRST        0
#define LSM_ORDER_SECONDARY    1
#define LSM_NAME_MAX            32
#define LSM_MAX_MODULES         8

#define LSM_HOOK_MAX            80

/* Forward declarations - avoid type conflicts with other subsystems */
struct inode;
struct file;
struct dentry;
struct vfsmount;
struct task;
struct socket;
struct sockaddr;
struct msg;
struct sk_buff;
struct kernfs_node;
struct linux_binprm;
struct cred;
struct vm_area;
struct file_lock;
struct path;
struct qstr;

typedef struct inode inode_t;
typedef struct file file_t;
typedef struct dentry dentry_t;
typedef struct vfsmount vfsmount_t;
typedef struct task task_t;
typedef struct socket socket_t;
typedef struct sockaddr sockaddr_t;
typedef struct msg msg_t;
typedef struct sk_buff sk_buff_t;
typedef struct kernfs_node kernfs_node_t;
typedef struct linux_binprm linux_binprm_t;
typedef struct cred cred_t;
typedef struct vm_area vm_area_t;
typedef struct file_lock file_lock_t;
typedef struct path path_t;
typedef struct qstr qstr_t;

#define MAY_EXEC    1
#define MAY_WRITE   2
#define MAY_READ    4
#define MAY_APPEND  8
#define MAY_ACCESS  16

typedef int (*lsm_inode_permission_fn)(inode_t *inode, int mask);
typedef int (*lsm_file_permission_fn)(file_t *file, int mask);
typedef int (*lsm_task_create_fn)(task_t *task);
typedef void (*lsm_task_free_fn)(task_t *task);
typedef int (*lsm_socket_bind_fn)(socket_t *sock, sockaddr_t *addr, int addr_len);
typedef int (*lsm_socket_connect_fn)(socket_t *sock, sockaddr_t *addr, int addr_len);
typedef int (*lsm_socket_accept_fn)(socket_t *sock, socket_t *newsock);
typedef int (*lsm_socket_sendmsg_fn)(socket_t *sock, msg_t *msg, int size);
typedef int (*lsm_socket_recvmsg_fn)(socket_t *sock, msg_t *msg, int size, int flags);
typedef int (*lsm_inode_setxattr_fn)(dentry_t *dentry, const char *name, const void *value, s64 size, int flags);
typedef int (*lsm_inode_getxattr_fn)(dentry_t *dentry, const char *name);
typedef int (*lsm_inode_removexattr_fn)(dentry_t *dentry, const char *name);
typedef int (*lsm_capability_fn)(task_t *task, int cap, int audit);
typedef int (*lsm_sysctl_fn)(const char *name, int op);
typedef int (*lsm_mmap_file_fn)(file_t *file, unsigned long reqprot, unsigned long prot, unsigned long flags);
typedef int (*lsm_file_lock_fn)(file_t *file, file_lock_t *fl);
typedef int (*lsm_kernfs_init_fn)(kernfs_node_t *kn);
typedef int (*lsm_kernfs_symlink_fn)(kernfs_node_t *parent, kernfs_node_t *kn);
typedef int (*lsm_bprm_check_security_fn)(linux_binprm_t *bprm);
typedef int (*lsm_bprm_set_creds_fn)(linux_binprm_t *bprm);
typedef int (*lsm_inode_create_fn)(inode_t *dir, dentry_t *dentry, int mode);
typedef int (*lsm_inode_link_fn)(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry);
typedef int (*lsm_inode_unlink_fn)(inode_t *dir, dentry_t *dentry);
typedef int (*lsm_inode_symlink_fn)(inode_t *dir, dentry_t *dentry, const char *name);
typedef int (*lsm_inode_mkdir_fn)(inode_t *dir, dentry_t *dentry, int mode);
typedef int (*lsm_inode_rmdir_fn)(inode_t *dir, dentry_t *dentry);
typedef int (*lsm_inode_rename_fn)(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry);
typedef int (*lsm_inode_setattr_fn)(dentry_t *dentry, int iattr);
typedef int (*lsm_inode_getattr_fn)(path_t *path);
typedef int (*lsm_inode_setsecurity_fn)(inode_t *inode, const char *name, const void *value, s64 size, int flags);
typedef int (*lsm_inode_getsecurity_fn)(inode_t *inode, const char *name, void **buffer, bool alloc);
typedef int (*lsm_inode_listsecurity_fn)(inode_t *inode, char *buffer, s64 buffer_size);
typedef int (*lsm_file_open_fn)(file_t *file);
typedef int (*lsm_file_receive_fn)(file_t *file);
typedef int (*lsm_task_setuid_fn)(cred_t *new, cred_t *old);
typedef int (*lsm_task_setgid_fn)(cred_t *new, cred_t *old);
typedef int (*lsm_task_setpgid_fn)(task_t *task, pid_t pgid);
typedef int (*lsm_task_getpgid_fn)(task_t *task);
typedef int (*lsm_task_getsid_fn)(task_t *task);
typedef int (*lsm_task_getsecid_fn)(task_t *task, u32 *secid);
typedef int (*lsm_task_setnice_fn)(task_t *task, int nice);
typedef int (*lsm_task_setioprio_fn)(task_t *task, int ioprio);
typedef int (*lsm_task_getioprio_fn)(task_t *task);
typedef int (*lsm_task_setscheduler_fn)(task_t *task);
typedef int (*lsm_task_getscheduler_fn)(task_t *task);
typedef int (*lsm_task_movememory_fn)(task_t *task);
typedef int (*lsm_task_kill_fn)(task_t *task, int sig);
typedef int (*lsm_task_wait_fn)(task_t *task);
typedef int (*lsm_ipc_permission_fn)(void *ipc_perm, int flag);
typedef int (*lsm_msg_msg_alloc_security_fn)(void *msg);
typedef void (*lsm_msg_msg_free_security_fn)(void *msg);
typedef int (*lms_shm_alloc_security_fn)(void *shp);
typedef void (*lsm_shm_free_security_fn)(void *shp);
typedef int (*lsm_net_sendmsg_fn)(socket_t *sock, msg_t *msg, int size);
typedef int (*lsm_net_recvmsg_fn)(socket_t *sock, msg_t *msg, int size, int flags);
typedef int (*lsm_d_instantiate_fn)(dentry_t *dentry, inode_t *inode);
typedef int (*lsm_vm_enough_memory_fn)(task_t *task, u64 pages);
typedef int (*lsm_inode_permission2_fn)(inode_t *inode, int mask);
typedef int (*lsm_inode_readlink_fn)(dentry_t *dentry);
typedef int (*lsm_inode_follow_link_fn)(dentry_t *dentry, inode_t *inode, bool rcu);
typedef int (*lsm_inode_permission_xattr_fn)(inode_t *inode, int mask);
typedef int (*lsm_file_fcntl_fn)(file_t *file, unsigned int cmd, unsigned long arg);
typedef int (*lsm_file_mprotect_fn)(vm_area_t *vma, unsigned long reqprot, unsigned long prot);
typedef int (*lsm_socket_listen_fn)(socket_t *sock, int backlog);
typedef int (*lsm_socket_shutdown_fn)(socket_t *sock, int how);
typedef int (*lsm_socket_getsockname_fn)(socket_t *sock);
typedef int (*lsm_socket_getpeername_fn)(socket_t *sock);
typedef int (*lsm_sk_alloc_security_fn)(void *sock, int family, int priority);
typedef void (*lsm_sk_free_security_fn)(void *sock);
typedef int (*lsm_sk_bind_fn)(void *sock, sockaddr_t *addr);

typedef struct security_operations {
    char name[LSM_NAME_MAX];
    lsm_inode_permission_fn inode_permission;
    lsm_file_permission_fn file_permission;
    lsm_task_create_fn task_create;
    lsm_task_free_fn task_free;
    lsm_socket_bind_fn socket_bind;
    lsm_socket_connect_fn socket_connect;
    lsm_socket_accept_fn socket_accept;
    lsm_socket_sendmsg_fn socket_sendmsg;
    lsm_socket_recvmsg_fn socket_recvmsg;
    lsm_inode_setxattr_fn inode_setxattr;
    lsm_inode_getxattr_fn inode_getxattr;
    lsm_inode_removexattr_fn inode_removexattr;
    lsm_capability_fn capability;
    lsm_sysctl_fn sysctl;
    lsm_mmap_file_fn mmap_file;
    lsm_file_lock_fn file_lock;
    lsm_kernfs_init_fn kernfs_init;
    lsm_kernfs_symlink_fn kernfs_symlink;
    lsm_bprm_check_security_fn bprm_check_security;
    lsm_bprm_set_creds_fn bprm_set_creds;
    lsm_inode_create_fn inode_create;
    lsm_inode_link_fn inode_link;
    lsm_inode_unlink_fn inode_unlink;
    lsm_inode_symlink_fn inode_symlink;
    lsm_inode_mkdir_fn inode_mkdir;
    lsm_inode_rmdir_fn inode_rmdir;
    lsm_inode_rename_fn inode_rename;
    lsm_inode_setattr_fn inode_setattr;
    lsm_inode_getattr_fn inode_getattr;
    lsm_inode_setsecurity_fn inode_setsecurity;
    lsm_inode_getsecurity_fn inode_getsecurity;
    lsm_inode_listsecurity_fn inode_listsecurity;
    lsm_file_open_fn file_open;
    lsm_file_receive_fn file_receive;
    lsm_task_setuid_fn task_setuid;
    lsm_task_setgid_fn task_setgid;
    lsm_task_setpgid_fn task_setpgid;
    lsm_task_getpgid_fn task_getpgid;
    lsm_task_getsid_fn task_getsid;
    lsm_task_getsecid_fn task_getsecid;
    lsm_task_setnice_fn task_setnice;
    lsm_task_setioprio_fn task_setioprio;
    lsm_task_getioprio_fn task_getioprio;
    lsm_task_setscheduler_fn task_setscheduler;
    lsm_task_getscheduler_fn task_getscheduler;
    lsm_task_movememory_fn task_movememory;
    lsm_task_kill_fn task_kill;
    lsm_task_wait_fn task_wait;
    lsm_ipc_permission_fn ipc_permission;
    lsm_msg_msg_alloc_security_fn msg_msg_alloc_security;
    lsm_msg_msg_free_security_fn msg_msg_free_security;
    lms_shm_alloc_security_fn shm_alloc_security;
    lsm_shm_free_security_fn shm_free_security;
    lsm_net_sendmsg_fn net_sendmsg;
    lsm_net_recvmsg_fn net_recvmsg;
    lsm_d_instantiate_fn d_instantiate;
    lsm_vm_enough_memory_fn vm_enough_memory;
    lsm_inode_readlink_fn inode_readlink;
    lsm_inode_follow_link_fn inode_follow_link;
    lsm_file_fcntl_fn file_fcntl;
    lsm_file_mprotect_fn file_mprotect;
    lsm_socket_listen_fn socket_listen;
    lsm_socket_shutdown_fn socket_shutdown;
    lsm_socket_getsockname_fn socket_getsockname;
    lsm_socket_getpeername_fn socket_getpeername;
    lsm_sk_alloc_security_fn sk_alloc_security;
    lsm_sk_free_security_fn sk_free_security;
    lsm_sk_bind_fn sk_bind;
    void *generic_unused[32];
} security_operations_t;

typedef struct lsm_module {
    char name[LSM_NAME_MAX];
    security_operations_t *ops;
    int order;
    int active;
    struct lsm_module *next;
} lsm_module_t;

int lsm_init(void);
int lsm_register_security(const char *name, security_operations_t *ops);
int lsm_unregister_security(const char *name);
security_operations_t *lsm_get_operations(const char *name);

int security_inode_permission(inode_t *inode, int mask);
int security_file_permission(file_t *file, int mask);
int security_task_create(task_t *task);
void security_task_free(task_t *task);
int security_socket_bind(socket_t *sock, sockaddr_t *addr, int addr_len);
int security_socket_connect(socket_t *sock, sockaddr_t *addr, int addr_len);
int security_socket_accept(socket_t *sock, socket_t *newsock);
int security_socket_sendmsg(socket_t *sock, msg_t *msg, int size);
int security_socket_recvmsg(socket_t *sock, msg_t *msg, int size, int flags);
int security_inode_setxattr(dentry_t *dentry, const char *name, const void *value, s64 size, int flags);
int security_inode_getxattr(dentry_t *dentry, const char *name);
int security_inode_removexattr(dentry_t *dentry, const char *name);
int security_capability(task_t *task, int cap, int audit);
int security_sysctl(const char *name, int op);
int security_mmap_file(file_t *file, unsigned long reqprot, unsigned long prot, unsigned long flags);
int security_file_lock(file_t *file, file_lock_t *fl);
int security_kernfs_init(kernfs_node_t *kn);
int security_kernfs_symlink(kernfs_node_t *parent, kernfs_node_t *kn);
int security_bprm_check(linux_binprm_t *bprm);
int security_bprm_set_creds(linux_binprm_t *bprm);
int security_inode_create(inode_t *dir, dentry_t *dentry, int mode);
int security_inode_link(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry);
int security_inode_unlink(inode_t *dir, dentry_t *dentry);
int security_inode_mkdir(inode_t *dir, dentry_t *dentry, int mode);
int security_inode_rmdir(inode_t *dir, dentry_t *dentry);
int security_inode_rename(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry);
int security_inode_setattr(dentry_t *dentry, int iattr);
int security_inode_getattr(path_t *path);
int security_file_open(file_t *file);
int security_file_receive(file_t *file);
int security_task_setuid(cred_t *new, cred_t *old);
int security_task_setgid(cred_t *new, cred_t *old);
int security_task_kill(task_t *task, int sig);
int security_task_setnice(task_t *task, int nice);
int security_task_setscheduler(task_t *task);
int security_ipc_permission(void *ipc_perm, int flag);
int security_socket_listen(socket_t *sock, int backlog);
int security_socket_shutdown(socket_t *sock, int how);

extern int lsm_initialized;
extern spinlock_t lsm_lock;

#endif
