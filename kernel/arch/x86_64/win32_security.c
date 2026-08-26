#include <arch/win32.h>
#include <arch/slab.h>
#include <string.h>
extern void* memory_alloc(uint64_t size);
extern void memory_free(void* p);

#define SECURITY_NULL_SID_AUTHORITY       {0,0,0,0,0,0}
#define SECURITY_WORLD_SID_AUTHORITY      {0,0,0,0,0,1}
#define SECURITY_LOCAL_SID_AUTHORITY      {0,0,0,0,0,2}
#define SECURITY_CREATOR_SID_AUTHORITY    {0,0,0,0,0,3}
#define SECURITY_NON_UNIQUE_AUTHORITY     {0,0,0,0,0,4}
#define SECURITY_NT_AUTHORITY             {0,0,0,0,0,5}

#define SECURITY_WORLD_RID                 0
#define SECURITY_LOCAL_RID                 0
#define SECURITY_CREATOR_OWNER_RID         0
#define SECURITY_CREATOR_GROUP_RID         1
#define SECURITY_CREATOR_OWNER_SERVER_RID  2
#define SECURITY_CREATOR_GROUP_SERVER_RID  3
#define SECURITY_DIALUP_RID                1
#define SECURITY_NETWORK_RID               2
#define SECURITY_BATCH_RID                 3
#define SECURITY_INTERACTIVE_RID           4
#define SECURITY_SERVICE_RID               6
#define SECURITY_ANONYMOUS_LOGON_RID       7
#define SECURITY_PROXY_RID                 8
#define SECURITY_ENTERPRISE_CONTROLLERS_RID 9
#define SECURITY_SERVER_LOGON_RID          9
#define SECURITY_PRINCIPAL_SELF_RID        10
#define SECURITY_AUTHENTICATED_USER_RID    11
#define SECURITY_RESTRICTED_CODE_RID       12
#define SECURITY_TERMINAL_SERVER_RID       14
#define SECURITY_REMOTE_LOGON_RID          15
#define SECURITY_THIS_ORGANIZATION_RID     16

#define DOMAIN_USER_RID_ADMIN              0x000001F4
#define DOMAIN_USER_RID_GUEST              0x000001F5
#define DOMAIN_USER_RID_KRBTGT             0x000001F6

#define DOMAIN_GROUP_RID_ADMINS            0x00000200
#define DOMAIN_GROUP_RID_USERS             0x00000201
#define DOMAIN_GROUP_RID_GUESTS            0x00000202

#define SECURITY_LOCAL_SYSTEM_RID          0x00000012
#define SECURITY_LOCAL_SERVICE_RID         0x00000013
#define SECURITY_NETWORK_SERVICE_RID       0x00000014

typedef struct _SID_IDENTIFIER_AUTHORITY {
    uint8_t Value[6];
} SID_IDENTIFIER_AUTHORITY;

typedef struct _SID {
    uint8_t  Revision;
    uint8_t  SubAuthorityCount;
    SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
    uint32_t SubAuthority[1];
} SID, *PSID;

typedef uint32_t ACCESS_MASK;

typedef struct _ACE_HEADER {
    uint8_t  AceType;
    uint8_t  AceFlags;
    uint16_t AceSize;
} ACE_HEADER;

#define ACCESS_ALLOWED_ACE_TYPE 0
#define ACCESS_DENIED_ACE_TYPE  1
#define SYSTEM_AUDIT_ACE_TYPE   2

#define OBJECT_INHERIT_ACE      0x01
#define CONTAINER_INHERIT_ACE   0x02
#define NO_PROPAGATE_INHERIT_ACE 0x04
#define INHERIT_ONLY_ACE        0x08
#define INHERITED_ACE           0x10
#define VALID_INHERIT_FLAGS     0x1F
#define SUCCESSFUL_ACCESS_ACE_FLAG 0x40
#define FAILED_ACCESS_ACE_FLAG  0x80

typedef struct _ACCESS_ALLOWED_ACE {
    ACE_HEADER Header;
    ACCESS_MASK Mask;
    uint32_t SidStart;
} ACCESS_ALLOWED_ACE;

typedef struct _ACCESS_DENIED_ACE {
    ACE_HEADER Header;
    ACCESS_MASK Mask;
    uint32_t SidStart;
} ACCESS_DENIED_ACE;

typedef struct _ACL {
    uint8_t  AclRevision;
    uint8_t  Sbz1;
    uint16_t AclSize;
    uint16_t AceCount;
    uint16_t Sbz2;
} ACL, *PACL;

#define SECURITY_DESCRIPTOR_REVISION 1
#define SE_OWNER_DEFAULTED          0x0001
#define SE_GROUP_DEFAULTED          0x0002
#define SE_DACL_PRESENT             0x0004
#define SE_DACL_DEFAULTED           0x0008
#define SE_SACL_PRESENT             0x0010
#define SE_SACL_DEFAULTED           0x0020
#define SE_DACL_AUTO_INHERIT_REQ    0x0100
#define SE_SACL_AUTO_INHERIT_REQ    0x0200
#define SE_DACL_AUTO_INHERITED      0x0400
#define SE_SACL_AUTO_INHERITED      0x0800
#define SE_DACL_PROTECTED           0x1000
#define SE_SACL_PROTECTED           0x2000
#define SE_RM_CONTROL_VALID         0x4000
#define SE_SELF_RELATIVE            0x8000

