#include <arch/lsm.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

int lsm_initialized = 0;
spinlock_t lsm_lock = SPINLOCK_INIT;

static lsm_module_t *lsm_modules = NULL;
static security_operations_t lsm_default_ops;
static security_operations_t *lsm_primary_ops = NULL;

static int lsm_call_chain_inode_permission(inode_t *inode, int mask)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_permission) {
            int rc = mod->ops->inode_permission(inode, mask);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_file_permission(file_t *file, int mask)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->file_permission) {
            int rc = mod->ops->file_permission(file, mask);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_task_create(task_t *task)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->task_create) {
            int rc = mod->ops->task_create(task);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static void lsm_call_chain_task_free(task_t *task)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->task_free) {
            mod->ops->task_free(task);
        }
        mod = mod->next;
    }
}

static int lsm_call_chain_socket_bind(socket_t *sock, sockaddr_t *addr, int addr_len)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->socket_bind) {
            int rc = mod->ops->socket_bind(sock, addr, addr_len);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_socket_connect(socket_t *sock, sockaddr_t *addr, int addr_len)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->socket_connect) {
            int rc = mod->ops->socket_connect(sock, addr, addr_len);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_socket_accept(socket_t *sock, socket_t *newsock)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->socket_accept) {
            int rc = mod->ops->socket_accept(sock, newsock);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_socket_sendmsg(socket_t *sock, msg_t *msg, int size)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->socket_sendmsg) {
            int rc = mod->ops->socket_sendmsg(sock, msg, size);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_socket_recvmsg(socket_t *sock, msg_t *msg, int size, int flags)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->socket_recvmsg) {
            int rc = mod->ops->socket_recvmsg(sock, msg, size, flags);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_inode_setxattr(dentry_t *dentry, const char *name, const void *value, s64 size, int flags)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_setxattr) {
            int rc = mod->ops->inode_setxattr(dentry, name, value, size, flags);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_inode_getxattr(dentry_t *dentry, const char *name)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_getxattr) {
            int rc = mod->ops->inode_getxattr(dentry, name);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_inode_removexattr(dentry_t *dentry, const char *name)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_removexattr) {
            int rc = mod->ops->inode_removexattr(dentry, name);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_capability(task_t *task, int cap, int audit)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->capability) {
            int rc = mod->ops->capability(task, cap, audit);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_sysctl(const char *name, int op)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->sysctl) {
            int rc = mod->ops->sysctl(name, op);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_mmap_file(file_t *file, unsigned long reqprot, unsigned long prot, unsigned long flags)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->mmap_file) {
            int rc = mod->ops->mmap_file(file, reqprot, prot, flags);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_file_lock(file_t *file, file_lock_t *fl)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->file_lock) {
            int rc = mod->ops->file_lock(file, fl);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_kernfs_init(kernfs_node_t *kn)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->kernfs_init) {
            int rc = mod->ops->kernfs_init(kn);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_kernfs_symlink(kernfs_node_t *parent, kernfs_node_t *kn)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->kernfs_symlink) {
            int rc = mod->ops->kernfs_symlink(parent, kn);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_bprm_check(linux_binprm_t *bprm)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->bprm_check_security) {
            int rc = mod->ops->bprm_check_security(bprm);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_bprm_set_creds(linux_binprm_t *bprm)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->bprm_set_creds) {
            int rc = mod->ops->bprm_set_creds(bprm);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_inode_create(inode_t *dir, dentry_t *dentry, int mode)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_create) {
            int rc = mod->ops->inode_create(dir, dentry, mode);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_inode_link(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_link) {
            int rc = mod->ops->inode_link(old_dentry, dir, new_dentry);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_inode_unlink(inode_t *dir, dentry_t *dentry)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_unlink) {
            int rc = mod->ops->inode_unlink(dir, dentry);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_inode_mkdir(inode_t *dir, dentry_t *dentry, int mode)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_mkdir) {
            int rc = mod->ops->inode_mkdir(dir, dentry, mode);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_inode_rmdir(inode_t *dir, dentry_t *dentry)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_rmdir) {
            int rc = mod->ops->inode_rmdir(dir, dentry);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_inode_rename(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->inode_rename) {
            int rc = mod->ops->inode_rename(old_dir, old_dentry, new_dir, new_dentry);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_file_open(file_t *file)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->file_open) {
            int rc = mod->ops->file_open(file);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_task_setuid(cred_t *new_creds, cred_t *old_creds)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->task_setuid) {
            int rc = mod->ops->task_setuid(new_creds, old_creds);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_task_setgid(cred_t *new_creds, cred_t *old_creds)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->task_setgid) {
            int rc = mod->ops->task_setgid(new_creds, old_creds);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_task_kill(task_t *task, int sig)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->task_kill) {
            int rc = mod->ops->task_kill(task, sig);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_task_setnice(task_t *task, int nice)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->task_setnice) {
            int rc = mod->ops->task_setnice(task, nice);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_task_setscheduler(task_t *task)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->task_setscheduler) {
            int rc = mod->ops->task_setscheduler(task);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_ipc_permission(void *ipc_perm, int flag)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->ipc_permission) {
            int rc = mod->ops->ipc_permission(ipc_perm, flag);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_socket_listen(socket_t *sock, int backlog)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->socket_listen) {
            int rc = mod->ops->socket_listen(sock, backlog);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

static int lsm_call_chain_socket_shutdown(socket_t *sock, int how)
{
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (mod->active && mod->ops && mod->ops->socket_shutdown) {
            int rc = mod->ops->socket_shutdown(sock, how);
            if (rc != 0) return rc;
        }
        mod = mod->next;
    }
    return 0;
}

int lsm_init(void)
{
    memset(&lsm_default_ops, 0, sizeof(lsm_default_ops));
    lsm_modules = NULL;
    lsm_primary_ops = NULL;
    lsm_initialized = 0;
    spin_init(&lsm_lock);
    return 0;
}

int lsm_register_security(const char *name, security_operations_t *ops)
{
    if (!name || !ops) return -1;

    spin_lock(&lsm_lock);
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (strncmp(mod->name, name, LSM_NAME_MAX) == 0) {
            spin_unlock(&lsm_lock);
            return -1;
        }
        mod = mod->next;
    }

    mod = kzalloc(sizeof(lsm_module_t));
    if (!mod) {
        spin_unlock(&lsm_lock);
        return -1;
    }

    strncpy(mod->name, name, LSM_NAME_MAX - 1);
    mod->name[LSM_NAME_MAX - 1] = '\0';
    mod->ops = ops;
    mod->order = LSM_ORDER_SECONDARY;
    mod->active = 1;

    if (!lsm_primary_ops) {
        mod->order = LSM_ORDER_FIRST;
        lsm_primary_ops = ops;
        mod->next = lsm_modules;
        lsm_modules = mod;
    } else {
        lsm_module_t *prev = NULL;
        lsm_module_t *curr = lsm_modules;
        while (curr && curr->order == LSM_ORDER_FIRST) {
            prev = curr;
            curr = curr->next;
        }
        mod->next = curr;
        if (prev) {
            prev->next = mod;
        } else {
            lsm_modules = mod;
        }
    }

    lsm_initialized = 1;
    spin_unlock(&lsm_lock);
    return 0;
}

int lsm_unregister_security(const char *name)
{
    if (!name) return -1;

    spin_lock(&lsm_lock);
    lsm_module_t *prev = NULL;
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (strncmp(mod->name, name, LSM_NAME_MAX) == 0) {
            if (prev) {
                prev->next = mod->next;
            } else {
                lsm_modules = mod->next;
            }
            if (mod->ops == lsm_primary_ops) {
                lsm_primary_ops = lsm_modules ? lsm_modules->ops : NULL;
            }
            kfree(mod);
            spin_unlock(&lsm_lock);
            return 0;
        }
        prev = mod;
        mod = mod->next;
    }
    spin_unlock(&lsm_lock);
    return -1;
}

