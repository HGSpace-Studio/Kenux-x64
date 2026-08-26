#include <kapi_leonos.h>
#include <kapi.h>
#include <arch/fs.h>
#include <string.h>

typedef struct {
    char name[KAPI_INI_NAME_LEN];
    char value[KAPI_INI_VALUE_LEN];
} kapi_ini_key_t;

typedef struct {
    char name[KAPI_INI_NAME_LEN];
    uint32_t key_count;
    kapi_ini_key_t keys[KAPI_INI_MAX_KEYS_PER_SECTION];
} kapi_ini_section_t;

typedef struct {
    uint32_t section_count;
    kapi_ini_section_t sections[KAPI_INI_MAX_SECTIONS];
    int loaded;
} kapi_ini_state_t;

static kapi_ini_state_t s_ini;
static char s_ini_buffer[KAPI_INI_MAX_SIZE];
static char s_tar_buffer[KAPI_INI_MAX_SIZE];

static uint32_t kapi_len(const char* s) {
    uint32_t len = 0;
    if (!s) return 0;
    while (s[len]) len++;
    return len;
}

static int kapi_eq(const char* a, const char* b) {
    uint32_t i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == 0 && b[i] == 0;
}

static void kapi_copy(char* dst, uint32_t cap, const char* src) {
    uint32_t i = 0;
    if (!dst || cap == 0) return;
    while (src && src[i] && i + 1u < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void kapi_trim(char* str) {
    if (!str || !str[0]) return;
    uint32_t len = kapi_len(str);
    uint32_t start = 0;
    while (start < len && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r')) start++;
    uint32_t end = len;
    while (end > start && (str[end - 1u] == ' ' || str[end - 1u] == '\t' || str[end - 1u] == '\r')) end--;
    if (start > 0 || end < len) {
        uint32_t i = 0;
        while (start < end) str[i++] = str[start++];
        str[i] = 0;
    }
}

static int kapi_read_small_file(const char* path, char* out, uint32_t capacity, uint32_t* out_size) {
    if (!path || !out || capacity == 0) return KAPI_EINVAL;
    int fd = vfs_open(path, FS_O_RDONLY, 0);
    if (fd < 0) return KAPI_ENOENT;
    uint32_t total = 0;
    while (total + 1u < capacity) {
        int got = vfs_read(fd, out + total, capacity - total - 1u);
        if (got < 0) { vfs_close(fd); return KAPI_ERROR; }
        if (got == 0) break;
        total += (uint32_t)got;
    }
    vfs_close(fd);
    out[total] = 0;
    if (out_size) *out_size = total;
    return KAPI_OK;
}

static int kapi_ini_buffer_is_text(const char* buffer, long len, uint32_t strict) {
    if (!buffer || len < 0) return 0;
    for (long i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)buffer[i];
        if (ch == 0) return 0;
        if (strict && ch < 0x20u && ch != '\n' && ch != '\r' && ch != '\t') return 0;
    }
    return 1;
}

static int kapi_ini_parse_section(const char* line, char* name, uint32_t cap, uint32_t strict) {
    if (!line || line[0] != '[' || !name || cap == 0) return 0;
    const char* end = line;
    while (*end && *end != ']') end++;
    if (*end != ']' || end <= line + 1) return 0;
    if (strict && end[1] != 0) return 0;
    uint32_t len = (uint32_t)(end - line - 1);
    if (len >= cap) { if (strict) return 0; len = cap - 1u; }
    memcpy(name, line + 1, len);
    name[len] = 0;
    kapi_trim(name);
    return name[0] ? 1 : 0;
}

static int kapi_ini_parse_key_value(const char* line, char* key, uint32_t key_cap,
                                    char* value, uint32_t value_cap, uint32_t strict) {
    if (!line || !key || !value) return 0;
    const char* eq = line;
    while (*eq && *eq != '=') eq++;
    if (*eq != '=' || eq == line) return 0;
    uint32_t key_len = (uint32_t)(eq - line);
    if (key_len >= key_cap) { if (strict) return 0; key_len = key_cap - 1u; }
    memcpy(key, line, key_len);
    key[key_len] = 0;
    kapi_trim(key);
    if (!key[0]) return 0;
    uint32_t val_len = kapi_len(eq + 1);
    if (val_len >= value_cap) { if (strict) return 0; val_len = value_cap - 1u; }
    memcpy(value, eq + 1, val_len);
    value[val_len] = 0;
    kapi_trim(value);
    return 1;
}

static kapi_ini_section_t* kapi_ini_find_section(const char* name) {
    if (!name || !s_ini.loaded) return 0;
    for (uint32_t i = 0; i < s_ini.section_count; i++) {
        if (kapi_eq(s_ini.sections[i].name, name)) return &s_ini.sections[i];
    }
    return 0;
}

static kapi_ini_key_t* kapi_ini_find_key(kapi_ini_section_t* section, const char* name) {
    if (!section || !name) return 0;
    for (uint32_t i = 0; i < section->key_count; i++) {
        if (kapi_eq(section->keys[i].name, name)) return &section->keys[i];
    }
    return 0;
}

static int kapi_ini_load_mode(const char* path, uint32_t strict) {
    uint32_t got = 0;
    memset(&s_ini, 0, sizeof(s_ini));
    int rc = kapi_read_small_file(path, s_ini_buffer, sizeof(s_ini_buffer), &got);
    if (rc != KAPI_OK) return rc;
    if (strict && got >= sizeof(s_ini_buffer) - 1u) return KAPI_ERROR;
    if (!kapi_ini_buffer_is_text(s_ini_buffer, (long)got, strict)) return KAPI_ERROR;
    const char* p = s_ini_buffer;
    const char* end = s_ini_buffer + got;
    kapi_ini_section_t* current = 0;
    while (p < end) {
        const char* lf = p;
        while (lf < end && *lf != '\n') lf++;
        uint32_t line_len = (uint32_t)(lf - p);
        char line[KAPI_INI_VALUE_LEN];
        if (line_len >= sizeof(line)) { if (strict) return KAPI_ERROR; line_len = sizeof(line) - 1u; }
        memcpy(line, p, line_len);
        line[line_len] = 0;
        kapi_trim(line);
        if (line[0] && line[0] != ';' && line[0] != '#') {
            if (line[0] == '[') {
                current = 0;
                if (s_ini.section_count < KAPI_INI_MAX_SECTIONS) {
                    char name[KAPI_INI_NAME_LEN];
                    if (kapi_ini_parse_section(line, name, sizeof(name), strict)) {
                        current = &s_ini.sections[s_ini.section_count++];
                        kapi_copy(current->name, sizeof(current->name), name);
                    } else if (strict) return KAPI_ERROR;
                } else if (strict) return KAPI_ERROR;
            } else if (current) {
                if (current->key_count < KAPI_INI_MAX_KEYS_PER_SECTION) {
                    kapi_ini_key_t* key = &current->keys[current->key_count];
                    if (kapi_ini_parse_key_value(line, key->name, sizeof(key->name), key->value, sizeof(key->value), strict)) {
                        current->key_count++;
                    } else if (strict) return KAPI_ERROR;
                } else if (strict) return KAPI_ERROR;
            } else if (strict) return KAPI_ERROR;
        }
        p = lf < end ? lf + 1 : end;
    }
    s_ini.loaded = 1;
    return KAPI_OK;
}

int KAPI_INI_Load(const char* path) { return kapi_ini_load_mode(path, 0); }
int KAPI_INI_LoadStrict(const char* path) { return kapi_ini_load_mode(path, 1); }
int KAPI_INI_Get(const char* section, const char* key, char* value, uint32_t capacity) {
    kapi_ini_section_t* sec = kapi_ini_find_section(section);
    kapi_ini_key_t* ent = kapi_ini_find_key(sec, key);
    if (!ent || !value || capacity == 0) return KAPI_ENOENT;
    kapi_copy(value, capacity, ent->value);
    return KAPI_OK;
}
int KAPI_INI_SectionCount(void) { return s_ini.loaded ? (int)s_ini.section_count : 0; }
int KAPI_INI_SectionName(uint32_t index, char* name, uint32_t capacity) {
    if (!s_ini.loaded || index >= s_ini.section_count || !name || capacity == 0) return KAPI_EINVAL;
    kapi_copy(name, capacity, s_ini.sections[index].name);
    return KAPI_OK;
}
int KAPI_INI_KeyCount(const char* section) {
    kapi_ini_section_t* sec = kapi_ini_find_section(section);
    return sec ? (int)sec->key_count : 0;
}
int KAPI_INI_KeyName(const char* section, uint32_t index, char* name, uint32_t capacity) {
    kapi_ini_section_t* sec = kapi_ini_find_section(section);
    if (!sec || index >= sec->key_count || !name || capacity == 0) return KAPI_EINVAL;
    kapi_copy(name, capacity, sec->keys[index].name);
    return KAPI_OK;
}

int KAPI_API_ParseInfo(const char* api_path, KAPI_API_INFO* info) {
    if (!api_path || !info) return KAPI_EINVAL;
    memset(info, 0, sizeof(*info));
    int rc = KAPI_INI_Load(api_path);
    if (rc != KAPI_OK) return rc;
    KAPI_INI_Get("App", "Name", info->name, sizeof(info->name));
    KAPI_INI_Get("App", "Version", info->version, sizeof(info->version));
    KAPI_INI_Get("App", "Main", info->main_exe, sizeof(info->main_exe));
    KAPI_INI_Get("App", "Path", info->default_path, sizeof(info->default_path));
    KAPI_INI_Get("App", "Icon", info->icon, sizeof(info->icon));
    KAPI_INI_Get("InputMethod", "Id", info->input_method_id, sizeof(info->input_method_id));
    KAPI_INI_Get("InputMethod", "Abbrev", info->input_method_abbreviation, sizeof(info->input_method_abbreviation));
    return info->name[0] ? KAPI_OK : KAPI_ERROR;
}
int KAPI_API_ExtractFiles(const char* api_path, const char* dest_dir) { return KAPI_TAR_ExtractAll(api_path, dest_dir); }
int KAPI_API_Install(const char* api_path, const char* dest_dir, uint32_t create_shortcut) {
    return KAPI_API_InstallWithProgress(api_path, dest_dir, create_shortcut, 0, 0);
}
int KAPI_API_InstallWithProgress(const char* api_path, const char* dest_dir, uint32_t create_shortcut,
                                 KAPI_API_PROGRESS_FN progress, void* context) {
    (void)create_shortcut;
    if (progress) progress(0, 1, context);
    int rc = KAPI_API_ExtractFiles(api_path, dest_dir);
    if (progress) progress(1, 1, context);
    return rc;
}

static int tar_header_zero(const unsigned char* hdr) {
    for (uint32_t i = 0; i < KAPI_TAR_BLOCK_SIZE; i++) if (hdr[i]) return 0;
    return 1;
}
static uint32_t tar_octal(const char* s, uint32_t len) {
    uint32_t value = 0;
    for (uint32_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == 0 || ch == ' ') break;
        if (ch < '0' || ch > '7') return 0;
        value = (value << 3) | (uint32_t)(ch - '0');
    }
    return value;
}
static void tar_append(char* out, uint32_t cap, uint32_t* pos, const char* text) {
    while (out && text && *text && *pos + 1u < cap) out[(*pos)++] = *text++;
    if (out && cap) out[*pos < cap ? *pos : cap - 1u] = 0;
}

int KAPI_TAR_List(const char* tar_path, char* output, uint32_t capacity) {
    uint32_t size = 0;
    int rc = kapi_read_small_file(tar_path, s_tar_buffer, sizeof(s_tar_buffer), &size);
    if (rc != KAPI_OK) return rc;
    if (!output || capacity == 0) return KAPI_EINVAL;
    output[0] = 0;
    uint32_t pos = 0;
    for (uint32_t off = 0; off + KAPI_TAR_BLOCK_SIZE <= size;) {
        const unsigned char* hdr = (const unsigned char*)s_tar_buffer + off;
        if (tar_header_zero(hdr)) break;
        char name[KAPI_TAR_NAME_LEN + 1];
        memcpy(name, hdr, KAPI_TAR_NAME_LEN);
        name[KAPI_TAR_NAME_LEN] = 0;
        uint32_t file_size = tar_octal((const char*)hdr + 124, 12);
        tar_append(output, capacity, &pos, name);
        tar_append(output, capacity, &pos, "\n");
        off += KAPI_TAR_BLOCK_SIZE + ((file_size + KAPI_TAR_BLOCK_SIZE - 1u) & ~(KAPI_TAR_BLOCK_SIZE - 1u));
    }
    return KAPI_OK;
}

int KAPI_TAR_ExtractFile(const char* tar_path, const char* stored_name, const char* dest_path) {
    if (!tar_path || !stored_name || !dest_path) return KAPI_EINVAL;
    uint32_t size = 0;
    int rc = kapi_read_small_file(tar_path, s_tar_buffer, sizeof(s_tar_buffer), &size);
    if (rc != KAPI_OK) return rc;
    for (uint32_t off = 0; off + KAPI_TAR_BLOCK_SIZE <= size;) {
        const unsigned char* hdr = (const unsigned char*)s_tar_buffer + off;
        if (tar_header_zero(hdr)) break;
        char name[KAPI_TAR_NAME_LEN + 1];
        memcpy(name, hdr, KAPI_TAR_NAME_LEN);
        name[KAPI_TAR_NAME_LEN] = 0;
        uint32_t file_size = tar_octal((const char*)hdr + 124, 12);
        uint32_t data_off = off + KAPI_TAR_BLOCK_SIZE;
        if (kapi_eq(name, stored_name)) {
            if (data_off + file_size > size) return KAPI_ERROR;
            int fd = vfs_open(dest_path, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, 0644);
            if (fd < 0) return KAPI_ERROR;
            uint32_t written = 0;
            while (written < file_size) {
                uint32_t chunk = file_size - written;
                if (chunk > 4096u) chunk = 4096u;
                int got = vfs_write(fd, s_tar_buffer + data_off + written, chunk);
                if (got <= 0) { vfs_close(fd); return KAPI_ERROR; }
                written += (uint32_t)got;
            }
            vfs_close(fd);
            return KAPI_OK;
        }
        off += KAPI_TAR_BLOCK_SIZE + ((file_size + KAPI_TAR_BLOCK_SIZE - 1u) & ~(KAPI_TAR_BLOCK_SIZE - 1u));
    }
    return KAPI_ENOENT;
}

int KAPI_TAR_ExtractAll(const char* tar_path, const char* dest_dir) {
    if (!tar_path || !dest_dir) return KAPI_EINVAL;
    uint32_t size = 0;
    int rc = kapi_read_small_file(tar_path, s_tar_buffer, sizeof(s_tar_buffer), &size);
    if (rc != KAPI_OK) return rc;
    for (uint32_t off = 0; off + KAPI_TAR_BLOCK_SIZE <= size;) {
        const unsigned char* hdr = (const unsigned char*)s_tar_buffer + off;
        if (tar_header_zero(hdr)) break;
        char name[KAPI_TAR_NAME_LEN + 1];
        memcpy(name, hdr, KAPI_TAR_NAME_LEN);
        name[KAPI_TAR_NAME_LEN] = 0;
        uint32_t file_size = tar_octal((const char*)hdr + 124, 12);
        char type = (char)hdr[156];
        if (name[0] && type != KAPI_TAR_TYPE_DIR) {
            char out_path[KAPI_FS_PATH_LEN];
            uint32_t pos = 0;
            while (dest_dir[pos] && pos + 1u < sizeof(out_path)) out_path[pos++] = dest_dir[pos];
            if (pos > 0 && out_path[pos - 1u] != '/' && pos + 1u < sizeof(out_path)) out_path[pos++] = '/';
            for (uint32_t i = 0; name[i] && pos + 1u < sizeof(out_path); i++) out_path[pos++] = name[i];
            out_path[pos] = 0;
            KAPI_TAR_ExtractFile(tar_path, name, out_path);
        }
        off += KAPI_TAR_BLOCK_SIZE + ((file_size + KAPI_TAR_BLOCK_SIZE - 1u) & ~(KAPI_TAR_BLOCK_SIZE - 1u));
    }
    return KAPI_OK;
}

int KAPI_TAR_Create(const char* tar_path) {
    if (!tar_path) return KAPI_EINVAL;
    int fd = vfs_open(tar_path, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, 0644);
    if (fd < 0) return KAPI_ERROR;
    char zero[KAPI_TAR_BLOCK_SIZE * 2U];
    memset(zero, 0, sizeof(zero));
    int rc = vfs_write(fd, zero, sizeof(zero));
    vfs_close(fd);
    return rc == (int)sizeof(zero) ? KAPI_OK : KAPI_ERROR;
}
int KAPI_TAR_PackFile(const char* tar_path, const char* file_path, const char* stored_name) {
    (void)tar_path; (void)file_path; (void)stored_name; return KAPI_ENOSYS;
}
int KAPI_TAR_PackFileAppend(int tar_fd, const char* file_path, const char* stored_name) {
    (void)tar_fd; (void)file_path; (void)stored_name; return KAPI_ENOSYS;
}
int KAPI_TAR_Finalize(int tar_fd) {
    char zero[KAPI_TAR_BLOCK_SIZE * 2U];
    memset(zero, 0, sizeof(zero));
    int rc = vfs_write(tar_fd, zero, sizeof(zero));
    return rc == (int)sizeof(zero) ? KAPI_OK : KAPI_ERROR;
}
int KAPI_TAR_PackDir(const char* tar_path, const char* dir_path) { (void)tar_path; (void)dir_path; return KAPI_ENOSYS; }
int KAPI_TAR_PackDirAppend(int tar_fd, const char* dir_path) { (void)tar_fd; (void)dir_path; return KAPI_ENOSYS; }
int KAPI_TAR_ExtractAllWithProgress(const char* tar_path, const char* dest_dir, KAPI_TAR_PROGRESS_FN progress, void* context) {
    if (progress) progress(0, 1, context);
    int rc = KAPI_TAR_ExtractAll(tar_path, dest_dir);
    if (progress) progress(1, 1, context);
    return rc;
}

int KAPI_Device_List(KAPI_DEVICE_INFO* devices, uint32_t capacity, uint32_t* out_count) {
    if (!devices || capacity == 0) return KAPI_EINVAL;
    memset(devices, 0, sizeof(*devices) * capacity);
    uint32_t count = 0;
    devices[count].id = 1;
    devices[count].device_class = KAPI_DEVICE_CLASS_SYSTEM;
    devices[count].flags = KAPI_DEVICE_FLAG_PRESENT | KAPI_DEVICE_FLAG_ACTIVE | KAPI_DEVICE_FLAG_BOOT;
    kapi_copy(devices[count].name, sizeof(devices[count].name), "Kenux Kernel");
    kapi_copy(devices[count].status, sizeof(devices[count].status), "Active");
    kapi_copy(devices[count].detail, sizeof(devices[count].detail), "KAPI compatibility surface");
    count++;
    if (capacity > count) {
        devices[count].id = 2;
        devices[count].device_class = KAPI_DEVICE_CLASS_DISPLAY;
        devices[count].flags = KAPI_DEVICE_FLAG_PRESENT | KAPI_DEVICE_FLAG_ACTIVE;
        kapi_copy(devices[count].name, sizeof(devices[count].name), "Framebuffer");
        kapi_copy(devices[count].status, sizeof(devices[count].status), "Active");
        kapi_copy(devices[count].detail, sizeof(devices[count].detail), "Kenux GUI framebuffer");
        count++;
    }
    if (out_count) *out_count = count;
    return KAPI_OK;
}

int KAPI_Audio_Configure(const KAPI_AUDIO_FORMAT* format) { (void)format; return KAPI_ENOSYS; }
long KAPI_Audio_Write(const void* data, uint32_t length, uint32_t* written) {
    (void)data; (void)length; if (written) *written = 0; return KAPI_ENOSYS;
}
int KAPI_Audio_GetState(KAPI_AUDIO_STATE* state) {
    if (!state) return KAPI_EINVAL;
    memset(state, 0, sizeof(*state));
    state->status = KAPI_AUDIO_STATUS_NO_DEVICE;
    return KAPI_OK;
}

static KAPI_INPUTM_PROVIDER s_inputm_provider;
static KAPI_INPUTM_RESULT s_inputm_result;
static KAPI_INPUTM_CONTEXT s_inputm_context;
static uint32_t s_inputm_has_provider;
static uint32_t s_inputm_has_result;
static uint32_t s_inputm_window_id;
static uint8_t s_inputm_last_key;
static uint8_t s_inputm_last_pressed;
static uint32_t s_inputm_has_key;
static char s_inputm_text[KAPI_INPUTM_TEXT_LEN];

int KAPI_InputM_Register(const KAPI_INPUTM_PROVIDER* provider) {
    if (!provider) return KAPI_EINVAL;
    memcpy(&s_inputm_provider, provider, sizeof(s_inputm_provider));
    s_inputm_has_provider = 1;
    return KAPI_OK;
}
int KAPI_InputM_Unregister(void) { memset(&s_inputm_provider, 0, sizeof(s_inputm_provider)); s_inputm_has_provider = 0; return KAPI_OK; }
int KAPI_InputM_ProviderNext(KAPI_INPUTM_KEY_EVENT* event) {
    if (!event) return KAPI_EINVAL;
    if (!s_inputm_has_key) return KAPI_EAGAIN;
    memset(event, 0, sizeof(*event));
    event->window_id = s_inputm_window_id;
    event->keycode = s_inputm_last_key;
    event->pressed = s_inputm_last_pressed;
    event->context_flags = s_inputm_context.flags;
    event->caret_x = s_inputm_context.caret_x;
    event->caret_y = s_inputm_context.caret_y;
    event->caret_w = s_inputm_context.caret_w;
    event->caret_h = s_inputm_context.caret_h;
    s_inputm_has_key = 0;
    return KAPI_OK;
}
int KAPI_InputM_ProviderResult(const KAPI_INPUTM_RESULT* result) {
    if (!result) return KAPI_EINVAL;
    memcpy(&s_inputm_result, result, sizeof(s_inputm_result));
    s_inputm_has_result = 1;
    if (result->type == KAPI_INPUTM_RESULT_COMMIT) kapi_copy(s_inputm_text, sizeof(s_inputm_text), result->text);
    return KAPI_OK;
}
int KAPI_InputM_SubmitKey(uint32_t window_id, uint8_t keycode, uint8_t pressed) {
    s_inputm_window_id = window_id; s_inputm_last_key = keycode; s_inputm_last_pressed = pressed; s_inputm_has_key = 1;
    return s_inputm_has_provider ? KAPI_OK : KAPI_EAGAIN;
}
int KAPI_InputM_PollResult(KAPI_INPUTM_RESULT* result) {
    if (!result) return KAPI_EINVAL;
    if (!s_inputm_has_result) return KAPI_EAGAIN;
    memcpy(result, &s_inputm_result, sizeof(*result));
    s_inputm_has_result = 0;
    return KAPI_OK;
}
int KAPI_InputM_SetActive(uint32_t uid, const char* id) {
    (void)uid;
    if (!id) return KAPI_EINVAL;
    if (!s_inputm_has_provider || !kapi_eq(s_inputm_provider.id, id)) return KAPI_ENOENT;
    s_inputm_provider.enabled = 1;
    return KAPI_OK;
}
int KAPI_InputM_List(uint32_t uid, KAPI_INPUTM_PROVIDER* providers, uint32_t capacity, uint32_t* out_count) {
    (void)uid;
    if (!providers || capacity == 0) return KAPI_EINVAL;
    if (s_inputm_has_provider) { memcpy(&providers[0], &s_inputm_provider, sizeof(providers[0])); if (out_count) *out_count = 1; }
    else if (out_count) *out_count = 0;
    return KAPI_OK;
}
int KAPI_InputM_SetContext(const KAPI_INPUTM_CONTEXT* context) {
    if (!context) return KAPI_EINVAL;
    memcpy(&s_inputm_context, context, sizeof(s_inputm_context));
    s_inputm_window_id = context->window_id;
    return KAPI_OK;
}
int KAPI_InputM_GetState(uint32_t uid, KAPI_INPUTM_STATE* state) {
    (void)uid;
    if (!state) return KAPI_EINVAL;
    memset(state, 0, sizeof(*state));
    if (s_inputm_has_provider) { kapi_copy(state->active_id, sizeof(state->active_id), s_inputm_provider.id); state->render_flags = s_inputm_provider.render_flags; }
    kapi_copy(state->composition, sizeof(state->composition), s_inputm_result.text);
    state->candidate_count = s_inputm_result.candidate_count;
    state->selected_candidate = s_inputm_result.selected_candidate;
    state->window_id = s_inputm_context.window_id;
    state->caret_x = s_inputm_context.caret_x;
    state->caret_y = s_inputm_context.caret_y;
    state->caret_w = s_inputm_context.caret_w;
    state->caret_h = s_inputm_context.caret_h;
    return KAPI_OK;
}
int KAPI_InputM_NotifyConfig(uint32_t uid) { (void)uid; return KAPI_OK; }
int KAPI_InputM_ObserveGUIKey(uint32_t window_id, uint8_t* keycode, uint8_t pressed) {
    if (!keycode) return KAPI_EINVAL;
    return KAPI_InputM_SubmitKey(window_id, *keycode, pressed);
}
int KAPI_InputM_PollGUICommit(uint32_t window_id) { (void)window_id; return s_inputm_text[0] ? 1 : 0; }
int KAPI_InputM_TakeText(char* buffer, uint32_t capacity) {
    if (!buffer || capacity == 0) return KAPI_EINVAL;
    if (!s_inputm_text[0]) { buffer[0] = 0; return KAPI_EAGAIN; }
    kapi_copy(buffer, capacity, s_inputm_text);
    s_inputm_text[0] = 0;
    return KAPI_OK;
}
int KAPI_InputM_TakeKey(uint8_t* keycode, uint8_t* pressed) {
    if (!keycode || !pressed) return KAPI_EINVAL;
    if (!s_inputm_has_key) return KAPI_EAGAIN;
    *keycode = s_inputm_last_key; *pressed = s_inputm_last_pressed; s_inputm_has_key = 0;
    return KAPI_OK;
}
void KAPI_InputM_NoteGUIWindow(uint32_t window_id) { s_inputm_window_id = window_id; s_inputm_context.window_id = window_id; }
int KAPI_InputM_SetCurrentContext(uint32_t flags, int32_t caret_x, int32_t caret_y, uint32_t caret_w, uint32_t caret_h) {
    s_inputm_context.window_id = s_inputm_window_id; s_inputm_context.flags = flags; s_inputm_context.caret_x = caret_x;
    s_inputm_context.caret_y = caret_y; s_inputm_context.caret_w = caret_w; s_inputm_context.caret_h = caret_h;
    return KAPI_OK;
}

int KAPI_Driver_List(KAPI_DRIVER_INFO* drivers, uint32_t capacity, uint32_t* out_count) {
    if (!drivers || capacity == 0) return KAPI_EINVAL;
    memset(drivers, 0, sizeof(*drivers) * capacity);
    drivers[0].id = 1;
    drivers[0].state = KAPI_DRIVER_STATE_LOADED;
    drivers[0].kind = KAPI_DRIVER_KIND_INPUT;
    drivers[0].flags = KAPI_DRIVER_FLAG_BUILTIN;
    drivers[0].abi_version = KAPI_DRIVER_ABI_VERSION;
    kapi_copy(drivers[0].file, sizeof(drivers[0].file), "builtin");
    kapi_copy(drivers[0].name, sizeof(drivers[0].name), "Kenux builtin input");
    if (out_count) *out_count = 1;
    return KAPI_OK;
}
int KAPI_Driver_Control(uint32_t action, const char* file) { (void)action; (void)file; return KAPI_ENOSYS; }

int KAPI_License_Status(KAPI_LICENSE_INFO* info) {
    if (!info) return KAPI_EINVAL;
    memset(info, 0, sizeof(*info));
    info->status = KAPI_LICENSE_STATUS_OK;
    kapi_copy(info->mode, sizeof(info->mode), "community");
    kapi_copy(info->install_id, sizeof(info->install_id), "kenux-local");
    kapi_copy(info->detail, sizeof(info->detail), "License not required by Kenux");
    return KAPI_OK;
}
int KAPI_License_Required(void) { return 0; }
int KAPI_License_DefaultServer(char* out, uint32_t cap) { if (!out || cap == 0) return KAPI_EINVAL; kapi_copy(out, cap, ""); return KAPI_OK; }
int KAPI_License_InstallID(char* out, uint32_t cap) { if (!out || cap == 0) return KAPI_EINVAL; kapi_copy(out, cap, "kenux-local"); return KAPI_OK; }
int KAPI_License_ActivateOnline(const char* email, const char* key, char* detail, uint32_t detail_cap) {
    (void)email; (void)key; if (detail && detail_cap) kapi_copy(detail, detail_cap, "Kenux does not require activation"); return KAPI_OK;
}
int KAPI_License_ActivateOffline(const char* email, const char* offline_key, char* detail, uint32_t detail_cap) {
    (void)email; (void)offline_key; if (detail && detail_cap) kapi_copy(detail, detail_cap, "Kenux does not require activation"); return KAPI_OK;
}

int KAPI_PNG_DecodeFile(const char* path, uint32_t** out_pixels, uint32_t* out_width, uint32_t* out_height) {
    (void)path;
    if (!out_pixels || !out_width || !out_height) return KAPI_EINVAL;
    *out_pixels = 0; *out_width = 0; *out_height = 0;
    return KAPI_ENOSYS;
}
void KAPI_PNG_Free(uint32_t* pixels) { (void)pixels; }

int KAPI_PTY_Create(void) { return KAPI_ENOSYS; }
int KAPI_PTY_ReadOutput(uint32_t pty_id, char* buffer, uint32_t length) { (void)pty_id; if (buffer && length) buffer[0] = 0; return KAPI_EAGAIN; }
int KAPI_PTY_WriteInput(uint32_t pty_id, const char* buffer, uint32_t length) { (void)pty_id; (void)buffer; return (int)length; }
int KAPI_PTY_Spawn(const char* path, uint32_t pty_id) { (void)path; (void)pty_id; return KAPI_ENOSYS; }
int KAPI_PTY_SpawnArgv(const char* path, uint32_t pty_id, char* const argv[], char* const envp[]) { (void)argv; (void)envp; return KAPI_PTY_Spawn(path, pty_id); }
int KAPI_PTY_Self(void) { return KAPI_ENOSYS; }
int KAPI_PTY_InputAvailable(void) { return 0; }
int KAPI_PTY_GetTermios(uint32_t pty_id, KAPI_PTY_TERMIOS* termios) {
    (void)pty_id;
    if (!termios) return KAPI_EINVAL;
    memset(termios, 0, sizeof(*termios));
    termios->c_iflag = KAPI_PTY_IFLAG_ICRNL;
    termios->c_lflag = KAPI_PTY_LFLAG_ECHO | KAPI_PTY_LFLAG_ICANON | KAPI_PTY_LFLAG_ISIG;
    return KAPI_OK;
}
int KAPI_PTY_SetTermios(uint32_t pty_id, const KAPI_PTY_TERMIOS* termios) { (void)pty_id; return termios ? KAPI_OK : KAPI_EINVAL; }
int KAPI_PTY_GetWinsize(uint32_t pty_id, KAPI_PTY_WINSIZE* winsize) { (void)pty_id; if (!winsize) return KAPI_EINVAL; winsize->ws_row = 25; winsize->ws_col = 80; return KAPI_OK; }
int KAPI_PTY_SetWinsize(uint32_t pty_id, const KAPI_PTY_WINSIZE* winsize) { (void)pty_id; return winsize ? KAPI_OK : KAPI_EINVAL; }

static uint32_t s_startup_next_request = 1;
int KAPI_Startup_Request(const KAPI_STARTUP_COMMAND* command, uint32_t* out_request_id) {
    if (!command || !out_request_id) return KAPI_EINVAL;
    *out_request_id = s_startup_next_request++;
    return KAPI_OK;
}
int KAPI_Startup_RequestStatus(uint32_t request_id, uint32_t* out_status) { if (!request_id || !out_status) return KAPI_EINVAL; *out_status = KAPI_STARTUP_STATUS_APPROVED; return KAPI_OK; }
int KAPI_Startup_DialogGet(void* request) { (void)request; return KAPI_EAGAIN; }
int KAPI_Startup_DialogResolve(uint32_t request_id, uint32_t decision) { (void)request_id; return (decision == KAPI_STARTUP_DECISION_ALLOW || decision == KAPI_STARTUP_DECISION_DENY) ? KAPI_OK : KAPI_EINVAL; }
int KAPI_Startup_List(uint32_t uid, KAPI_STARTUP_ENTRY* entries, uint32_t capacity, uint32_t* out_count) {
    (void)uid;
    if (!entries && capacity > 0) return KAPI_EINVAL;
    if (entries && capacity) memset(entries, 0, sizeof(*entries) * capacity);
    if (out_count) *out_count = 0;
    return KAPI_OK;
}
int KAPI_Startup_SetEnabled(uint32_t uid, uint32_t entry_id, uint32_t enabled) { (void)uid; (void)entry_id; (void)enabled; return KAPI_ENOSYS; }
int KAPI_Startup_Remove(uint32_t uid, uint32_t entry_id) { (void)uid; (void)entry_id; return KAPI_ENOSYS; }
int KAPI_Startup_LaunchCurrentUser(void) { return KAPI_OK; }