typedef struct _SECURITY_DESCRIPTOR {
    uint8_t  Revision;
    uint8_t  Sbz1;
    uint16_t Control;
    uint32_t Owner;
    uint32_t Group;
    uint32_t Sacl;
    uint32_t Dacl;
} SECURITY_DESCRIPTOR, *PISECURITY_DESCRIPTOR, *PSECURITY_DESCRIPTOR;

#define SE_PRIVILEGE_ENABLED_BY_DEFAULT 0x00000001
#define SE_PRIVILEGE_ENABLED             0x00000002
#define SE_PRIVILEGE_REMOVED             0x00000004
#define SE_PRIVILEGE_USED_FOR_ACCESS     0x80000000

#define SE_MIN_WELL_KNOWN_PRIVILEGE (2L)
#define SE_CREATE_TOKEN_PRIVILEGE (2L)
#define SE_ASSIGNPRIMARYTOKEN_PRIVILEGE (3L)
#define SE_LOCK_MEMORY_PRIVILEGE (4L)
#define SE_INCREASE_QUOTA_PRIVILEGE (5L)
#define SE_UNSOLICITED_INPUT_PRIVILEGE (6L)
#define SE_MACHINE_ACCOUNT_PRIVILEGE (7L)
#define SE_TCB_PRIVILEGE (9L)
#define SE_SECURITY_PRIVILEGE (10L)
#define SE_TAKE_OWNERSHIP_PRIVILEGE (11L)
#define SE_LOAD_DRIVER_PRIVILEGE (12L)
#define SE_SYSTEM_PROFILE_PRIVILEGE (13L)
#define SE_SYSTEMTIME_PRIVILEGE (14L)
#define SE_PROF_SINGLE_PROCESS_PRIVILEGE (15L)
#define SE_INC_BASE_PRIORITY_PRIVILEGE (16L)
#define SE_CREATE_PAGEFILE_PRIVILEGE (17L)
#define SE_CREATE_PERMANENT_PRIVILEGE (18L)
#define SE_AUDIT_PRIVILEGE (19L)
#define SE_BACKUP_PRIVILEGE (20L)
#define SE_RESTORE_PRIVILEGE (21L)
#define SE_SHUTDOWN_PRIVILEGE (22L)
#define SE_DEBUG_PRIVILEGE (23L)
#define SE_AUDIT_SUBSYSTEM_PRIVILEGE (25L)
#define SE_CHANGE_NOTIFY_PRIVILEGE (23L)

typedef struct _TOKEN {
    uint32_t TokenType;
    uint32_t ImpersonationLevel;
    uint32_t SessionId;
    LUID   AuthenticationId;
    uint64_t ExpirationTime;
    void*  UserSid;
    void*  GroupSids;
    uint32_t GroupCount;
    void*  Privileges;
    uint32_t PrivilegeCount;
    void*  OwnerSid;
    void*  PrimaryGroupSid;
    void*  DefaultDacl;
    HANDLE TokenHandle;
} TOKEN, *PTOKEN;

#define ERROR_SERVICE_EXISTS 1073

static void set_last_error(uint32_t err) {
    extern uint32_t GetLastError(void);
    extern void SetLastError(uint32_t dwErrCode);
    SetLastError(err);
}

BOOL InitializeSecurityDescriptor(PSECURITY_DESCRIPTOR pSD, uint32_t dwRevision) {
    if (pSD == NULL) return FALSE;
    if (dwRevision != SECURITY_DESCRIPTOR_REVISION) return FALSE;
    pSD->Revision = (uint8_t)dwRevision;
    pSD->Sbz1 = 0;
    pSD->Control = 0;
    pSD->Owner = 0;
    pSD->Group = 0;
    pSD->Sacl = 0;
    pSD->Dacl = 0;
    return TRUE;
}

BOOL SetSecurityDescriptorOwner(PSECURITY_DESCRIPTOR pSD, PSID pOwner, BOOL bOwnerDefaulted) {
    if (pSD == NULL) return FALSE;
    if (pOwner != NULL) {
        pSD->Owner = (uint32_t)((uint8_t*)pOwner - (uint8_t*)pSD);
    } else {
        pSD->Owner = 0;
    }
    if (bOwnerDefaulted) {
        pSD->Control |= SE_OWNER_DEFAULTED;
    } else {
        pSD->Control &= (uint16_t)~SE_OWNER_DEFAULTED;
    }
    pSD->Control |= SE_SELF_RELATIVE;
    return TRUE;
}

BOOL SetSecurityDescriptorGroup(PSECURITY_DESCRIPTOR pSD, PSID pGroup, BOOL bGroupDefaulted) {
    if (pSD == NULL) return FALSE;
    if (pGroup != NULL) {
        pSD->Group = (uint32_t)((uint8_t*)pGroup - (uint8_t*)pSD);
    } else {
        pSD->Group = 0;
    }
    if (bGroupDefaulted) {
        pSD->Control |= SE_GROUP_DEFAULTED;
    } else {
        pSD->Control &= (uint16_t)~SE_GROUP_DEFAULTED;
    }
    pSD->Control |= SE_SELF_RELATIVE;
    return TRUE;
}