security_operations_t *lsm_get_operations(const char *name)
{
    if (!name) return NULL;
    spin_lock(&lsm_lock);
    lsm_module_t *mod = lsm_modules;
    while (mod) {
        if (strncmp(mod->name, name, LSM_NAME_MAX) == 0) {
            spin_unlock(&lsm_lock);
            return mod->ops;
        }
        mod = mod->next;
    }
    spin_unlock(&lsm_lock);
    return NULL;
}

int security_inode_permission(inode_t *inode, int mask)
{
    return lsm_call_chain_inode_permission(inode, mask);
}

int security_file_permission(file_t *file, int mask)
{
    return lsm_call_chain_file_permission(file, mask);
}

int security_task_create(task_t *task)
{
    return lsm_call_chain_task_create(task);
}

void security_task_free(task_t *task)
{
    lsm_call_chain_task_free(task);
}

int security_socket_bind(socket_t *sock, sockaddr_t *addr, int addr_len)
{
    return lsm_call_chain_socket_bind(sock, addr, addr_len);
}

int security_socket_connect(socket_t *sock, sockaddr_t *addr, int addr_len)
{
    return lsm_call_chain_socket_connect(sock, addr, addr_len);
}

int security_socket_accept(socket_t *sock, socket_t *newsock)
{
    return lsm_call_chain_socket_accept(sock, newsock);
}

