#ifndef ARCH_X86_64_REGISTRY_H
#define ARCH_X86_64_REGISTRY_H

#include <arch/types.h>
#include <arch/win32.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#ifndef HKEY_DEFINED
#define HKEY_DEFINED
#endif

#ifndef KEY_QUERY_VALUE
#define KEY_QUERY_VALUE          0x00000001
#define KEY_SET_VALUE            0x00000002
#define KEY_CREATE_SUB_KEY       0x00000004
#define KEY_ENUMERATE_SUB_KEYS   0x00000008
#define KEY_NOTIFY               0x00000010
#define KEY_CREATE_LINK          0x00000020
#define KEY_WOW64_64KEY          0x00000100
#define KEY_WOW64_32KEY          0x00000200

#define KEY_READ                 (KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY | 0x00020000)
#define KEY_WRITE                (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | 0x00020000)
#define KEY_EXECUTE              (KEY_READ)
#define KEY_ALL_ACCESS           (KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY | KEY_CREATE_LINK | 0x000F0000)
#endif

#define REG_NONE                 0
#define REG_SZ                   1
#define REG_EXPAND_SZ            2
#define REG_BINARY               3
#define REG_DWORD                4
#define REG_DWORD_BIG_ENDIAN     5
#define REG_LINK                 6
#define REG_MULTI_SZ             7
#define REG_RESOURCE_LIST        8
#define REG_QWORD                11

#define REG_CREATED_NEW_KEY      0x00000001
#define REG_OPENED_EXISTING_KEY  0x00000002

#define REG_OPTION_VOLATILE      0x00000001
#define REG_OPTION_NON_VOLATILE  0x00000000

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS            0
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND     2
#endif
#ifndef ERROR_ACCESS_DENIED
#define ERROR_ACCESS_DENIED      5
#endif
#ifndef ERROR_INVALID_HANDLE
#define ERROR_INVALID_HANDLE     6
#endif
#ifndef ERROR_NOT_ENOUGH_MEMORY
#define ERROR_NOT_ENOUGH_MEMORY  8
#endif
#ifndef ERROR_INVALID_PARAMETER
#define ERROR_INVALID_PARAMETER  87
#endif
#ifndef ERROR_NO_MORE_ITEMS
#define ERROR_NO_MORE_ITEMS      259
#endif

typedef struct hive_header {
    char     magic[4];
    uint32_t sequence_low;
    uint32_t sequence_high;
    uint64_t last_write_time;
    uint32_t major_version;
    uint32_t minor_version;
    uint32_t file_type;
    uint32_t format;
    uint32_t root_cell_offset;
    uint32_t hive_bins_size;
    uint32_t cluster;
    uint8_t  guid[16];
    uint32_t unknown[15];
} __attribute__((packed)) hive_header_t;

#define NK_FLAG_HIVE_EXIT        0x0001
#define NK_FLAG_HIVE_ENTRY       0x0002
#define NK_FLAG_NO_DELETE        0x0004
#define NK_FLAG_SYMBOLIC_LINK    0x0010
#define NK_FLAG_COMPAT_NAME      0x0020
#define NK_FLAG_PREDEFINED_HANDLE 0x0040
#define NK_FLAG_VIRT_MIRROR_TARGET 0x0080
#define NK_FLAG_VIRT_VIRTUAL_SOURCE 0x0100

typedef struct nk_cell {
    char     magic[2];
    uint16_t flags;
    uint32_t parent_cell_offset;
    uint32_t subkey_count_stable;
    uint32_t subkey_count_volatile;
    uint32_t lf_subkey_list_offset;
    uint32_t lh_subkey_list_offset;
    uint32_t security_cell_offset;
    uint32_t class_cell_offset;
    uint32_t value_list_offset;
    uint32_t value_count;
    uint16_t key_name_length;
    uint16_t class_length;
    uint8_t  key_name[];
} __attribute__((packed)) nk_cell_t;