BOOL SetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR pSD, BOOL bDaclPresent, PACL pDacl, BOOL bDaclDefaulted) {
    if (pSD == NULL) return FALSE;
    if (bDaclPresent) {
        pSD->Control |= SE_DACL_PRESENT;
        if (pDacl != NULL) {
            pSD->Dacl = (uint32_t)((uint8_t*)pDacl - (uint8_t*)pSD);
        } else {
            pSD->Dacl = 0;
        }
    } else {
        pSD->Control &= (uint16_t)~SE_DACL_PRESENT;
        pSD->Dacl = 0;
    }
    if (bDaclDefaulted) {
        pSD->Control |= SE_DACL_DEFAULTED;
    } else {
        pSD->Control &= (uint16_t)~SE_DACL_DEFAULTED;
    }
    pSD->Control |= SE_SELF_RELATIVE;
    return TRUE;
}

BOOL GetSecurityDescriptorOwner(PSECURITY_DESCRIPTOR pSD, PSID* pOwner, BOOL* lpbOwnerDefaulted) {
    if (pSD == NULL || pOwner == NULL) return FALSE;
    if (pSD->Owner == 0) {
        *pOwner = NULL;
    } else {
        *pOwner = (PSID)((uint8_t*)pSD + pSD->Owner);
    }
    if (lpbOwnerDefaulted != NULL) {
        *lpbOwnerDefaulted = (pSD->Control & SE_OWNER_DEFAULTED) ? TRUE : FALSE;
    }
    return TRUE;
}

BOOL GetSecurityDescriptorGroup(PSECURITY_DESCRIPTOR pSD, PSID* pGroup, BOOL* lpbGroupDefaulted) {
    if (pSD == NULL || pGroup == NULL) return FALSE;
    if (pSD->Group == 0) {
        *pGroup = NULL;
    } else {
        *pGroup = (PSID)((uint8_t*)pSD + pSD->Group);
    }
    if (lpbGroupDefaulted != NULL) {
        *lpbGroupDefaulted = (pSD->Control & SE_GROUP_DEFAULTED) ? TRUE : FALSE;
    }
    return TRUE;
}

BOOL GetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR pSD, BOOL* lpbDaclPresent, PACL* pDacl, BOOL* lpbDaclDefaulted) {
    if (pSD == NULL) return FALSE;
    if (lpbDaclPresent != NULL) {
        *lpbDaclPresent = (pSD->Control & SE_DACL_PRESENT) ? TRUE : FALSE;
    }
    if (pDacl != NULL) {
        if (pSD->Dacl == 0) {
            *pDacl = NULL;
        } else {
            *pDacl = (PACL)((uint8_t*)pSD + pSD->Dacl);
        }
    }
    if (lpbDaclDefaulted != NULL) {
        *lpbDaclDefaulted = (pSD->Control & SE_DACL_DEFAULTED) ? TRUE : FALSE;
    }
    return TRUE;
}

uint32_t GetSecurityDescriptorControl(PSECURITY_DESCRIPTOR pSD, uint16_t* pControl, uint32_t* lpdwRevision) {
    if (pSD == NULL) return 0;
    if (pControl != NULL) {
        *pControl = pSD->Control;
    }
    if (lpdwRevision != NULL) {
        *lpdwRevision = (uint32_t)pSD->Revision;
    }
    return (uint32_t)pSD->Control;
}

BOOL InitializeAcl(PACL pAcl, uint32_t nAclLength, uint32_t dwAclRevision) {
    if (pAcl == NULL) return FALSE;
    if (nAclLength < sizeof(ACL)) return FALSE;
    if (dwAclRevision != 2) return FALSE;
    memset(pAcl, 0, nAclLength);
    pAcl->AclRevision = (uint8_t)dwAclRevision;
    pAcl->Sbz1 = 0;
    pAcl->AclSize = (uint16_t)nAclLength;
    pAcl->AceCount = 0;
    pAcl->Sbz2 = 0;
    return TRUE;
}

uint32_t GetLengthSid(PSID pSid) {
    if (pSid == NULL) return 0;
    return 8u + 4u * (uint32_t)pSid->SubAuthorityCount;
}

static uint16_t get_ace_size_for_sid(PSID pSid) {
    uint32_t sid_len = GetLengthSid(pSid);
    return (uint16_t)(sizeof(ACE_HEADER) + sizeof(ACCESS_MASK) + sid_len);
}

BOOL AddAccessAllowedAce(PACL pAcl, uint32_t dwAceRevision, ACCESS_MASK AccessMask, PSID pSid) {
    uint16_t ace_size;
    uint16_t new_size;
    uint8_t* ace_ptr;
    ACCESS_ALLOWED_ACE* ace;
    uint32_t sid_len;

    if (pAcl == NULL || pSid == NULL) return FALSE;
    if (dwAceRevision != 2) return FALSE;

    ace_size = get_ace_size_for_sid(pSid);
    new_size = (uint16_t)(pAcl->AclSize + ace_size);
    if (new_size > pAcl->AclSize && (uint32_t)pAcl->AceCount < 0xFFFFu) {
        ace_ptr = (uint8_t*)pAcl + pAcl->AclSize;
        ace = (ACCESS_ALLOWED_ACE*)ace_ptr;
        ace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
        ace->Header.AceFlags = 0;
        ace->Header.AceSize = ace_size;
        ace->Mask = AccessMask;
        sid_len = GetLengthSid(pSid);
        memcpy(&ace->SidStart, pSid, sid_len);
        pAcl->AceCount++;
        pAcl->AclSize = new_size;
        return TRUE;
    }
    return FALSE;
}

