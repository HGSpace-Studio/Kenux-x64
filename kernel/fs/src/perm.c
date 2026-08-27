#include "perm.h"
#include <string.h>

void fs_perm_init(fs_perm_t* perm, uid_t uid, gid_t gid, uint16_t mode)
{
    if (!perm) return;
    memset(perm, 0, sizeof(fs_perm_t));
    spin_init(&perm->lock);
    spin_init(&perm->acl.lock);
    perm->uid = uid;
    perm->gid = gid;
    perm->mode = mode;
}

int fs_perm_check(fs_perm_t* perm, uid_t uid, gid_t gid, int access)
{
    if (!perm) return -1;
    spinlock_acquire(&perm->lock);

    uint16_t mode = perm->mode;
    int result = 0;

    if (uid == 0) {
        if (access & PERM_READ) result |= (mode & (PERM_OWNER_READ | PERM_GROUP_READ | PERM_OTHER_READ)) ? 1 : 1;
        if (access & PERM_WRITE) result |= (mode & (PERM_OWNER_WRITE | PERM_GROUP_WRITE | PERM_OTHER_WRITE)) ? 1 : 0;
        if (access & PERM_EXEC) result |= (mode & (PERM_OWNER_EXEC | PERM_GROUP_EXEC | PERM_OTHER_EXEC)) ? 1 : 1;
        if ((access & PERM_WRITE) && !(mode & (PERM_OWNER_WRITE | PERM_GROUP_WRITE | PERM_OTHER_WRITE))) {
            spinlock_release(&perm->lock);
            return -2;
        }
        spinlock_release(&perm->lock);
        return 0;
    }

    if (uid == perm->uid) {
        uint16_t owner_bits = (mode >> PERM_OWNER_SHIFT) & 0x07;
        if ((access & PERM_READ) && !(owner_bits & PERM_READ)) { spinlock_release(&perm->lock); return -2; }
        if ((access & PERM_WRITE) && !(owner_bits & PERM_WRITE)) { spinlock_release(&perm->lock); return -2; }
        if ((access & PERM_EXEC) && !(owner_bits & PERM_EXEC)) { spinlock_release(&perm->lock); return -2; }
        spinlock_release(&perm->lock);
        return 0;
    }

    if (gid == perm->gid) {
        uint16_t group_bits = (mode >> PERM_GROUP_SHIFT) & 0x07;
        if ((access & PERM_READ) && !(group_bits & PERM_READ)) { spinlock_release(&perm->lock); return -2; }
        if ((access & PERM_WRITE) && !(group_bits & PERM_WRITE)) { spinlock_release(&perm->lock); return -2; }
        if ((access & PERM_EXEC) && !(group_bits & PERM_EXEC)) { spinlock_release(&perm->lock); return -2; }
        spinlock_release(&perm->lock);
        return 0;
    }

    uint16_t other_bits = (mode >> PERM_OTHER_SHIFT) & 0x07;
    if ((access & PERM_READ) && !(other_bits & PERM_READ)) { spinlock_release(&perm->lock); return -2; }
    if ((access & PERM_WRITE) && !(other_bits & PERM_WRITE)) { spinlock_release(&perm->lock); return -2; }
    if ((access & PERM_EXEC) && !(other_bits & PERM_EXEC)) { spinlock_release(&perm->lock); return -2; }

    spinlock_release(&perm->lock);
    return 0;
}

int fs_perm_chmod(fs_perm_t* perm, uint16_t mode)
{
    if (!perm) return -1;
    spinlock_acquire(&perm->lock);
    perm->mode = (perm->mode & 0xF000) | (mode & 0x0FFF);
    spinlock_release(&perm->lock);
    return 0;
}

int fs_perm_chown(fs_perm_t* perm, uid_t uid, gid_t gid)
{
    if (!perm) return -1;
    spinlock_acquire(&perm->lock);
    if ((uint32_t)uid != (uint32_t)-1) perm->uid = uid;
    if ((uint32_t)gid != (uint32_t)-1) perm->gid = gid;
    spinlock_release(&perm->lock);
    return 0;
}

