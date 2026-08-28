#ifndef KERNEL_FS_PERM_H
#define KERNEL_FS_PERM_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define PERM_READ    0x04
#define PERM_WRITE   0x02
#define PERM_EXEC    0x01

#define PERM_OWNER_SHIFT  6
#define PERM_GROUP_SHIFT  3
#define PERM_OTHER_SHIFT  0

#define PERM_OWNER_READ   (PERM_READ << PERM_OWNER_SHIFT)
#define PERM_OWNER_WRITE  (PERM_WRITE << PERM_OWNER_SHIFT)
#define PERM_OWNER_EXEC   (PERM_EXEC << PERM_OWNER_SHIFT)
#define PERM_GROUP_READ   (PERM_READ << PERM_GROUP_SHIFT)
#define PERM_GROUP_WRITE  (PERM_WRITE << PERM_GROUP_SHIFT)
#define PERM_GROUP_EXEC   (PERM_EXEC << PERM_GROUP_SHIFT)
#define PERM_OTHER_READ   (PERM_READ << PERM_OTHER_SHIFT)
#define PERM_OTHER_WRITE  (PERM_WRITE << PERM_OTHER_SHIFT)
#define PERM_OTHER_EXEC   (PERM_EXEC << PERM_OTHER_SHIFT)

#define PERM_SETUID       0x0800
#define PERM_SETGID       0x0400
#define PERM_STICKY       0x0200

#define ACL_MAX_ENTRIES   32

#define ACL_TYPE_USER_OBJ  0x01
#define ACL_TYPE_USER      0x02
#define ACL_TYPE_GROUP_OBJ 0x04
#define ACL_TYPE_GROUP     0x08
#define ACL_TYPE_MASK      0x10
#define ACL_TYPE_OTHER     0x20

#define ACL_TAG_ALLOW      0x01
#define ACL_TAG_DENY       0x02

typedef struct {
    uint8_t  type;
    uint8_t  tag;
    uid_t    uid;
    gid_t    gid;
    uint16_t perm;
} acl_entry_t;

typedef struct {
    acl_entry_t entries[ACL_MAX_ENTRIES];
    int         count;
    spinlock_t  lock;
} acl_t;

typedef struct {
    uid_t    uid;
    gid_t    gid;
    uint16_t mode;
    acl_t    acl;
    spinlock_t lock;
} fs_perm_t;

void     fs_perm_init(fs_perm_t* perm, uid_t uid, gid_t gid, uint16_t mode);
int      fs_perm_check(fs_perm_t* perm, uid_t uid, gid_t gid, int access);
int      fs_perm_chmod(fs_perm_t* perm, uint16_t mode);
int      fs_perm_chown(fs_perm_t* perm, uid_t uid, gid_t gid);
int      fs_perm_acl_add(fs_perm_t* perm, const acl_entry_t* entry);
int      fs_perm_acl_remove(fs_perm_t* perm, int index);
int      fs_perm_acl_check(fs_perm_t* perm, uid_t uid, gid_t gid, int access);
uint16_t fs_perm_mode_from_acl(acl_t* acl);

#endif