BOOL AddAccessDeniedAce(PACL pAcl, uint32_t dwAceRevision, ACCESS_MASK AccessMask, PSID pSid) {
    uint16_t ace_size;
    uint16_t new_size;
    uint8_t* ace_ptr;
    ACCESS_DENIED_ACE* ace;
    uint32_t sid_len;

    if (pAcl == NULL || pSid == NULL) return FALSE;
    if (dwAceRevision != 2) return FALSE;

    ace_size = get_ace_size_for_sid(pSid);
    new_size = (uint16_t)(pAcl->AclSize + ace_size);
    if (new_size > pAcl->AclSize && (uint32_t)pAcl->AceCount < 0xFFFFu) {
        ace_ptr = (uint8_t*)pAcl + pAcl->AclSize;
        ace = (ACCESS_DENIED_ACE*)ace_ptr;
        ace->Header.AceType = ACCESS_DENIED_ACE_TYPE;
        ace->Header.AceFlags = 0;
        ace->Header.AceSize = ace_size;
        ace->Mask = AccessMask;
        sid_len = GetLengthSid(pSid);
        memcpy(&ace->SidStart, pSid, sid_len);
        pAcl->AceCount++;
        pAcl->AclSize = new_size;
        return TRUE;
    }
    return FALSE;
}

#define WELL_KNOWN_SID_TYPE_NULL 0
#define WELL_KNOWN_SID_TYPE_WORLD 1
#define WELL_KNOWN_SID_TYPE_LOCAL 2
#define WELL_KNOWN_SID_TYPE_CREATOR_OWNER 3
#define WELL_KNOWN_SID_TYPE_CREATOR_GROUP 4
#define WELL_KNOWN_SID_TYPE_NT_AUTHORITY 5
#define WELL_KNOWN_SID_TYPE_LOCAL_SYSTEM 16
#define WELL_KNOWN_SID_TYPE_LOCAL_SERVICE 19
#define WELL_KNOWN_SID_TYPE_NETWORK_SERVICE 20
#define WELL_KNOWN_SID_TYPE_AUTHENTICATED_USER 17
#define WELL_KNOWN_SID_TYPE_BUILTIN_ADMINS 26
#define WELL_KNOWN_SID_TYPE_BUILTIN_USERS 27
#define WELL_KNOWN_SID_TYPE_BUILTIN_GUESTS 28

uint8_t* CreateWellKnownSid(uint32_t WellKnownSidType, PSID DomainSid, PSID pSid, uint32_t* cbSid) {
    SID_IDENTIFIER_AUTHORITY world_auth = SECURITY_WORLD_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY local_auth = SECURITY_LOCAL_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY creator_auth = SECURITY_CREATOR_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY nt_auth = SECURITY_NT_AUTHORITY;
    PSID out = pSid;
    uint32_t need_len = 0;

    (void)DomainSid;

    switch (WellKnownSidType) {
        case WELL_KNOWN_SID_TYPE_WORLD:
        case WELL_KNOWN_SID_TYPE_NULL:
            need_len = 8u + 4u * 1u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 1;
                memcpy(&out->IdentifierAuthority, &world_auth, 6);
                out->SubAuthority[0] = SECURITY_WORLD_RID;
            }
            break;
        case WELL_KNOWN_SID_TYPE_LOCAL:
            need_len = 8u + 4u * 1u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 1;
                memcpy(&out->IdentifierAuthority, &local_auth, 6);
                out->SubAuthority[0] = SECURITY_LOCAL_RID;
            }
            break;
        case WELL_KNOWN_SID_TYPE_CREATOR_OWNER:
            need_len = 8u + 4u * 1u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 1;
                memcpy(&out->IdentifierAuthority, &creator_auth, 6);
                out->SubAuthority[0] = SECURITY_CREATOR_OWNER_RID;
            }
            break;
        case WELL_KNOWN_SID_TYPE_CREATOR_GROUP:
            need_len = 8u + 4u * 1u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 1;
                memcpy(&out->IdentifierAuthority, &creator_auth, 6);
                out->SubAuthority[0] = SECURITY_CREATOR_GROUP_RID;
            }
            break;
        case WELL_KNOWN_SID_TYPE_LOCAL_SYSTEM:
            need_len = 8u + 4u * 1u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 1;
                memcpy(&out->IdentifierAuthority, &nt_auth, 6);
                out->SubAuthority[0] = SECURITY_LOCAL_SYSTEM_RID;
            }
            break;
        case WELL_KNOWN_SID_TYPE_LOCAL_SERVICE:
            need_len = 8u + 4u * 1u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 1;
                memcpy(&out->IdentifierAuthority, &nt_auth, 6);
                out->SubAuthority[0] = SECURITY_LOCAL_SERVICE_RID;
            }
            break;
        case WELL_KNOWN_SID_TYPE_NETWORK_SERVICE:
            need_len = 8u + 4u * 1u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 1;
                memcpy(&out->IdentifierAuthority, &nt_auth, 6);
                out->SubAuthority[0] = SECURITY_NETWORK_SERVICE_RID;
            }
            break;
        case WELL_KNOWN_SID_TYPE_AUTHENTICATED_USER:
            need_len = 8u + 4u * 1u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 1;
                memcpy(&out->IdentifierAuthority, &nt_auth, 6);
                out->SubAuthority[0] = SECURITY_AUTHENTICATED_USER_RID;
            }
            break;
        case WELL_KNOWN_SID_TYPE_BUILTIN_ADMINS:
            need_len = 8u + 4u * 2u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 2;
                memcpy(&out->IdentifierAuthority, &nt_auth, 6);
                out->SubAuthority[0] = 32u;
                out->SubAuthority[1] = DOMAIN_GROUP_RID_ADMINS;
            }
            break;
        case WELL_KNOWN_SID_TYPE_BUILTIN_USERS:
            need_len = 8u + 4u * 2u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 2;
                memcpy(&out->IdentifierAuthority, &nt_auth, 6);
                out->SubAuthority[0] = 32u;
                out->SubAuthority[1] = DOMAIN_GROUP_RID_USERS;
            }
            break;
        case WELL_KNOWN_SID_TYPE_BUILTIN_GUESTS:
            need_len = 8u + 4u * 2u;
            if (out != NULL && cbSid != NULL && *cbSid >= need_len) {
                out->Revision = 1;
                out->SubAuthorityCount = 2;
                memcpy(&out->IdentifierAuthority, &nt_auth, 6);
                out->SubAuthority[0] = 32u;
                out->SubAuthority[1] = DOMAIN_GROUP_RID_GUESTS;
            }
            break;
        default:
            return NULL;
    }

    if (cbSid != NULL) {
        *cbSid = need_len;
    }
    return (uint8_t*)out;
}

