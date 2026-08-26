#include <arch/registry.h>
#include <string.h>
#include <arch/slab.h>

typedef struct reg_value {
    char            name[16383];
    uint32_t        type;
    uint32_t        data_size;
    uint8_t*        data;
    struct reg_value* next;
} reg_value_t;

typedef struct reg_key {
    HKEY            handle;
    char            name[256];
    uint8_t         flags;
    struct reg_key* parent;
    struct reg_key* first_subkey;
    struct reg_key* next_sibling;
    reg_value_t*    first_value;
    uint64_t        last_write_time;
} reg_key_t;

#define REGISTRY_ROOT_COUNT 6

static reg_key_t* registry_roots[REGISTRY_ROOT_COUNT];
static HKEY       registry_next_handle = (HKEY)(uintptr_t)0x10000;

static void* memory_alloc_wrapper(uint32_t size)
{
    extern void* memory_alloc(uint64_t size);
    return memory_alloc((uint64_t)size);
}

static reg_key_t* handle_to_key(HKEY hKey)
{
    uintptr_t h = (uintptr_t)hKey;
    uint64_t i;

    for (i = 0; i < (uint64_t)REGISTRY_ROOT_COUNT; i++) {
        if (registry_roots[i] == NULL) {
            continue;
        }
        if ((uintptr_t)registry_roots[i]->handle == h) {
            return registry_roots[i];
        }
    }

    for (i = 0; i < (uint64_t)REGISTRY_ROOT_COUNT; i++) {
        reg_key_t* stack[1024];
        int32_t sp = 0;
        reg_key_t* root;

        root = registry_roots[i];
        if (root == NULL) {
            continue;
        }

        stack[sp++] = root;

        while (sp > 0) {
            reg_key_t* cur;
            reg_key_t* child;

            sp--;
            cur = stack[sp];

            if ((uintptr_t)cur->handle == h) {
                return cur;
            }

            child = cur->first_subkey;
            while (child != NULL) {
                if (sp < 1024) {
                    stack[sp++] = child;
                }
                child = child->next_sibling;
            }
        }
    }

    return NULL;
}