int fs_perm_acl_add(fs_perm_t* perm, const acl_entry_t* entry)
{
    if (!perm || !entry) return -1;
    spinlock_acquire(&perm->acl.lock);
    if (perm->acl.count >= ACL_MAX_ENTRIES) { spinlock_release(&perm->acl.lock); return -2; }
    perm->acl.entries[perm->acl.count] = *entry;
    perm->acl.count++;
    spinlock_release(&perm->acl.lock);
    return 0;
}

int fs_perm_acl_remove(fs_perm_t* perm, int index)
{
    if (!perm) return -1;
    spinlock_acquire(&perm->acl.lock);
    if (index < 0 || index >= perm->acl.count) { spinlock_release(&perm->acl.lock); return -2; }
    for (int i = index; i < perm->acl.count - 1; i++) {
        perm->acl.entries[i] = perm->acl.entries[i + 1];
    }
    perm->acl.count--;
    spinlock_release(&perm->acl.lock);
    return 0;
}

int fs_perm_acl_check(fs_perm_t* perm, uid_t uid, gid_t gid, int access)
{
    if (!perm) return -1;
    spinlock_acquire(&perm->acl.lock);

    for (int i = 0; i < perm->acl.count; i++) {
        acl_entry_t* e = &perm->acl.entries[i];
        int match = 0;

        switch (e->type) {
        case ACL_TYPE_USER_OBJ: match = (uid == perm->uid); break;
        case ACL_TYPE_USER: match = (uid == e->uid); break;
        case ACL_TYPE_GROUP_OBJ: match = (gid == perm->gid); break;
        case ACL_TYPE_GROUP: match = (gid == e->gid); break;
        case ACL_TYPE_MASK: match = 1; break;
        case ACL_TYPE_OTHER: match = 1; break;
        default: break;
        }

        if (match) {
            int allowed = 1;
            if ((access & PERM_READ) && !(e->perm & PERM_READ)) allowed = 0;
            if ((access & PERM_WRITE) && !(e->perm & PERM_WRITE)) allowed = 0;
            if ((access & PERM_EXEC) && !(e->perm & PERM_EXEC)) allowed = 0;

            if (e->tag == ACL_TAG_DENY && !allowed) {
                spinlock_release(&perm->acl.lock);
                return -2;
            }
            if (e->tag == ACL_TAG_ALLOW && allowed) {
                spinlock_release(&perm->acl.lock);
                return 0;
            }
        }
    }

    spinlock_release(&perm->acl.lock);
    return fs_perm_check(perm, uid, gid, access);
}

uint16_t fs_perm_mode_from_acl(acl_t* acl)
{
    if (!acl) return 0;
    uint16_t mode = 0;

    for (int i = 0; i < acl->count; i++) {
        acl_entry_t* e = &acl->entries[i];
        if (e->tag != ACL_TAG_ALLOW) continue;

        switch (e->type) {
        case ACL_TYPE_USER_OBJ:
            if (e->perm & PERM_READ) mode |= PERM_OWNER_READ;
            if (e->perm & PERM_WRITE) mode |= PERM_OWNER_WRITE;
            if (e->perm & PERM_EXEC) mode |= PERM_OWNER_EXEC;
            break;
        case ACL_TYPE_GROUP_OBJ:
            if (e->perm & PERM_READ) mode |= PERM_GROUP_READ;
            if (e->perm & PERM_WRITE) mode |= PERM_GROUP_WRITE;
            if (e->perm & PERM_EXEC) mode |= PERM_GROUP_EXEC;
            break;
        case ACL_TYPE_OTHER:
            if (e->perm & PERM_READ) mode |= PERM_OTHER_READ;
            if (e->perm & PERM_WRITE) mode |= PERM_OTHER_WRITE;
            if (e->perm & PERM_EXEC) mode |= PERM_OTHER_EXEC;
            break;
        default: break;
        }
    }
    return mode;
}