BOOL EqualSid(PSID pSid1, PSID pSid2) {
    uint32_t len1, len2;
    if (pSid1 == NULL || pSid2 == NULL) return FALSE;
    if (pSid1->Revision != pSid2->Revision) return FALSE;
    if (pSid1->SubAuthorityCount != pSid2->SubAuthorityCount) return FALSE;
    len1 = GetLengthSid(pSid1);
    len2 = GetLengthSid(pSid2);
    if (len1 != len2) return FALSE;
    return memcmp(pSid1, pSid2, len1) == 0 ? TRUE : FALSE;
}

BOOL CopySid(uint32_t nDestinationSidLength, PSID pDestinationSid, PSID pSourceSid) {
    uint32_t src_len;
    if (pDestinationSid == NULL || pSourceSid == NULL) return FALSE;
    src_len = GetLengthSid(pSourceSid);
    if (nDestinationSidLength < src_len) return FALSE;
    memcpy(pDestinationSid, pSourceSid, src_len);
    return TRUE;
}

PSID AllocateAndInitializeSid(SID_IDENTIFIER_AUTHORITY* pIdentifierAuthority, uint8_t nSubAuthorityCount,
                               uint32_t sa0, uint32_t sa1, uint32_t sa2, uint32_t sa3,
                               uint32_t sa4, uint32_t sa5, uint32_t sa6, uint32_t sa7) {
    uint32_t total_len;
    PSID sid;
    uint32_t subs[8];
    uint8_t i;

    if (pIdentifierAuthority == NULL || nSubAuthorityCount > 8 || nSubAuthorityCount == 0) return NULL;

    subs[0] = sa0; subs[1] = sa1; subs[2] = sa2; subs[3] = sa3;
    subs[4] = sa4; subs[5] = sa5; subs[6] = sa6; subs[7] = sa7;

    total_len = 8u + 4u * (uint32_t)nSubAuthorityCount;
    sid = (PSID)memory_alloc((uint64_t)total_len);
    if (sid == NULL) return NULL;
    memset(sid, 0, total_len);
    sid->Revision = 1;
    sid->SubAuthorityCount = nSubAuthorityCount;
    memcpy(&sid->IdentifierAuthority, pIdentifierAuthority, 6);
    for (i = 0; i < nSubAuthorityCount; i++) {
        sid->SubAuthority[i] = subs[i];
    }
    return sid;
}

uint32_t GetSidIdentifierAuthority(PSID pSid, SID_IDENTIFIER_AUTHORITY** ppsia) {
    if (pSid == NULL || ppsia == NULL) return 0;
    *ppsia = &pSid->IdentifierAuthority;
    return 0;
}

uint32_t* GetSidSubAuthority(PSID pSid, uint32_t nSubAuthority) {
    if (pSid == NULL) return NULL;
    if (nSubAuthority >= (uint32_t)pSid->SubAuthorityCount) return NULL;
    return &pSid->SubAuthority[nSubAuthority];
}

uint8_t GetSidSubAuthorityCount(PSID pSid) {
    if (pSid == NULL) return 0;
    return pSid->SubAuthorityCount;
}

BOOL IsValidSid(PSID pSid) {
    if (pSid == NULL) return FALSE;
    if (pSid->Revision != 1) return FALSE;
    if (pSid->SubAuthorityCount > 15) return FALSE;
    return TRUE;
}