int security_socket_sendmsg(socket_t *sock, msg_t *msg, int size)
{
    return lsm_call_chain_socket_sendmsg(sock, msg, size);
}

int security_socket_recvmsg(socket_t *sock, msg_t *msg, int size, int flags)
{
    return lsm_call_chain_socket_recvmsg(sock, msg, size, flags);
}

int security_inode_setxattr(dentry_t *dentry, const char *name, const void *value, s64 size, int flags)
{
    return lsm_call_chain_inode_setxattr(dentry, name, value, size, flags);
}

int security_inode_getxattr(dentry_t *dentry, const char *name)
{
    return lsm_call_chain_inode_getxattr(dentry, name);
}

int security_inode_removexattr(dentry_t *dentry, const char *name)
{
    return lsm_call_chain_inode_removexattr(dentry, name);
}

int security_capability(task_t *task, int cap, int audit)
{
    return lsm_call_chain_capability(task, cap, audit);
}

int security_sysctl(const char *name, int op)
{
    return lsm_call_chain_sysctl(name, op);
}

int security_mmap_file(file_t *file, unsigned long reqprot, unsigned long prot, unsigned long flags)
{
    return lsm_call_chain_mmap_file(file, reqprot, prot, flags);
}

int security_file_lock(file_t *file, file_lock_t *fl)
{
    return lsm_call_chain_file_lock(file, fl);
}

int security_kernfs_init(kernfs_node_t *kn)
{
    return lsm_call_chain_kernfs_init(kn);
}

int security_kernfs_symlink(kernfs_node_t *parent, kernfs_node_t *kn)
{
    return lsm_call_chain_kernfs_symlink(parent, kn);
}

int security_bprm_check(linux_binprm_t *bprm)
{
    return lsm_call_chain_bprm_check(bprm);
}

int security_bprm_set_creds(linux_binprm_t *bprm)
{
    return lsm_call_chain_bprm_set_creds(bprm);
}

int security_inode_create(inode_t *dir, dentry_t *dentry, int mode)
{
    return lsm_call_chain_inode_create(dir, dentry, mode);
}

int security_inode_link(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry)
{
    return lsm_call_chain_inode_link(old_dentry, dir, new_dentry);
}

int security_inode_unlink(inode_t *dir, dentry_t *dentry)
{
    return lsm_call_chain_inode_unlink(dir, dentry);
}

int security_inode_mkdir(inode_t *dir, dentry_t *dentry, int mode)
{
    return lsm_call_chain_inode_mkdir(dir, dentry, mode);
}

int security_inode_rmdir(inode_t *dir, dentry_t *dentry)
{
    return lsm_call_chain_inode_rmdir(dir, dentry);
}

int security_inode_rename(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry)
{
    return lsm_call_chain_inode_rename(old_dir, old_dentry, new_dir, new_dentry);
}

int security_inode_setattr(dentry_t *dentry, int iattr)
{
    (void)dentry;
    (void)iattr;
    return 0;
}

int security_inode_getattr(path_t *path)
{
    (void)path;
    return 0;
}

int security_file_open(file_t *file)
{
    return lsm_call_chain_file_open(file);
}

int security_file_receive(file_t *file)
{
    (void)file;
    return 0;
}

int security_task_setuid(cred_t *new_creds, cred_t *old_creds)
{
    return lsm_call_chain_task_setuid(new_creds, old_creds);
}

int security_task_setgid(cred_t *new_creds, cred_t *old_creds)
{
    return lsm_call_chain_task_setgid(new_creds, old_creds);
}

int security_task_kill(task_t *task, int sig)
{
    return lsm_call_chain_task_kill(task, sig);
}

int security_task_setnice(task_t *task, int nice)
{
    return lsm_call_chain_task_setnice(task, nice);
}

int security_task_setscheduler(task_t *task)
{
    return lsm_call_chain_task_setscheduler(task);
}

int security_ipc_permission(void *ipc_perm, int flag)
{
    return lsm_call_chain_ipc_permission(ipc_perm, flag);
}

int security_socket_listen(socket_t *sock, int backlog)
{
    return lsm_call_chain_socket_listen(sock, backlog);
}

int security_socket_shutdown(socket_t *sock, int how)
{
    return lsm_call_chain_socket_shutdown(sock, how);
}