typedef struct vk_cell {
    char     magic[2];
    uint16_t padding;
    uint16_t value_name_length;
    uint32_t data_length;
    uint32_t type;
    uint16_t flags;
    uint16_t padding2;
    uint8_t  value_name[];
} __attribute__((packed)) vk_cell_t;

#define VK_FLAG_VOLATILE         0x0001

typedef struct sk_cell {
    char     magic[2];
    uint16_t padding;
    uint32_t precomputed_size;
    uint32_t next_cell_offset;
    uint32_t reference_count;
    uint32_t owner_sid_offset;
    uint32_t group_sid_offset;
    uint32_t dacl_offset;
    uint32_t sacl_offset;
    uint32_t header_size;
} __attribute__((packed)) sk_cell_t;

typedef struct lf_entry {
    uint32_t cell_offset;
    uint32_t hash_value;
    uint8_t  name_length;
    uint8_t  name[];
} __attribute__((packed)) lf_entry_t;

typedef struct lf_cell {
    char        magic[2];
    uint16_t    entry_count;
    lf_entry_t  entries[];
} __attribute__((packed)) lf_cell_t;

typedef struct lh_entry {
    uint32_t cell_offset;
    uint32_t hash_value;
    uint8_t  name_length;
    uint8_t  name[];
} __attribute__((packed)) lh_entry_t;

typedef struct lh_cell {
    char        magic[2];
    uint16_t    entry_count;
    lh_entry_t  entries[];
} __attribute__((packed)) lh_cell_t;

typedef struct ri_cell {
    char     magic[2];
    uint16_t entry_count;
    uint32_t offsets[];
} __attribute__((packed)) ri_cell_t;

typedef struct lshash_entry {
    uint8_t hash[16];
} lshash_entry_t;

#ifdef __cplusplus
extern "C" {
#endif

int registry_init(void);

int RegOpenKeyExA(HKEY hKey,
                  const char* lpSubKey,
                  uint32_t ulOptions,
                  REGSAM samDesired,
                  HKEY* phkResult);

int RegCreateKeyExA(HKEY hKey,
                    const char* lpSubKey,
                    uint32_t Reserved,
                    char* lpClass,
                    uint32_t dwOptions,
                    REGSAM samDesired,
                    void* lpSecurityAttributes,
                    HKEY* phkResult,
                    uint32_t* lpdwDisposition);

int RegCloseKey(HKEY hKey);

int RegQueryValueExA(HKEY hKey,
                     const char* lpValueName,
                     uint32_t* lpReserved,
                     uint32_t* lpType,
                     uint8_t* lpData,
                     uint32_t* lpcbData);

int RegSetValueExA(HKEY hKey,
                   const char* lpValueName,
                   uint32_t Reserved,
                   uint32_t dwType,
                   const uint8_t* lpData,
                   uint32_t cbData);

int RegEnumKeyExA(HKEY hKey,
                  uint32_t dwIndex,
                  char* lpName,
                  uint32_t* lpcchName,
                  uint32_t* lpReserved,
                  char* lpClass,
                  uint32_t* lpcchClass,
                  uint64_t* lpftLastWriteTime);

int RegEnumValueA(HKEY hKey,
                  uint32_t dwIndex,
                  char* lpValueName,
                  uint32_t* lpcchValueName,
                  uint32_t* lpReserved,
                  uint32_t* lpType,
                  uint8_t* lpData,
                  uint32_t* lpcbData);

int RegDeleteKeyA(HKEY hKey, const char* lpSubKey);

int RegDeleteValueA(HKEY hKey, const char* lpValueName);

int RegFlushKey(HKEY hKey);

int RegGetValueA(HKEY hkey,
                 const char* lpSubKey,
                 const char* lpValue,
                 uint32_t dwFlags,
                 uint32_t* pdwType,
                 void* pvData,
                 uint32_t* pcbData);

#ifdef __cplusplus
}
#endif

#endif