static BOOL sid_is_world_or_auth(PSID pSid) {
    SID_IDENTIFIER_AUTHORITY world_auth = SECURITY_WORLD_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY nt_auth = SECURITY_NT_AUTHORITY;
    if (memcmp(&pSid->IdentifierAuthority, &world_auth, 6) == 0) return TRUE;
    if (memcmp(&pSid->IdentifierAuthority, &nt_auth, 6) == 0 &&
        pSid->SubAuthorityCount >= 1 &&
        pSid->SubAuthority[0] == SECURITY_AUTHENTICATED_USER_RID) return TRUE;
    if (memcmp(&pSid->IdentifierAuthority, &nt_auth, 6) == 0 &&
        pSid->SubAuthorityCount >= 1 &&
        pSid->SubAuthority[0] == SECURITY_LOCAL_SYSTEM_RID) return TRUE;
    return FALSE;
}

BOOL AccessCheck(PSECURITY_DESCRIPTOR pSecurityDescriptor, HANDLE ClientToken,
                 ACCESS_MASK DesiredAccess, ACCESS_MASK* GrantedAccess, uint32_t* AccessStatus) {
    BOOL dacl_present = FALSE;
    PACL dacl = NULL;
    ACCESS_MASK granted = 0;
    ACCESS_MASK denied = 0;
    uint16_t i;
    uint8_t* ace_base;

    (void)ClientToken;

    if (pSecurityDescriptor == NULL) {
        if (GrantedAccess != NULL) *GrantedAccess = DesiredAccess;
        if (AccessStatus != NULL) *AccessStatus = 0;
        return TRUE;
    }

    GetSecurityDescriptorDacl(pSecurityDescriptor, &dacl_present, &dacl, NULL);
    if (!dacl_present || dacl == NULL) {
        if (GrantedAccess != NULL) *GrantedAccess = DesiredAccess;
        if (AccessStatus != NULL) *AccessStatus = 0;
        return TRUE;
    }

    ace_base = (uint8_t*)dacl + sizeof(ACL);
    for (i = 0; i < dacl->AceCount; i++) {
        ACE_HEADER* hdr = (ACE_HEADER*)ace_base;
        if (hdr->AceType == ACCESS_ALLOWED_ACE_TYPE) {
            ACCESS_ALLOWED_ACE* aa = (ACCESS_ALLOWED_ACE*)ace_base;
            PSID sid = (PSID)&aa->SidStart;
            if (sid_is_world_or_auth(sid)) {
                granted |= aa->Mask;
            }
        } else if (hdr->AceType == ACCESS_DENIED_ACE_TYPE) {
            ACCESS_DENIED_ACE* da = (ACCESS_DENIED_ACE*)ace_base;
            PSID sid = (PSID)&da->SidStart;
            if (sid_is_world_or_auth(sid)) {
                denied |= da->Mask;
            }
        }
        ace_base += hdr->AceSize;
    }

    granted = (granted & ~denied);

    if (GrantedAccess != NULL) *GrantedAccess = granted;
    if ((granted & DesiredAccess) == DesiredAccess) {
        if (AccessStatus != NULL) *AccessStatus = 0;
        return TRUE;
    }
    if (AccessStatus != NULL) *AccessStatus = 0xC0000022u;
    return FALSE;
}

static PTOKEN g_system_token = NULL;
static HANDLE g_next_token_handle = (HANDLE)0x1000;

static PTOKEN create_system_token(void) {
    PTOKEN tok;
    SID_IDENTIFIER_AUTHORITY nt_auth = SECURITY_NT_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY world_auth = SECURITY_WORLD_SID_AUTHORITY;
    PSID sys_sid;
    PSID* groups;
    LUID_AND_ATTRIBUTES* privs;
    uint32_t i;

    tok = (PTOKEN)memory_alloc(sizeof(TOKEN));
    if (tok == NULL) return NULL;
    memset(tok, 0, sizeof(TOKEN));

    tok->TokenType = 1;
    tok->ImpersonationLevel = 0;
    tok->SessionId = 0;
    tok->AuthenticationId.LowPart = 0x3E7u;
    tok->AuthenticationId.HighPart = 0;
    tok->ExpirationTime = 0x7FFFFFFFFFFFFFFFull;

    sys_sid = AllocateAndInitializeSid(&nt_auth, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0);
    tok->UserSid = sys_sid;

    tok->GroupCount = 3;
    groups = (PSID*)memory_alloc(sizeof(PSID) * 3u);
    groups[0] = AllocateAndInitializeSid(&nt_auth, 2, 32u, DOMAIN_GROUP_RID_ADMINS, 0, 0, 0, 0, 0, 0);
    groups[1] = AllocateAndInitializeSid(&nt_auth, 1, SECURITY_AUTHENTICATED_USER_RID, 0, 0, 0, 0, 0, 0, 0);
    groups[2] = AllocateAndInitializeSid(&world_auth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0);
    tok->GroupSids = groups;

    tok->PrivilegeCount = 24;
    privs = (LUID_AND_ATTRIBUTES*)memory_alloc(sizeof(LUID_AND_ATTRIBUTES) * 24u);
    for (i = 0; i < 24u; i++) {
        privs[i].Luid.LowPart = 2u + i;
        privs[i].Luid.HighPart = 0;
        privs[i].Attributes = SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT;
    }
    tok->Privileges = privs;

    tok->OwnerSid = sys_sid;
    tok->PrimaryGroupSid = groups[0];
    tok->DefaultDacl = NULL;
    tok->TokenHandle = g_next_token_handle;
    g_next_token_handle = (HANDLE)((uint8_t*)g_next_token_handle + 1);

    return tok;
}