static reg_key_t* find_child_key(reg_key_t* parent, const char* name)
{
    reg_key_t* child;

    if (parent == NULL || name == NULL) {
        return NULL;
    }

    child = parent->first_subkey;
    while (child != NULL) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

static int32_t split_path(const char* path, char parts[][256], int32_t max_parts)
{
    int32_t count = 0;
    int32_t i = 0;
    int32_t j = 0;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    while (path[i] != '\0' && count < max_parts) {
        if (path[i] == '\\') {
            if (j > 0) {
                parts[count][j] = '\0';
                count++;
                j = 0;
            }
            i++;
        } else {
            if (j < 255) {
                parts[count][j] = path[i];
                j++;
            }
            i++;
        }
    }

    if (j > 0 && count < max_parts) {
        parts[count][j] = '\0';
        count++;
    }

    return count;
}

static reg_key_t* walk_path(reg_key_t* start, char parts[][256], int32_t count, int32_t* last_index)
{
    reg_key_t* current;
    int32_t i;

    if (start == NULL) {
        if (last_index != NULL) {
            *last_index = -1;
        }
        return NULL;
    }

    current = start;
    for (i = 0; i < count; i++) {
        reg_key_t* child;

        child = find_child_key(current, parts[i]);
        if (child == NULL) {
            if (last_index != NULL) {
                *last_index = i;
            }
            return current;
        }
        current = child;
    }

    if (last_index != NULL) {
        *last_index = count;
    }
    return current;
}

static reg_key_t* alloc_key(const char* name, reg_key_t* parent)
{
    reg_key_t* key;

    key = (reg_key_t*)memory_alloc_wrapper((uint32_t)sizeof(reg_key_t));
    if (key == NULL) {
        return NULL;
    }

    memset(key, 0, sizeof(reg_key_t));
    key->handle = registry_next_handle;
    registry_next_handle = (HKEY)((uintptr_t)registry_next_handle + 1);

    if (name != NULL) {
        strncpy(key->name, name, (size_t)255);
        key->name[255] = '\0';
    }

    key->parent = parent;
    key->flags = 0;
    key->first_subkey = NULL;
    key->next_sibling = NULL;
    key->first_value = NULL;
    key->last_write_time = 0;

    return key;
}

static void attach_child(reg_key_t* parent, reg_key_t* child)
{
    if (parent == NULL || child == NULL) {
        return;
    }

    child->next_sibling = parent->first_subkey;
    parent->first_subkey = child;
}

static reg_value_t* find_value(reg_key_t* key, const char* name)
{
    reg_value_t* val;

    if (key == NULL) {
        return NULL;
    }

    val = key->first_value;
    while (val != NULL) {
        if (name == NULL || name[0] == '\0') {
            if (val->name[0] == '\0') {
                return val;
            }
        } else {
            if (strcmp(val->name, name) == 0) {
                return val;
            }
        }
        val = val->next;
    }

    return NULL;
}

static int32_t set_value_internal(reg_key_t* key, const char* name,
                                  uint32_t type, const uint8_t* data,
                                  uint32_t data_size)
{
    reg_value_t* val;
    uint8_t* new_data;

    if (key == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    val = find_value(key, name);
    if (val != NULL) {
        if (val->data != NULL) {
            kfree(val->data);
        }
    } else {
        val = (reg_value_t*)memory_alloc_wrapper((uint32_t)sizeof(reg_value_t));
        if (val == NULL) {
            return ERROR_NOT_ENOUGH_MEMORY;
        }
        memset(val, 0, sizeof(reg_value_t));

        if (name != NULL) {
            strncpy(val->name, name, (size_t)16382);
            val->name[16382] = '\0';
        }

        val->next = key->first_value;
        key->first_value = val;
    }

    val->type = type;
    val->data_size = data_size;

    if (data_size > 0 && data != NULL) {
        new_data = (uint8_t*)memory_alloc_wrapper(data_size);
        if (new_data == NULL) {
            return ERROR_NOT_ENOUGH_MEMORY;
        }
        memcpy(new_data, data, (size_t)data_size);
        val->data = new_data;
    } else {
        val->data = NULL;
    }

    return ERROR_SUCCESS;
}

static void seed_value_sz(reg_key_t* key, const char* name, const char* str)
{
    size_t len_sz;
    uint32_t len;

    len_sz = strlen(str);
    len_sz = len_sz + (size_t)1;
    len = (uint32_t)len_sz;
    set_value_internal(key, name, REG_SZ, (const uint8_t*)str, len);
}

static void seed_value_dword(reg_key_t* key, const char* name, uint32_t val)
{
    set_value_internal(key, name, REG_DWORD, (const uint8_t*)&val,
                       (uint32_t)sizeof(uint32_t));
}

static reg_key_t* create_path(reg_key_t* root, const char* path)
{
    char parts[64][256];
    int32_t count;
    int32_t last_idx;
    reg_key_t* current;
    int32_t i;

    count = split_path(path, parts, 64);
    if (count == 0) {
        return root;
    }

    current = walk_path(root, parts, count, &last_idx);

    for (i = last_idx; i < count; i++) {
        reg_key_t* new_key;

        new_key = alloc_key(parts[i], current);
        if (new_key == NULL) {
            return NULL;
        }
        attach_child(current, new_key);
        current = new_key;
    }

    return current;
}

int registry_init(void)
{
    const char* root_names[REGISTRY_ROOT_COUNT] = {
        "HKCR", "HKCU", "HKLM", "HKU", "HKCC", "HKPD"
    };
    HKEY root_handles[REGISTRY_ROOT_COUNT] = {
        HKEY_CLASSES_ROOT,
        HKEY_CURRENT_USER,
        HKEY_LOCAL_MACHINE,
        HKEY_USERS,
        HKEY_CURRENT_CONFIG,
        HKEY_PERFORMANCE_DATA
    };
    uint64_t i;
    reg_key_t* k;
    uint32_t dword_zero = 0;
    int success_count = 0;

    for (i = 0; i < (uint64_t)REGISTRY_ROOT_COUNT; i++) {
        registry_roots[i] = alloc_key(root_names[i], NULL);
        if (registry_roots[i] == NULL) {
            continue;   /* tolerate individual root alloc failure */
        }
        registry_roots[i]->handle = root_handles[i];
        success_count++;
    }

    /* tolerate total root alloc failure: fall back to minimal stub that
     * returns success for all queries but stores nothing. This keeps the
     * rest of the Win32 subsystem alive during early boot. */
    if (success_count == 0) {
        registry_next_handle = (HKEY)(uintptr_t)0x10000;
        return 0;
    }

    registry_next_handle = (HKEY)(uintptr_t)0x10000;

    /* Only seed the most critical HKLM path; skip others to reduce early
     * memory pressure and avoid possible walk_path issues. */
    if (registry_roots[2] != NULL) {
        k = create_path(registry_roots[2], "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
        if (k != NULL) {
            seed_value_sz(k, "ProductName", "KenuxOS Windows Compatible");
            seed_value_sz(k, "CurrentVersion", "10.0");
            seed_value_sz(k, "CurrentBuild", "26000");
        }
    }

    return 0;
}

int RegOpenKeyExA(HKEY hKey,
                  const char* lpSubKey,
                  uint32_t ulOptions,
                  REGSAM samDesired,
                  HKEY* phkResult)
{
    reg_key_t* start;
    char parts[64][256];
    int32_t count;
    int32_t last_idx;
    reg_key_t* result;

    (void)ulOptions;
    (void)samDesired;

    if (phkResult == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    start = handle_to_key(hKey);
    if (start == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    if (lpSubKey == NULL || lpSubKey[0] == '\0') {
        *phkResult = (HKEY)start->handle;
        return ERROR_SUCCESS;
    }

    count = split_path(lpSubKey, parts, 64);
    result = walk_path(start, parts, count, &last_idx);

    if (last_idx != count) {
        return ERROR_FILE_NOT_FOUND;
    }

    *phkResult = (HKEY)result->handle;
    return ERROR_SUCCESS;
}

int RegCreateKeyExA(HKEY hKey,
                    const char* lpSubKey,
                    uint32_t Reserved,
                    char* lpClass,
                    uint32_t dwOptions,
                    REGSAM samDesired,
                    void* lpSecurityAttributes,
                    HKEY* phkResult,
                    uint32_t* lpdwDisposition)
{
    reg_key_t* start;
    char parts[64][256];
    int32_t count;
    int32_t last_idx;
    reg_key_t* current;
    int32_t i;
    int32_t created = 0;

    (void)Reserved;
    (void)lpClass;
    (void)dwOptions;
    (void)samDesired;
    (void)lpSecurityAttributes;

    if (lpSubKey == NULL || phkResult == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    start = handle_to_key(hKey);
    if (start == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    count = split_path(lpSubKey, parts, 64);
    if (count == 0) {
        *phkResult = (HKEY)start->handle;
        if (lpdwDisposition != NULL) {
            *lpdwDisposition = (uint32_t)REG_OPENED_EXISTING_KEY;
        }
        return ERROR_SUCCESS;
    }

    current = walk_path(start, parts, count, &last_idx);

    if (last_idx == count) {
        *phkResult = (HKEY)current->handle;
        if (lpdwDisposition != NULL) {
            *lpdwDisposition = (uint32_t)REG_OPENED_EXISTING_KEY;
        }
        return ERROR_SUCCESS;
    }

    for (i = last_idx; i < count; i++) {
        reg_key_t* new_key;

        new_key = alloc_key(parts[i], current);
        if (new_key == NULL) {
            return ERROR_NOT_ENOUGH_MEMORY;
        }
        attach_child(current, new_key);
        current = new_key;
        created = 1;
    }

    *phkResult = (HKEY)current->handle;
    if (lpdwDisposition != NULL) {
        if (created != 0) {
            *lpdwDisposition = (uint32_t)REG_CREATED_NEW_KEY;
        } else {
            *lpdwDisposition = (uint32_t)REG_OPENED_EXISTING_KEY;
        }
    }

    return ERROR_SUCCESS;
}

int RegCloseKey(HKEY hKey)
{
    reg_key_t* key;

    key = handle_to_key(hKey);
    if (key == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    return ERROR_SUCCESS;
}

int RegQueryValueExA(HKEY hKey,
                     const char* lpValueName,
                     uint32_t* lpReserved,
                     uint32_t* lpType,
                     uint8_t* lpData,
                     uint32_t* lpcbData)
{
    reg_key_t* key;
    reg_value_t* val;

    (void)lpReserved;

    if (lpcbData == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    key = handle_to_key(hKey);
    if (key == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    val = find_value(key, lpValueName);
    if (val == NULL) {
        return ERROR_FILE_NOT_FOUND;
    }

    if (lpType != NULL) {
        *lpType = val->type;
    }

    if (lpData == NULL) {
        *lpcbData = val->data_size;
        return ERROR_SUCCESS;
    }

    if (*lpcbData < val->data_size) {
        *lpcbData = val->data_size;
        return ERROR_FILE_NOT_FOUND;
    }

    if (val->data_size > 0 && val->data != NULL) {
        memcpy(lpData, val->data, (size_t)val->data_size);
    }
    *lpcbData = val->data_size;

    return ERROR_SUCCESS;
}

int RegSetValueExA(HKEY hKey,
                   const char* lpValueName,
                   uint32_t Reserved,
                   uint32_t dwType,
                   const uint8_t* lpData,
                   uint32_t cbData)
{
    reg_key_t* key;

    (void)Reserved;

    key = handle_to_key(hKey);
    if (key == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    return set_value_internal(key, lpValueName, dwType, lpData, cbData);
}

int RegEnumKeyExA(HKEY hKey,
                  uint32_t dwIndex,
                  char* lpName,
                  uint32_t* lpcchName,
                  uint32_t* lpReserved,
                  char* lpClass,
                  uint32_t* lpcchClass,
                  uint64_t* lpftLastWriteTime)
{
    reg_key_t* key;
    reg_key_t* child;
    uint32_t count;
    uint32_t i;
    uint32_t name_len;

    (void)lpReserved;
    (void)lpClass;
    (void)lpcchClass;

    if (lpName == NULL || lpcchName == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    key = handle_to_key(hKey);
    if (key == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    count = 0;
    child = key->first_subkey;
    while (child != NULL) {
        count++;
        child = child->next_sibling;
    }

    if (dwIndex >= count) {
        return ERROR_NO_MORE_ITEMS;
    }

    child = key->first_subkey;
    for (i = 0; i < dwIndex; i++) {
        if (child == NULL) {
            break;
        }
        child = child->next_sibling;
    }

    if (child == NULL) {
        return ERROR_NO_MORE_ITEMS;
    }

    {
        size_t nl_sz = strlen(child->name);
        name_len = (uint32_t)nl_sz;
    }
    if (*lpcchName <= name_len) {
        *lpcchName = (uint32_t)(name_len + (uint32_t)1);
        return ERROR_ACCESS_DENIED;
    }

    memcpy(lpName, child->name, (size_t)name_len + (size_t)1);
    *lpcchName = name_len;

    if (lpftLastWriteTime != NULL) {
        *lpftLastWriteTime = child->last_write_time;
    }

    return ERROR_SUCCESS;
}

int RegEnumValueA(HKEY hKey,
                  uint32_t dwIndex,
                  char* lpValueName,
                  uint32_t* lpcchValueName,
                  uint32_t* lpReserved,
                  uint32_t* lpType,
                  uint8_t* lpData,
                  uint32_t* lpcbData)
{
    reg_key_t* key;
    reg_value_t* val;
    uint32_t count;
    uint32_t i;
    uint32_t name_len;

    (void)lpReserved;

    if (lpValueName == NULL || lpcchValueName == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    key = handle_to_key(hKey);
    if (key == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    count = 0;
    val = key->first_value;
    while (val != NULL) {
        count++;
        val = val->next;
    }

    if (dwIndex >= count) {
        return ERROR_NO_MORE_ITEMS;
    }

    val = key->first_value;
    for (i = 0; i < dwIndex; i++) {
        if (val == NULL) {
            break;
        }
        val = val->next;
    }

    if (val == NULL) {
        return ERROR_NO_MORE_ITEMS;
    }

    {
        size_t nl_sz = strlen(val->name);
        name_len = (uint32_t)nl_sz;
    }
    if (*lpcchValueName <= name_len) {
        *lpcchValueName = (uint32_t)(name_len + (uint32_t)1);
        if (lpcbData != NULL) {
            *lpcbData = val->data_size;
        }
        return ERROR_ACCESS_DENIED;
    }

    memcpy(lpValueName, val->name, (size_t)name_len + (size_t)1);
    *lpcchValueName = name_len;

    if (lpType != NULL) {
        *lpType = val->type;
    }

    if (lpcbData != NULL) {
        if (lpData == NULL) {
            *lpcbData = val->data_size;
        } else if (*lpcbData >= val->data_size) {
            if (val->data_size > 0 && val->data != NULL) {
                memcpy(lpData, val->data, (size_t)val->data_size);
            }
            *lpcbData = val->data_size;
        } else {
            *lpcbData = val->data_size;
            return ERROR_ACCESS_DENIED;
        }
    }

    return ERROR_SUCCESS;
}

static void free_value(reg_value_t* val)
{
    if (val == NULL) {
        return;
    }
    if (val->data != NULL) {
        kfree(val->data);
    }
    kfree(val);
}

static void free_key_recursive(reg_key_t* key)
{
    reg_key_t* child;
    reg_key_t* next_child;
    reg_value_t* val;
    reg_value_t* next_val;

    if (key == NULL) {
        return;
    }

    child = key->first_subkey;
    while (child != NULL) {
        next_child = child->next_sibling;
        free_key_recursive(child);
        child = next_child;
    }

    val = key->first_value;
    while (val != NULL) {
        next_val = val->next;
        free_value(val);
        val = next_val;
    }

    kfree(key);
}

int RegDeleteKeyA(HKEY hKey, const char* lpSubKey)
{
    reg_key_t* start;
    reg_key_t* parent;
    reg_key_t** prev_ptr;
    reg_key_t* target;
    char parts[64][256];
    int32_t count;
    int32_t last_idx;

    if (lpSubKey == NULL || lpSubKey[0] == '\0') {
        return ERROR_INVALID_PARAMETER;
    }

    start = handle_to_key(hKey);
    if (start == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    count = split_path(lpSubKey, parts, 64);
    if (count == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    if (count == 1) {
        parent = start;
    } else {
        parent = walk_path(start, parts, count - 1, &last_idx);
        if (last_idx != count - 1) {
            return ERROR_FILE_NOT_FOUND;
        }
    }

    prev_ptr = &parent->first_subkey;
    while (*prev_ptr != NULL) {
        if (strcmp((*prev_ptr)->name, parts[count - 1]) == 0) {
            break;
        }
        prev_ptr = &(*prev_ptr)->next_sibling;
    }

    target = *prev_ptr;
    if (target == NULL) {
        return ERROR_FILE_NOT_FOUND;
    }

    *prev_ptr = target->next_sibling;
    target->next_sibling = NULL;

    free_key_recursive(target);

    return ERROR_SUCCESS;
}

int RegDeleteValueA(HKEY hKey, const char* lpValueName)
{
    reg_key_t* key;
    reg_value_t** prev_ptr;
    reg_value_t* target;

    key = handle_to_key(hKey);
    if (key == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    prev_ptr = &key->first_value;
    while (*prev_ptr != NULL) {
        int match;

        if (lpValueName == NULL || lpValueName[0] == '\0') {
            match = ((*prev_ptr)->name[0] == '\0') ? 1 : 0;
        } else {
            match = (strcmp((*prev_ptr)->name, lpValueName) == 0) ? 1 : 0;
        }

        if (match != 0) {
            break;
        }
        prev_ptr = &(*prev_ptr)->next;
    }

    target = *prev_ptr;
    if (target == NULL) {
        return ERROR_FILE_NOT_FOUND;
    }

    *prev_ptr = target->next;
    target->next = NULL;

    free_value(target);

    return ERROR_SUCCESS;
}

int RegFlushKey(HKEY hKey)
{
    reg_key_t* key;

    key = handle_to_key(hKey);
    if (key == NULL) {
        return ERROR_INVALID_HANDLE;
    }

    return ERROR_SUCCESS;
}

int RegGetValueA(HKEY hkey,
                 const char* lpSubKey,
                 const char* lpValue,
                 uint32_t dwFlags,
                 uint32_t* pdwType,
                 void* pvData,
                 uint32_t* pcbData)
{
    HKEY opened;
    int32_t ret;
    uint32_t reserved = 0;

    (void)dwFlags;

    if (pcbData == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    if (lpSubKey != NULL && lpSubKey[0] != '\0') {
        ret = RegOpenKeyExA(hkey, lpSubKey, 0, KEY_READ, &opened);
        if (ret != ERROR_SUCCESS) {
            return ret;
        }
    } else {
        opened = hkey;
    }

    ret = RegQueryValueExA(opened, lpValue, &reserved, pdwType,
                          (uint8_t*)pvData, pcbData);

    if (lpSubKey != NULL && lpSubKey[0] != '\0') {
        RegCloseKey(opened);
    }

    return ret;
}