BOOL OpenProcessToken(HANDLE ProcessHandle, uint32_t DesiredAccess, HANDLE* TokenHandle) {
    (void)ProcessHandle;
    (void)DesiredAccess;
    if (TokenHandle == NULL) return FALSE;
    if (g_system_token == NULL) {
        g_system_token = create_system_token();
        if (g_system_token == NULL) return FALSE;
    }
    *TokenHandle = g_system_token->TokenHandle;
    return TRUE;
}

BOOL OpenThreadToken(HANDLE ThreadHandle, uint32_t DesiredAccess, BOOL OpenAsSelf, HANDLE* TokenHandle) {
    (void)ThreadHandle;
    (void)DesiredAccess;
    (void)OpenAsSelf;
    return OpenProcessToken(NULL, 0, TokenHandle);
}

BOOL PrivilegeCheck(HANDLE ClientToken, void* RequiredPrivileges, BOOL* pfResult) {
    (void)ClientToken;
    (void)RequiredPrivileges;
    if (pfResult == NULL) return FALSE;
    *pfResult = TRUE;
    return TRUE;
}

typedef struct {
    const char* name;
    uint32_t luid_low;
} priv_name_map_t;

static const priv_name_map_t g_priv_names[] = {
    {"SeCreateTokenPrivilege",          2},
    {"SeAssignPrimaryTokenPrivilege",   3},
    {"SeLockMemoryPrivilege",           4},
    {"SeIncreaseQuotaPrivilege",        5},
    {"SeUnsolicitedInputPrivilege",     6},
    {"SeMachineAccountPrivilege",       7},
    {"SeTcbPrivilege",                  9},
    {"SeSecurityPrivilege",            10},
    {"SeTakeOwnershipPrivilege",       11},
    {"SeLoadDriverPrivilege",          12},
    {"SeSystemProfilePrivilege",       13},
    {"SeSystemtimePrivilege",          14},
    {"SeProfileSingleProcessPrivilege",15},
    {"SeIncreaseBasePriorityPrivilege",16},
    {"SeCreatePagefilePrivilege",      17},
    {"SeCreatePermanentPrivilege",     18},
    {"SeAuditPrivilege",               19},
    {"SeBackupPrivilege",              20},
    {"SeRestorePrivilege",             21},
    {"SeShutdownPrivilege",            22},
    {"SeDebugPrivilege",               23},
    {"SeChangeNotifyPrivilege",        23},
    {"SeAuditSubsystemPrivilege",      25},
};

BOOL LookupPrivilegeValueA(const char* lpSystemName, const char* lpName, PLUID lpLuid) {
    uint32_t i;
    uint32_t count;
    (void)lpSystemName;
    if (lpName == NULL || lpLuid == NULL) return FALSE;
    count = sizeof(g_priv_names) / sizeof(g_priv_names[0]);
    for (i = 0; i < count; i++) {
        if (strcmp(g_priv_names[i].name, lpName) == 0) {
            lpLuid->LowPart = g_priv_names[i].luid_low;
            lpLuid->HighPart = 0;
            return TRUE;
        }
    }
    lpLuid->LowPart = 23u;
    lpLuid->HighPart = 0;
    return TRUE;
}

typedef struct {
    uint8_t auth[6];
    uint8_t sub_count;
    uint32_t subs[2];
    const char* name;
    const char* domain;
    uint32_t use;
} sid_name_map_t;

#define SidTypeUser 1
#define SidTypeGroup 2
#define SidTypeDomain 3
#define SidTypeAlias 4
#define SidTypeWellKnownGroup 5

static const sid_name_map_t g_sid_names[] = {
    {{0,0,0,0,0,5}, 1, {SECURITY_LOCAL_SYSTEM_RID, 0},    "SYSTEM",          "NT AUTHORITY",   SidTypeWellKnownGroup},
    {{0,0,0,0,0,5}, 1, {SECURITY_LOCAL_SERVICE_RID, 0},   "LOCAL SERVICE",   "NT AUTHORITY",   SidTypeWellKnownGroup},
    {{0,0,0,0,0,5}, 1, {SECURITY_NETWORK_SERVICE_RID, 0}, "NETWORK SERVICE", "NT AUTHORITY",   SidTypeWellKnownGroup},
    {{0,0,0,0,0,5}, 1, {SECURITY_AUTHENTICATED_USER_RID, 0}, "Authenticated Users", "NT AUTHORITY", SidTypeWellKnownGroup},
    {{0,0,0,0,0,1}, 1, {SECURITY_WORLD_RID, 0},          "Everyone",        "Everyone",       SidTypeWellKnownGroup},
    {{0,0,0,0,0,5}, 2, {32u, DOMAIN_GROUP_RID_ADMINS},    "Administrators",  "BUILTIN",        SidTypeAlias},
    {{0,0,0,0,0,5}, 2, {32u, DOMAIN_GROUP_RID_USERS},     "Users",           "BUILTIN",        SidTypeAlias},
    {{0,0,0,0,0,5}, 2, {32u, DOMAIN_GROUP_RID_GUESTS},    "Guests",          "BUILTIN",        SidTypeAlias},
    {{0,0,0,0,0,5}, 1, {DOMAIN_USER_RID_ADMIN, 0},       "Administrator",   "DOMAIN",         SidTypeUser},
    {{0,0,0,0,0,5}, 1, {DOMAIN_USER_RID_GUEST, 0},       "Guest",           "DOMAIN",         SidTypeUser},
};

static const sid_name_map_t* find_sid_map(PSID pSid) {
    uint32_t i;
    uint32_t count;
    count = sizeof(g_sid_names) / sizeof(g_sid_names[0]);
    for (i = 0; i < count; i++) {
        const sid_name_map_t* m = &g_sid_names[i];
        uint8_t j;
        if (memcmp(pSid->IdentifierAuthority.Value, m->auth, 6) != 0) continue;
        if (pSid->SubAuthorityCount != m->sub_count) continue;
        for (j = 0; j < m->sub_count; j++) {
            if (pSid->SubAuthority[j] != m->subs[j]) break;
        }
        if (j == m->sub_count) return m;
    }
    return NULL;
}

BOOL LookupAccountSidA(const char* lpSystemName, void* Sid, char* Name, LPDWORD cchName,
                        char* ReferencedDomainName, LPDWORD cchReferencedDomainName, PSID_NAME_USE peUse) {
    const sid_name_map_t* m;
    size_t name_len;
    size_t dom_len;
    PSID psid;
    (void)lpSystemName;
    if (Sid == NULL || cchName == NULL) return FALSE;
    psid = (PSID)Sid;
    m = find_sid_map(psid);
    if (m == NULL) {
        if (Name != NULL && *cchName >= 8) strcpy(Name, "Unknown");
        *cchName = 8u;
        if (ReferencedDomainName != NULL && cchReferencedDomainName != NULL && *cchReferencedDomainName >= 7) {
            strcpy(ReferencedDomainName, "Unknown");
        }
        if (cchReferencedDomainName != NULL) *cchReferencedDomainName = 7u;
        if (peUse != NULL) *peUse = SidTypeUser;
        return TRUE;
    }
    name_len = strlen(m->name) + 1u;
    dom_len = strlen(m->domain) + 1u;
    if (Name != NULL && cchName != NULL && *cchName >= (uint32_t)name_len) {
        strcpy(Name, m->name);
    }
    if (ReferencedDomainName != NULL && cchReferencedDomainName != NULL && *cchReferencedDomainName >= (uint32_t)dom_len) {
        strcpy(ReferencedDomainName, m->domain);
    }
    *cchName = (uint32_t)name_len;
    if (cchReferencedDomainName != NULL) *cchReferencedDomainName = (uint32_t)dom_len;
    if (peUse != NULL) *peUse = m->use;
    return TRUE;
}

static const sid_name_map_t* find_name_map(const char* lpAccountName) {
    uint32_t i;
    uint32_t count;
    size_t alen;
    if (lpAccountName == NULL) return NULL;
    alen = strlen(lpAccountName);
    count = sizeof(g_sid_names) / sizeof(g_sid_names[0]);
    for (i = 0; i < count; i++) {
        const sid_name_map_t* m = &g_sid_names[i];
        if (strlen(m->name) == alen && strcmp(m->name, lpAccountName) == 0) return m;
    }
    return NULL;
}

BOOL LookupAccountNameA(const char* lpSystemName, const char* lpAccountName, void* Sid, LPDWORD cbSid,
                         char* ReferencedDomainName, LPDWORD cchReferencedDomainName, PSID_NAME_USE peUse) {
    const sid_name_map_t* m;
    uint32_t need_sid;
    size_t dom_len;
    (void)lpSystemName;
    if (lpAccountName == NULL || cbSid == NULL) return FALSE;
    m = find_name_map(lpAccountName);
    if (m == NULL) {
        SID_IDENTIFIER_AUTHORITY nt_auth = SECURITY_NT_AUTHORITY;
        need_sid = 8u + 4u * 1u;
        if (Sid != NULL && *cbSid >= need_sid) {
            PSID psid = (PSID)Sid;
            psid->Revision = 1;
            psid->SubAuthorityCount = 1;
            memcpy(&psid->IdentifierAuthority, &nt_auth, 6);
            psid->SubAuthority[0] = DOMAIN_USER_RID_ADMIN;
        }
        *cbSid = need_sid;
        if (ReferencedDomainName != NULL && cchReferencedDomainName != NULL && *cchReferencedDomainName >= 7) {
            strcpy(ReferencedDomainName, "BUILTIN");
        }
        if (cchReferencedDomainName != NULL) *cchReferencedDomainName = 7u;
        if (peUse != NULL) *peUse = SidTypeUser;
        return TRUE;
    }
    need_sid = 8u + 4u * (uint32_t)m->sub_count;
    if (Sid != NULL && *cbSid >= need_sid) {
        uint8_t j;
        PSID psid = (PSID)Sid;
        psid->Revision = 1;
        psid->SubAuthorityCount = m->sub_count;
        memcpy(psid->IdentifierAuthority.Value, m->auth, 6);
        for (j = 0; j < m->sub_count; j++) {
            psid->SubAuthority[j] = m->subs[j];
        }
    }
    *cbSid = need_sid;
    dom_len = strlen(m->domain) + 1u;
    if (ReferencedDomainName != NULL && cchReferencedDomainName != NULL && *cchReferencedDomainName >= (uint32_t)dom_len) {
        strcpy(ReferencedDomainName, m->domain);
    }
    if (cchReferencedDomainName != NULL) *cchReferencedDomainName = (uint32_t)dom_len;
    if (peUse != NULL) *peUse = m->use;
    return TRUE;
}
