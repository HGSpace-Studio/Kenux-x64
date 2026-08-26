#include <arch/win32.h>
#include <arch/win32_process.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <arch/fs.h>
#include <arch/registry.h>

extern void* memory_alloc(uint64_t size);
extern void memory_free(void* p);

extern win32_process_t* win32_create_process(const char*, const char*, const char*);
extern BOOL win32_terminate_process(uint32_t, uint32_t);
extern HANDLE win32_handle_alloc(void*, uint32_t, uint32_t);
extern uint32_t win32_handle_duplicate(HANDLE);

#define HANDLE_OBJECT_TYPE_PIPE 13
#define HANDLE_OBJECT_TYPE_PROCESS2 14

#ifndef FILEOP_FLAGS
typedef WORD FILEOP_FLAGS;
#endif
#ifndef LPDWORD
typedef DWORD* LPDWORD;
#endif
#ifndef PHANDLE
typedef HANDLE* PHANDLE;
#endif

#define MAX_TRAY_ICONS 64
#define MAX_DRAG_FILES 32
#define CMD_MAX_VARS 256
#define CMD_MAX_LINE 4096
#define CMD_MAX_ARGS 64
#define CMD_MAX_LABELS 512

typedef struct {
    HWND hwnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    HICON hIcon;
    char szTip[128];
    char szInfo[256];
    char szInfoTitle[64];
} tray_icon_t;

static tray_icon_t g_tray_icons[MAX_TRAY_ICONS];
static int g_tray_icon_count = 0;

typedef struct {
    int file_count;
    char files[MAX_DRAG_FILES][MAX_PATH];
    POINT pt;
    BOOL pt_valid;
} drag_data_t;

static drag_data_t g_drag_data;
static BOOL g_dragging = FALSE;

typedef struct {
    char name[64];
    char value[2048];
} cmd_var_t;

typedef struct {
    char label[256];
    int line_number;
} cmd_label_t;

static cmd_var_t g_cmd_global_vars[CMD_MAX_VARS];
static int g_cmd_global_var_count = 0;

static int strncasecmp_local(const char* a, const char* b, size_t n) {
    size_t i = 0;
    while (i < n && *a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return (ca < cb) ? -1 : 1;
        a++; b++; i++;
    }
    if (i >= n) return 0;
    if (*a) return 1;
    if (*b) return -1;
    return 0;
}

static int strcasecmp_local(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return (ca < cb) ? -1 : 1;
        a++; b++;
    }
    if (*a) return 1;
    if (*b) return -1;
    return 0;
}

static char* stristr_local(const char* haystack, const char* needle) {
    size_t nl;
    if (!*needle) return (char*)haystack;
    nl = strlen(needle);
    while (*haystack) {
        if (strncasecmp_local(haystack, needle, nl) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }
    return NULL;
}

static char* strtok_local_r(char* str, const char* delim, char** context) {
    char* start;
    char* end;

    if (context == NULL || delim == NULL) return NULL;

    if (str == NULL) {
        str = *context;
    }

    if (str == NULL || *str == '\0') {
        *context = NULL;
        return NULL;
    }

    start = str;
    while (*start && strchr(delim, *start)) start++;
    if (*start == '\0') {
        *context = NULL;
        return NULL;
    }

    end = start;
    while (*end && !strchr(delim, *end)) end++;
    if (*end == '\0') {
        *context = NULL;
    } else {
        *end = '\0';
        *context = end + 1;
    }
    return start;
}

static void str_to_upper(char* s) {
    while (*s) {
        if (*s >= 'a' && *s <= 'z') *s -= 32;
        s++;
    }
}

HINSTANCE ShellExecuteA(HWND hwnd, const char* lpOperation,
                        const char* lpFile, const char* lpParameters,
                        const char* lpDirectory, INT nShowCmd)
{
    const char* ext;
    char assoc_key[256];
    char assoc_value[512];
    char cmd_line[4096];
    HKEY hkey;
    DWORD dtype;
    DWORD dsize;
    int rc;
    char* p;
    char exe_path[MAX_PATH];
    char params_buf[2048];

    (void)hwnd;
    (void)nShowCmd;

    if (lpFile == NULL || *lpFile == '\0') {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return (HINSTANCE)ERROR_FILE_NOT_FOUND;
    }

    ext = PathFindExtensionA(lpFile);

    if (lpOperation == NULL) {
        lpOperation = "open";
    }

    if (ext == NULL || *ext == '\0') {
        strncpy(exe_path, lpFile, sizeof(exe_path) - 1);
        exe_path[sizeof(exe_path) - 1] = '\0';
    } else if (strcasecmp_local(ext, ".exe") == 0 ||
               strcasecmp_local(ext, ".com") == 0 ||
               strcasecmp_local(ext, ".bat") == 0 ||
               strcasecmp_local(ext, ".cmd") == 0) {
        strncpy(exe_path, lpFile, sizeof(exe_path) - 1);
        exe_path[sizeof(exe_path) - 1] = '\0';
    } else {
        snprintf(assoc_key, sizeof(assoc_key), "%s", ext);
        rc = RegOpenKeyExA(HKEY_CLASSES_ROOT, assoc_key, 0, KEY_READ, &hkey);
        if (rc == 0) {
            dsize = sizeof(assoc_value);
            rc = RegQueryValueExA(hkey, NULL, NULL, &dtype, (BYTE*)assoc_value, &dsize);
            RegCloseKey(hkey);
            if (rc == 0 && dtype == REG_SZ) {
                snprintf(assoc_key, sizeof(assoc_key), "%s\\shell\\%s\\command", assoc_value, lpOperation);
                rc = RegOpenKeyExA(HKEY_CLASSES_ROOT, assoc_key, 0, KEY_READ, &hkey);
                if (rc == 0) {
                    dsize = sizeof(assoc_value);
                    rc = RegQueryValueExA(hkey, NULL, NULL, &dtype, (BYTE*)assoc_value, &dsize);
                    RegCloseKey(hkey);
                    if (rc == 0 && dtype == REG_SZ) {
                        char tmp[512];
                        char* dst;
                        const char* src;
                        dst = exe_path;
                        src = assoc_value;
                        while (*src == ' ') src++;
                        if (*src == '"') {
                            src++;
                            while (*src && *src != '"' && (dst - exe_path) < MAX_PATH - 1) {
                                *dst++ = *src++;
                            }
                            if (*src == '"') src++;
                        } else {
                            while (*src && *src != ' ' && (dst - exe_path) < MAX_PATH - 1) {
                                *dst++ = *src++;
                            }
                        }
                        *dst = '\0';
                        strncpy(params_buf, src, sizeof(params_buf) - 1);
                        params_buf[sizeof(params_buf) - 1] = '\0';
                    } else {
                        strncpy(exe_path, "notepad.exe", sizeof(exe_path) - 1);
                        exe_path[sizeof(exe_path) - 1] = '\0';
                        snprintf(params_buf, sizeof(params_buf), "\"%s\"", lpFile);
                    }
                } else {
                    strncpy(exe_path, "notepad.exe", sizeof(exe_path) - 1);
                    exe_path[sizeof(exe_path) - 1] = '\0';
                    snprintf(params_buf, sizeof(params_buf), "\"%s\"", lpFile);
                }
            } else {
                strncpy(exe_path, "notepad.exe", sizeof(exe_path) - 1);
                exe_path[sizeof(exe_path) - 1] = '\0';
                snprintf(params_buf, sizeof(params_buf), "\"%s\"", lpFile);
            }
        } else {
            strncpy(exe_path, "notepad.exe", sizeof(exe_path) - 1);
            exe_path[sizeof(exe_path) - 1] = '\0';
            snprintf(params_buf, sizeof(params_buf), "\"%s\"", lpFile);
        }
    }

    cmd_line[0] = '\0';
    if (lpParameters && *lpParameters) {
        if (params_buf[0]) {
            p = params_buf;
            while (*p) {
                if (strncmp(p, "%1", 2) == 0) {
                    strncat(cmd_line, lpFile, sizeof(cmd_line) - strlen(cmd_line) - 1);
                    p += 2;
                    if (lpParameters && *lpParameters) {
                        strncat(cmd_line, " ", sizeof(cmd_line) - strlen(cmd_line) - 1);
                        strncat(cmd_line, lpParameters, sizeof(cmd_line) - strlen(cmd_line) - 1);
                    }
                } else {
                    size_t cl = strlen(cmd_line);
                    if (cl < sizeof(cmd_line) - 1) {
                        cmd_line[cl] = *p;
                        cmd_line[cl + 1] = '\0';
                    }
                    p++;
                }
            }
        } else {
            snprintf(cmd_line, sizeof(cmd_line), "\"%s\" %s", exe_path, lpParameters);
        }
    } else {
        if (params_buf[0]) {
            p = params_buf;
            while (*p) {
                if (strncmp(p, "%1", 2) == 0) {
                    strncat(cmd_line, lpFile, sizeof(cmd_line) - strlen(cmd_line) - 1);
                    p += 2;
                } else {
                    size_t cl = strlen(cmd_line);
                    if (cl < sizeof(cmd_line) - 1) {
                        cmd_line[cl] = *p;
                        cmd_line[cl + 1] = '\0';
                    }
                    p++;
                }
            }
        } else {
            snprintf(cmd_line, sizeof(cmd_line), "\"%s\"", exe_path);
        }
    }

    {
        void* si[16];
        void* pi[4];
        memset(si, 0, sizeof(si));
        memset(pi, 0, sizeof(pi));
        if (CreateProcessA(exe_path, cmd_line, NULL, NULL, FALSE, 0, NULL,
                           lpDirectory, si, pi)) {
            return (HINSTANCE)42;
        }
    }

    SetLastError(ERROR_FILE_NOT_FOUND);
    return (HINSTANCE)ERROR_FILE_NOT_FOUND;
}

BOOL ShellExecuteExA(void* pExecInfo) {
    if (pExecInfo == NULL) return FALSE;
    {
        HWND hwnd = *(HWND*)((char*)pExecInfo + 8);
        char* lpVerb = *(char**)((char*)pExecInfo + 16);
        char* lpFile = *(char**)((char*)pExecInfo + 24);
        char* lpParameters = *(char**)((char*)pExecInfo + 32);
        char* lpDirectory = *(char**)((char*)pExecInfo + 40);
        int nShow = *(int*)((char*)pExecInfo + 48);
        HINSTANCE hi = ShellExecuteA(hwnd, lpVerb, lpFile, lpParameters, lpDirectory, nShow);
        if ((uintptr_t)hi > 32) {
            return TRUE;
        }
    }
    return FALSE;
}

UINT DragQueryFileA(HANDLE hDrop, UINT iFile, char* lpszFile, UINT cch) {
    drag_data_t* dd;
    if (hDrop == NULL) return 0;
    dd = (drag_data_t*)hDrop;
    if (iFile == 0xFFFFFFFF) {
        return (UINT)dd->file_count;
    }
    if (iFile >= (UINT)dd->file_count) return 0;
    if (lpszFile == NULL || cch == 0) {
        return (UINT)strlen(dd->files[iFile]);
    }
    strncpy(lpszFile, dd->files[iFile], cch - 1);
    if (cch > 0) lpszFile[cch - 1] = '\0';
    return (UINT)strlen(lpszFile);
}

BOOL DragQueryPoint(HANDLE hDrop, void* lppt) {
    drag_data_t* dd;
    if (hDrop == NULL || lppt == NULL) return FALSE;
    dd = (drag_data_t*)hDrop;
    *(POINT*)lppt = dd->pt;
    return dd->pt_valid;
}

VOID DragFinish(HANDLE hDrop) {
    (void)hDrop;
    g_dragging = FALSE;
    memset(&g_drag_data, 0, sizeof(g_drag_data));
}

BOOL Dragging(void) {
    return g_dragging;
}

typedef struct _SHFILEOPSTRUCTA_LOCAL {
    HWND   hwnd;
    UINT   wFunc;
    const char* pFrom;
    const char* pTo;
    FILEOP_FLAGS fFlags;
    BOOL   fAnyOperationsAborted;
    void*  hNameMappings;
    const char* lpszProgressTitle;
} SHFILEOPSTRUCTA_LOCAL;

int SHFileOperationA(void* lpFileOp) {
    SHFILEOPSTRUCTA_LOCAL* op;
    const char* src;
    const char* dst;
    char from_buf[4096];
    char to_buf[4096];
    char* p;
    char* next;

    if (lpFileOp == NULL) return 2;

    op = (SHFILEOPSTRUCTA_LOCAL*)lpFileOp;
    op->fAnyOperationsAborted = FALSE;

    if (op->pFrom == NULL) return 0;

    src = op->pFrom;
    dst = op->pTo;

    while (src && *src) {
        strncpy(from_buf, src, sizeof(from_buf) - 1);
        from_buf[sizeof(from_buf) - 1] = '\0';

        if (dst && *dst) {
            strncpy(to_buf, dst, sizeof(to_buf) - 1);
            to_buf[sizeof(to_buf) - 1] = '\0';
        } else {
            to_buf[0] = '\0';
        }

        switch (op->wFunc) {
            case FO_MOVE:
                if (to_buf[0]) MoveFileA(from_buf, to_buf);
                break;
            case FO_COPY:
                if (to_buf[0]) CopyFileA(from_buf, to_buf, FALSE);
                break;
            case FO_DELETE:
                DeleteFileA(from_buf);
                RemoveDirectoryA(from_buf);
                break;
            case FO_RENAME:
                if (to_buf[0]) MoveFileA(from_buf, to_buf);
                break;
        }

        p = (char*)src + strlen(src) + 1;
        if (*p == '\0') break;
        src = p;

        if (dst && *dst) {
            next = (char*)dst + strlen(dst) + 1;
            if (*next != '\0') dst = next;
        }
    }

    return 0;
}

void PathAddBackslashA(char* path) {
    size_t len;
    if (path == NULL) return;
    len = strlen(path);
    if (len == 0) {
        path[0] = '\\';
        path[1] = '\0';
        return;
    }
    if (len >= MAX_PATH - 1) return;
    if (path[len - 1] != '\\' && path[len - 1] != '/') {
        path[len] = '\\';
        path[len + 1] = '\0';
    }
}

BOOL PathRemoveBackslashA(char* path) {
    size_t len;
    if (path == NULL) return FALSE;
    len = strlen(path);
    if (len <= 1) return FALSE;
    if (len == 3 && path[1] == ':' && path[2] == '\\') return FALSE;
    if (path[len - 1] == '\\' || path[len - 1] == '/') {
        path[len - 1] = '\0';
        return TRUE;
    }
    return FALSE;
}

void PathAddExtensionA(char* path, const char* ext) {
    const char* e;
    size_t plen;
    if (path == NULL || ext == NULL) return;
    e = PathFindExtensionA(path);
    if (e && *e) return;
    plen = strlen(path);
    if (plen + strlen(ext) + 1 >= MAX_PATH) return;
    if (*ext != '.') {
        path[plen] = '.';
        strcpy(path + plen + 1, ext);
    } else {
        strcpy(path + plen, ext);
    }
}

BOOL PathRemoveExtensionA(char* path) {
    char* e;
    if (path == NULL) return FALSE;
    e = PathFindExtensionA(path);
    if (e == NULL || *e == '\0') return FALSE;
    *e = '\0';
    return TRUE;
}

BOOL PathRenameExtensionA(char* dst, const char* src, const char* ext) {
    size_t len;
    const char* e;
    if (dst == NULL || src == NULL || ext == NULL) return FALSE;
    e = PathFindExtensionA(src);
    if (e) {
        len = (size_t)(e - src);
    } else {
        len = strlen(src);
    }
    if (len + strlen(ext) + 2 >= MAX_PATH) return FALSE;
    memcpy(dst, src, len);
    dst[len] = '\0';
    if (*ext != '.') {
        dst[len] = '.';
        strcpy(dst + len + 1, ext);
    } else {
        strcpy(dst + len, ext);
    }
    return TRUE;
}

BOOL PathFileExistsA(const char* path) {
    DWORD attr;
    if (path == NULL) return FALSE;
    attr = GetFileAttributesA(path);
    if (attr == 0xFFFFFFFF) return FALSE;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) return FALSE;
    return TRUE;
}

BOOL PathDirectoryExistsA(const char* path) {
    DWORD attr;
    if (path == NULL) return FALSE;
    attr = GetFileAttributesA(path);
    if (attr == 0xFFFFFFFF) return FALSE;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
}

char* PathFindFileNameA(const char* path) {
    const char* p;
    const char* last;
    if (path == NULL) return NULL;
    last = path;
    for (p = path; *p; p++) {
        if (*p == '\\' || *p == '/') {
            if (p[1]) last = p + 1;
        }
    }
    return (char*)last;
}

char* PathFindExtensionA(const char* path) {
    const char* fn;
    const char* p;
    if (path == NULL) return NULL;
    fn = PathFindFileNameA(path);
    for (p = fn; *p; p++);
    while (p > fn) {
        p--;
        if (*p == '.') return (char*)p;
        if (*p == '\\' || *p == '/') break;
    }
    return (char*)p + strlen(p);
}

char* PathFindNextComponentA(const char* path) {
    const char* p;
    if (path == NULL) return NULL;
    for (p = path; *p; p++) {
        if (*p == '\\' || *p == '/') {
            while (*p == '\\' || *p == '/') p++;
            return (char*)p;
        }
    }
    return (char*)p;
}

BOOL PathIsDirectoryA(const char* path) {
    return PathDirectoryExistsA(path);
}

BOOL PathIsFileSpecA(const char* path) {
    const char* p;
    if (path == NULL) return FALSE;
    for (p = path; *p; p++) {
        if (*p == '*' || *p == '?') return TRUE;
    }
    return FALSE;
}

BOOL PathIsRootA(const char* path) {
    size_t len;
    if (path == NULL) return FALSE;
    len = strlen(path);
    if (len == 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) return TRUE;
    if (len == 2 && path[1] == ':') return TRUE;
    if (len == 1 && (path[0] == '\\' || path[0] == '/')) return TRUE;
    return FALSE;
}

BOOL PathIsRelativeA(const char* path) {
    if (path == NULL) return TRUE;
    if (path[0] == '\\' || path[0] == '/') return FALSE;
    if (path[0] && path[1] == ':') return FALSE;
    return TRUE;
}

BOOL PathStripPathA(char* path) {
    char* fn;
    if (path == NULL) return FALSE;
    fn = PathFindFileNameA(path);
    if (fn == path) return FALSE;
    memmove(path, fn, strlen(fn) + 1);
    return TRUE;
}

BOOL PathStripToRootA(char* path) {
    if (path == NULL) return FALSE;
    if (path[0] && path[1] == ':') {
        path[2] = '\\';
        path[3] = '\0';
        return TRUE;
    }
    if (path[0] == '\\' || path[0] == '/') {
        path[0] = '\\';
        path[1] = '\0';
        return TRUE;
    }
    return FALSE;
}

BOOL PathQuoteSpacesA(char* path) {
    size_t len;
    int has_spaces;
    const char* p;
    char buf[MAX_PATH];
    if (path == NULL) return FALSE;
    has_spaces = 0;
    for (p = path; *p; p++) {
        if (*p == ' ') { has_spaces = 1; break; }
    }
    if (!has_spaces) return FALSE;
    len = strlen(path);
    if (len + 2 >= MAX_PATH) return FALSE;
    buf[0] = '"';
    memcpy(buf + 1, path, len);
    buf[len + 1] = '"';
    buf[len + 2] = '\0';
    strcpy(path, buf);
    return TRUE;
}

BOOL PathUnquoteSpacesA(char* path) {
    size_t len;
    if (path == NULL) return FALSE;
    len = strlen(path);
    if (len < 2) return FALSE;
    if (path[0] != '"') return FALSE;
    if (path[len - 1] != '"') return FALSE;
    memmove(path, path + 1, len - 2);
    path[len - 2] = '\0';
    return TRUE;
}

BOOL PathCanonicalizeA(char* dst, const char* src) {
    char* comps[64];
    int ncomp;
    int i;
    int out_i;
    const char* p;
    const char* start;
    char buf[MAX_PATH];
    char drive[8];
    int is_abs;
    int add_slash;
    size_t len;

    if (dst == NULL || src == NULL) return FALSE;

    ncomp = 0;
    drive[0] = '\0';
    is_abs = 0;

    if (src[0] && src[1] == ':') {
        drive[0] = src[0];
        drive[1] = ':';
        drive[2] = '\\';
        drive[3] = '\0';
        p = src + 2;
        is_abs = 1;
    } else if (src[0] == '\\' || src[0] == '/') {
        p = src;
        is_abs = 1;
    } else {
        p = src;
    }

    while (*p) {
        while (*p == '\\' || *p == '/') p++;
        if (!*p) break;
        start = p;
        while (*p && *p != '\\' && *p != '/') p++;
        len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') {
            continue;
        } else if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (ncomp > 0) {
                ncomp--;
            }
        } else {
            if (ncomp < 64) {
                comps[ncomp] = (char*)start;
                comps[ncomp + 128] = (char*)len;
                ncomp++;
            }
        }
    }

    {
        char tmp[MAX_PATH];
        int ti;
        const char* sp;
        int ci;
        (void)buf;
        (void)out_i;
        (void)add_slash;

        ti = 0;
        if (drive[0]) {
            strcpy(tmp, drive);
            ti = (int)strlen(tmp);
        } else if (is_abs) {
            tmp[ti++] = '\\';
            tmp[ti] = '\0';
        } else {
            tmp[ti] = '\0';
        }
        for (i = 0; i < ncomp; i++) {
            sp = comps[i];
            ci = (int)((uintptr_t)comps[i + 128]);
            if (ti > 0 && tmp[ti - 1] != '\\') {
                tmp[ti++] = '\\';
            }
            memcpy(tmp + ti, sp, ci);
            ti += ci;
            tmp[ti] = '\0';
        }
        if (ti == 0) {
            if (!is_abs) {
                tmp[ti++] = '.';
                tmp[ti] = '\0';
            } else if (drive[0]) {
                strcpy(tmp, drive);
            } else {
                tmp[ti++] = '\\';
                tmp[ti] = '\0';
            }
        }
        strncpy(dst, tmp, MAX_PATH - 1);
        dst[MAX_PATH - 1] = '\0';
    }
    return TRUE;
}

BOOL PathCombineA(char* dst, const char* dir, const char* file) {
    char tmp[MAX_PATH];
    size_t dl;

    if (dst == NULL) return FALSE;

    if (file == NULL || *file == '\0') {
        if (dir == NULL) { dst[0] = '\0'; return FALSE; }
        strncpy(dst, dir, MAX_PATH - 1);
        dst[MAX_PATH - 1] = '\0';
        return TRUE;
    }

    if (PathIsRelativeA(file) == FALSE) {
        PathCanonicalizeA(dst, file);
        return TRUE;
    }

    if (dir == NULL || *dir == '\0') {
        PathCanonicalizeA(dst, file);
        return TRUE;
    }

    strncpy(tmp, dir, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    dl = strlen(tmp);
    if (dl > 0 && tmp[dl - 1] != '\\' && tmp[dl - 1] != '/') {
        if (dl < sizeof(tmp) - 2) {
            tmp[dl] = '\\';
            tmp[dl + 1] = '\0';
        }
    }
    strncat(tmp, file, sizeof(tmp) - strlen(tmp) - 1);
    PathCanonicalizeA(dst, tmp);
    return TRUE;
}

BOOL PathAppendA(char* path, const char* more) {
    size_t pl;
    if (path == NULL || more == NULL) return FALSE;
    pl = strlen(path);
    if (pl > 0 && path[pl - 1] != '\\' && path[pl - 1] != '/') {
        if (pl + 1 >= MAX_PATH) return FALSE;
        path[pl] = '\\';
        path[pl + 1] = '\0';
        pl++;
    }
    if (pl + strlen(more) + 1 >= MAX_PATH) return FALSE;
    strcat(path, more);
    return TRUE;
}

void PathBuildRootA(char* root, int drive) {
    if (root == NULL) return;
    if (drive >= 0 && drive <= 25) {
        root[0] = (char)('A' + drive);
    } else {
        root[0] = 'C';
    }
    root[1] = ':';
    root[2] = '\\';
    root[3] = '\0';
}

BOOL PathGetDriveNumberA(const char* path) {
    if (path == NULL) return -1;
    if (path[0] && path[1] == ':') {
        char c = path[0];
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a';
        return -1;
    }
    return -1;
}

BOOL PathMatchSpecA(const char* path, const char* spec) {
    const char* p;
    const char* s;
    const char* star_p;
    const char* star_s;

    if (path == NULL || spec == NULL) return FALSE;

    p = path; s = spec;
    star_p = NULL; star_s = NULL;

    while (*p) {
        if (*s == '*') {
            if (!*++s) return TRUE;
            star_s = s;
            star_p = p;
        } else if (*s == '?' || (*s && (
            (*s >= 'A' && *s <= 'Z' && *s + 32 == *p) ||
            (*s >= 'a' && *s <= 'z' && *s - 32 == *p) ||
            *s == *p))) {
            if (*s) s++;
            p++;
        } else if (star_s) {
            s = star_s;
            p = ++star_p;
        } else {
            return FALSE;
        }
    }
    while (*s == '*') s++;
    return !*s;
}

typedef struct {
    int csidl;
    const char* path;
} csidl_map_t;

static const csidl_map_t g_csidl_map[] = {
    { CSIDL_DESKTOP, "C:\\Users\\User\\Desktop" },
    { CSIDL_DESKTOPDIRECTORY, "C:\\Users\\User\\Desktop" },
    { CSIDL_PERSONAL, "C:\\Users\\User\\Documents" },
    { CSIDL_MYDOCUMENTS, "C:\\Users\\User\\Documents" },
    { CSIDL_MYPICTURES, "C:\\Users\\User\\Pictures" },
    { CSIDL_MYMUSIC, "C:\\Users\\User\\Music" },
    { CSIDL_MYVIDEO, "C:\\Users\\User\\Videos" },
    { CSIDL_FAVORITES, "C:\\Users\\User\\Favorites" },
    { CSIDL_STARTUP, "C:\\Users\\User\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup" },
    { CSIDL_STARTMENU, "C:\\Users\\User\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu" },
    { CSIDL_PROGRAMS, "C:\\Users\\User\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs" },
    { CSIDL_RECENT, "C:\\Users\\User\\AppData\\Roaming\\Microsoft\\Windows\\Recent" },
    { CSIDL_SENDTO, "C:\\Users\\User\\AppData\\Roaming\\Microsoft\\Windows\\SendTo" },
    { CSIDL_APPDATA, "C:\\Users\\User\\AppData\\Roaming" },
    { CSIDL_LOCAL_APPDATA, "C:\\Users\\User\\AppData\\Local" },
    { CSIDL_COOKIES, "C:\\Users\\User\\AppData\\Local\\Microsoft\\Windows\\INetCookies" },
    { CSIDL_HISTORY, "C:\\Users\\User\\AppData\\Local\\Microsoft\\Windows\\History" },
    { CSIDL_INTERNET_CACHE, "C:\\Users\\User\\AppData\\Local\\Microsoft\\Windows\\INetCache" },
    { CSIDL_TEMPLATES, "C:\\Users\\User\\AppData\\Roaming\\Microsoft\\Windows\\Templates" },
    { CSIDL_ADMINTOOLS, "C:\\Users\\User\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Administrative Tools" },
    { CSIDL_WINDOWS, "C:\\Windows" },
    { CSIDL_SYSTEM, "C:\\Windows\\System32" },
    { CSIDL_SYSTEMX86, "C:\\Windows\\SysWOW64" },
    { CSIDL_PROGRAM_FILES, "C:\\Program Files" },
    { CSIDL_PROGRAM_FILESX86, "C:\\Program Files (x86)" },
    { CSIDL_PROGRAM_FILES_COMMON, "C:\\Program Files\\Common Files" },
    { CSIDL_PROGRAM_FILES_COMMONX86, "C:\\Program Files (x86)\\Common Files" },
    { CSIDL_PROFILE, "C:\\Users\\User" },
    { CSIDL_DRIVES, "C:\\" },
    { CSIDL_NETWORK, "\\\\" },
    { CSIDL_FONTS, "C:\\Windows\\Fonts" },
    { CSIDL_COMMON_STARTMENU, "C:\\ProgramData\\Microsoft\\Windows\\Start Menu" },
    { CSIDL_COMMON_PROGRAMS, "C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs" },
    { CSIDL_COMMON_STARTUP, "C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Startup" },
    { CSIDL_COMMON_DESKTOPDIRECTORY, "C:\\Users\\Public\\Desktop" },
    { CSIDL_COMMON_APPDATA, "C:\\ProgramData" },
    { CSIDL_COMMON_TEMPLATES, "C:\\ProgramData\\Microsoft\\Windows\\Templates" },
    { CSIDL_COMMON_DOCUMENTS, "C:\\Users\\Public\\Documents" },
    { CSIDL_COMMON_ADMINTOOLS, "C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Administrative Tools" },
    { CSIDL_COMMON_FAVORITES, "C:\\Users\\Public\\Favorites" },
    { -1, NULL }
};

int SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, char* pszPath) {
    int i;
    int actual;
    const char* path;

    (void)hwnd;
    (void)hToken;
    (void)dwFlags;

    if (pszPath == NULL) return -2147024809;

    actual = csidl & ~CSIDL_FLAG_CREATE;

    for (i = 0; g_csidl_map[i].csidl != -1; i++) {
        if (g_csidl_map[i].csidl == actual) {
            path = g_csidl_map[i].path;
            strncpy(pszPath, path, MAX_PATH - 1);
            pszPath[MAX_PATH - 1] = '\0';
            if (csidl & CSIDL_FLAG_CREATE) {
                CreateDirectoryA(pszPath, NULL);
            }
            return 0;
        }
    }
    return -2147024809;
}

int SHGetSpecialFolderPathA(HWND hwnd, char* pszPath, int csidl, BOOL fCreate) {
    int flags = 0;
    if (fCreate) flags |= CSIDL_FLAG_CREATE;
    return (SHGetFolderPathA(hwnd, csidl | flags, NULL, SHGFP_TYPE_CURRENT, pszPath) == 0) ? TRUE : FALSE;
}

typedef struct _NOTIFYICONDATAA_LOCAL {
    DWORD cbSize;
    HWND hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    HICON hIcon;
    char szTip[128];
    DWORD dwState;
    DWORD dwStateMask;
    char szInfo[256];
    union {
        UINT uTimeout;
        UINT uVersion;
    } DUMMYUNIONNAME;
    char szInfoTitle[64];
    DWORD dwInfoFlags;
    GUID guidItem;
    HICON hBalloonIcon;
} NOTIFYICONDATAA_LOCAL;

BOOL Shell_NotifyIconA(DWORD dwMessage, void* pnid) {
    NOTIFYICONDATAA_LOCAL* nid;
    int i;
    tray_icon_t* ti;

    if (pnid == NULL) return FALSE;
    nid = (NOTIFYICONDATAA_LOCAL*)pnid;

    switch (dwMessage) {
        case NIM_ADD:
            if (g_tray_icon_count >= MAX_TRAY_ICONS) return FALSE;
            ti = &g_tray_icons[g_tray_icon_count++];
            ti->hwnd = nid->hWnd;
            ti->uID = nid->uID;
            ti->uFlags = nid->uFlags;
            ti->uCallbackMessage = nid->uCallbackMessage;
            ti->hIcon = nid->hIcon;
            strncpy(ti->szTip, nid->szTip, sizeof(ti->szTip) - 1);
            ti->szTip[sizeof(ti->szTip) - 1] = '\0';
            return TRUE;

        case NIM_MODIFY:
            for (i = 0; i < g_tray_icon_count; i++) {
                if (g_tray_icons[i].hwnd == nid->hWnd && g_tray_icons[i].uID == nid->uID) {
                    ti = &g_tray_icons[i];
                    if (nid->uFlags & NIF_MESSAGE) ti->uCallbackMessage = nid->uCallbackMessage;
                    if (nid->uFlags & NIF_ICON) ti->hIcon = nid->hIcon;
                    if (nid->uFlags & NIF_TIP) {
                        strncpy(ti->szTip, nid->szTip, sizeof(ti->szTip) - 1);
                        ti->szTip[sizeof(ti->szTip) - 1] = '\0';
                    }
                    if (nid->uFlags & NIF_INFO) {
                        strncpy(ti->szInfo, nid->szInfo, sizeof(ti->szInfo) - 1);
                        ti->szInfo[sizeof(ti->szInfo) - 1] = '\0';
                        strncpy(ti->szInfoTitle, nid->szInfoTitle, sizeof(ti->szInfoTitle) - 1);
                        ti->szInfoTitle[sizeof(ti->szInfoTitle) - 1] = '\0';
                    }
                    return TRUE;
                }
            }
            return FALSE;

        case NIM_DELETE:
            for (i = 0; i < g_tray_icon_count; i++) {
                if (g_tray_icons[i].hwnd == nid->hWnd && g_tray_icons[i].uID == nid->uID) {
                    int j;
                    for (j = i; j < g_tray_icon_count - 1; j++) {
                        g_tray_icons[j] = g_tray_icons[j + 1];
                    }
                    g_tray_icon_count--;
                    return TRUE;
                }
            }
            return FALSE;

        case NIM_SETFOCUS:
            return TRUE;

        case NIM_SETVERSION:
            return TRUE;
    }
    return FALSE;
}

void cmd_init(cmd_state_t* cmd, int interactive) {
    if (cmd == NULL) return;
    memset(cmd, 0, sizeof(cmd_state_t));
    strncpy(cmd->cwd, "C:\\", sizeof(cmd->cwd) - 1);
    cmd->cwd[sizeof(cmd->cwd) - 1] = '\0';
    cmd->echo_on = 1;
    cmd->interactive = interactive;
    cmd->errorlevel = 0;
    cmd->nvars = 0;
    cmd->recurse_depth = 0;
    memset(&cmd->if_state, 0, sizeof(cmd->if_state));
    memset(&cmd->for_state, 0, sizeof(cmd->for_state));
    memset(&cmd->goto_state, 0, sizeof(cmd->goto_state));
    memset(&cmd->call_stack, 0, sizeof(cmd->call_stack));

    cmd_set_var(cmd, "PATH", "C:\\Windows\\system32;C:\\Windows;C:\\Windows\\System32\\Wbem");
    cmd_set_var(cmd, "PROMPT", "$P$G");
    cmd_set_var(cmd, "TEMP", "C:\\Windows\\Temp");
    cmd_set_var(cmd, "TMP", "C:\\Windows\\Temp");
    cmd_set_var(cmd, "SYSTEMROOT", "C:\\Windows");
    cmd_set_var(cmd, "SYSTEMDRIVE", "C:");
    cmd_set_var(cmd, "WINDIR", "C:\\Windows");
    cmd_set_var(cmd, "USERNAME", "User");
    cmd_set_var(cmd, "USERPROFILE", "C:\\Users\\User");
    cmd_set_var(cmd, "COMPUTERNAME", "KENUXK-PC");
    cmd_set_var(cmd, "OS", "Windows_NT");
    cmd_set_var(cmd, "PATHEXT", ".COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC");
}

void cmd_destroy(cmd_state_t* cmd) {
    int i;
    if (cmd == NULL) return;
    for (i = 0; i < cmd->nvars; i++) {
        if (cmd->vars[i]) {
            memory_free(cmd->vars[i]);
            cmd->vars[i] = NULL;
        }
    }
    cmd->nvars = 0;
}

int cmd_set_var(cmd_state_t* cmd, const char* name, const char* value) {
    int i;
    char entry[2560];
    size_t nl;

    if (cmd == NULL || name == NULL || value == NULL) return -1;
    nl = strlen(name);
    if (nl == 0) return -1;

    for (i = 0; i < cmd->nvars; i++) {
        const char* v = cmd->vars[i];
        if (v && strncasecmp_local(v, name, nl) == 0 && v[nl] == '=') {
            snprintf(entry, sizeof(entry), "%s=%s", name, value);
            memory_free(cmd->vars[i]);
            cmd->vars[i] = (char*)memory_alloc(strlen(entry) + 1);
            if (cmd->vars[i] == NULL) return -1;
            strcpy(cmd->vars[i], entry);
            return 0;
        }
    }

    if (cmd->nvars >= 256) return -1;
    snprintf(entry, sizeof(entry), "%s=%s", name, value);
    cmd->vars[cmd->nvars] = (char*)memory_alloc(strlen(entry) + 1);
    if (cmd->vars[cmd->nvars] == NULL) return -1;
    strcpy(cmd->vars[cmd->nvars], entry);
    cmd->nvars++;
    return 0;
}

const char* cmd_get_var(cmd_state_t* cmd, const char* name) {
    int i;
    size_t nl;
    if (cmd == NULL || name == NULL) return NULL;
    nl = strlen(name);
    if (nl == 0) return NULL;
    for (i = 0; i < cmd->nvars; i++) {
        const char* v = cmd->vars[i];
        if (v && strncasecmp_local(v, name, nl) == 0 && v[nl] == '=') {
            return v + nl + 1;
        }
    }
    return NULL;
}

int cmd_unset_var(cmd_state_t* cmd, const char* name) {
    int i;
    size_t nl;
    if (cmd == NULL || name == NULL) return -1;
    nl = strlen(name);
    if (nl == 0) return -1;
    for (i = 0; i < cmd->nvars; i++) {
        const char* v = cmd->vars[i];
        if (v && strncasecmp_local(v, name, nl) == 0 && v[nl] == '=') {
            memory_free(cmd->vars[i]);
            cmd->vars[i] = NULL;
            {
                int j;
                for (j = i; j < cmd->nvars - 1; j++) {
                    cmd->vars[j] = cmd->vars[j + 1];
                }
                cmd->nvars--;
            }
            return 0;
        }
    }
    return -1;
}

static int cmd_atoi_safe(const char* s) {
    if (s == NULL) return 0;
    while (*s == ' ') s++;
    if (*s == '-') {
        s++;
        return -atoi(s);
    }
    return atoi(s);
}

int cmd_expand(cmd_state_t* cmd, const char* in, char* out, size_t outsz) {
    size_t oi;
    const char* p;

    if (cmd == NULL || in == NULL || out == NULL || outsz == 0) return -1;
    oi = 0;
    p = in;

    while (*p && oi < outsz - 1) {
        if (*p == '%') {
            p++;
            if (*p == '%') {
                out[oi++] = '%';
                p++;
            } else if (*p == '*') {
                out[oi++] = '%';
                out[oi++] = '*';
                p++;
            } else if (*p >= '0' && *p <= '9') {
                int argn = *p - '0';
                (void)argn;
                p++;
            } else if (*p == '~') {
                while (*p && *p != '%') p++;
                if (*p) p++;
            } else {
                char vname[256];
                int vi = 0;
                const char* val;
                while (*p && *p != '%' && vi < 255) {
                    vname[vi++] = *p++;
                }
                vname[vi] = '\0';
                if (*p == '%') p++;
                if (strcasecmp_local(vname, "errorlevel") == 0) {
                    char tmp[32];
                    snprintf(tmp, sizeof(tmp), "%d", cmd->errorlevel);
                    {
                        size_t tl = strlen(tmp);
                        if (oi + tl < outsz - 1) {
                            memcpy(out + oi, tmp, tl);
                            oi += tl;
                        }
                    }
                } else {
                    val = cmd_get_var(cmd, vname);
                    if (val) {
                        size_t vl = strlen(val);
                        if (oi + vl < outsz - 1) {
                            memcpy(out + oi, val, vl);
                            oi += vl;
                        }
                    } else {
                        size_t vl;
                        char envval[2048];
                        DWORD r = GetEnvironmentVariableA(vname, envval, sizeof(envval));
                        if (r > 0) {
                            vl = strlen(envval);
                            if (oi + vl < outsz - 1) {
                                memcpy(out + oi, envval, vl);
                                oi += vl;
                            }
                        }
                    }
                }
            }
        } else {
            out[oi++] = *p++;
        }
    }
    out[oi] = '\0';
    return (int)oi;
}

static int cmd_tokenize(const char* line, char** argv, int maxarg) {
    int argc;
    const char* p;
    int in_quotes;
    char* buf;
    size_t line_len;

    if (line == NULL || argv == NULL || maxarg <= 0) return 0;

    line_len = strlen(line) + 1;
    buf = (char*)memory_alloc(line_len);
    if (buf == NULL) return 0;

    argc = 0;
    p = line;
    in_quotes = 0;

    while (*p && argc < maxarg) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        if (*p == '"') {
            in_quotes = 1;
            p++;
        } else {
            in_quotes = 0;
        }

        argv[argc] = buf + (p - line);

        while (*p) {
            if (in_quotes) {
                if (*p == '"') {
                    p++;
                    break;
                }
                buf[p - line] = *p;
                p++;
            } else {
                if (*p == ' ' || *p == '\t') break;
                if (*p == '"') {
                    in_quotes = 1;
                    p++;
                    continue;
                }
                buf[p - line] = *p;
                p++;
            }
        }
        buf[p - line < (int)line_len ? p - line : (int)line_len - 1] = '\0';
        argc++;
    }

    return argc;
}

static char* cmd_strdup_local(const char* s) {
    size_t n;
    char* r;
    if (s == NULL) return NULL;
    n = strlen(s) + 1;
    r = (char*)memory_alloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

static int cmd_eval_expr(const char* expr) {
    const char* p;
    int result;
    int cur;
    char op;
    int tmp;

    if (expr == NULL) return 0;
    p = expr;
    while (*p == ' ') p++;

    result = 0;
    op = '+';

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        if (*p == '(') {
            int depth = 1;
            const char* s = p + 1;
            char sub[1024];
            int si = 0;
            while (*p && depth) {
                p++;
                if (*p == '(') depth++;
                else if (*p == ')') depth--;
                if (depth && si < 1023) sub[si++] = *p;
            }
            if (*p) p++;
            sub[si] = '\0';
            cur = cmd_eval_expr(sub);
            (void)s;
        } else {
            int neg = 0;
            while (*p == ' ') p++;
            if (*p == '-') { neg = 1; p++; }
            else if (*p == '+') p++;
            while (*p == ' ') p++;
            cur = 0;
            if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
                p += 2;
                while (*p) {
                    char c = *p;
                    if (c >= '0' && c <= '9') { cur = cur * 16 + (c - '0'); p++; }
                    else if (c >= 'a' && c <= 'f') { cur = cur * 16 + (c - 'a' + 10); p++; }
                    else if (c >= 'A' && c <= 'F') { cur = cur * 16 + (c - 'A' + 10); p++; }
                    else break;
                }
            } else {
                while (*p >= '0' && *p <= '9') {
                    cur = cur * 10 + (*p - '0');
                    p++;
                }
            }
            if (neg) cur = -cur;
        }

        tmp = result;
        switch (op) {
            case '+': result = tmp + cur; break;
            case '-': result = tmp - cur; break;
            case '*': result = tmp * cur; break;
            case '/': result = cur ? tmp / cur : 0; break;
            case '%': result = cur ? tmp % cur : 0; break;
            case '&': result = tmp & cur; break;
            case '|': result = tmp | cur; break;
            case '^': result = tmp ^ cur; break;
            default:  result = cur; break;
        }

        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '%' ||
            *p == '&' || *p == '|' || *p == '^') {
            op = *p++;
        } else {
            break;
        }
    }

    return result;
}

static int cmd_builtin_echo(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_cd(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_md(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_rd(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_dir(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_type(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_copy(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_del(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_set(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_cls(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_ver(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_date(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_time(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_path(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_prompt(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_exit(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_title(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_pause(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_call(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_find(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_sort(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_more(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_help(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_ren(cmd_state_t* cmd, int argc, char** argv);
static int cmd_builtin_move(cmd_state_t* cmd, int argc, char** argv);

cmd_builtin_t cmd_builtins[] = {
    { "echo",   cmd_builtin_echo,   "ECHO [ON|OFF] | ECHO message" },
    { "cd",     cmd_builtin_cd,     "CD / CHDIR [/D] [drive:][path]" },
    { "chdir",  cmd_builtin_cd,     "CD / CHDIR [/D] [drive:][path]" },
    { "md",     cmd_builtin_md,     "MD / MKDIR [drive:]path" },
    { "mkdir",  cmd_builtin_md,     "MD / MKDIR [drive:]path" },
    { "rd",     cmd_builtin_rd,     "RD / RMDIR [/S] [/Q] [drive:]path" },
    { "rmdir",  cmd_builtin_rd,     "RD / RMDIR [/S] [/Q] [drive:]path" },
    { "dir",    cmd_builtin_dir,    "DIR [drive:][path][filename] [/P] [/W]" },
    { "type",   cmd_builtin_type,   "TYPE [drive:][path]filename" },
    { "copy",   cmd_builtin_copy,   "COPY [/Y] source [+ source ...] [destination]" },
    { "del",    cmd_builtin_del,    "DEL / ERASE [/P] [/F] names..." },
    { "erase",  cmd_builtin_del,    "DEL / ERASE [/P] [/F] names..." },
    { "set",    cmd_builtin_set,    "SET [var[=[str]]] | SET /A expr | SET /P var=prompt" },
    { "cls",    cmd_builtin_cls,    "CLS - clear screen" },
    { "ver",    cmd_builtin_ver,    "VER - display version" },
    { "date",   cmd_builtin_date,   "DATE - display/set date" },
    { "time",   cmd_builtin_time,   "TIME - display/set time" },
    { "path",   cmd_builtin_path,   "PATH [[drive:]path[;...]]" },
    { "prompt", cmd_builtin_prompt, "PROMPT [text]" },
    { "exit",   cmd_builtin_exit,   "EXIT [/B] [exitcode]" },
    { "title",  cmd_builtin_title,  "TITLE [string]" },
    { "pause",  cmd_builtin_pause,  "PAUSE - suspend and wait for input" },
    { "call",   cmd_builtin_call,   "CALL [drive:][path]filename [args...] | CALL :label [args...]" },
    { "find",   cmd_builtin_find,   "FIND [/V] [/C] [/N] [/I] \"string\" files..." },
    { "sort",   cmd_builtin_sort,   "SORT [/R] [/+n] [file]" },
    { "more",   cmd_builtin_more,   "MORE [file]" },
    { "help",   cmd_builtin_help,   "HELP [command]" },
    { "ren",    cmd_builtin_ren,    "REN [drive:][path]filename1 filename2" },
    { "rename", cmd_builtin_ren,    "REN [drive:][path]filename1 filename2" },
    { "move",   cmd_builtin_move,   "MOVE [/Y] [/-Y] source dest" },
    { NULL,     NULL,               NULL }
};

int cmd_builtin_count = 30;

static void cmd_write_stdout(const char* s) {
    if (s == NULL) return;
    fprintf(stdout, "%s", s);
    fflush(stdout);
}

static void cmd_write_stdout_line(const char* s) {
    if (s) cmd_write_stdout(s);
    cmd_write_stdout("\r\n");
}

static int cmd_find_builtin(const char* name) {
    int i;
    if (name == NULL) return -1;
    for (i = 0; cmd_builtins[i].name; i++) {
        if (strcasecmp_local(cmd_builtins[i].name, name) == 0) return i;
    }
    return -1;
}

static int cmd_builtin_echo(cmd_state_t* cmd, int argc, char** argv) {
    if (argc <= 1) {
        cmd_write_stdout(cmd->echo_on ? "ECHO is on.\r\n" : "ECHO is off.\r\n");
        return 0;
    }
    if (strcasecmp_local(argv[1], "on") == 0) {
        cmd->echo_on = 1;
        return 0;
    }
    if (strcasecmp_local(argv[1], "off") == 0) {
        cmd->echo_on = 0;
        return 0;
    }
    {
        int i;
        for (i = 1; i < argc; i++) {
            if (i > 1) cmd_write_stdout(" ");
            cmd_write_stdout(argv[i]);
        }
        cmd_write_stdout("\r\n");
    }
    return 0;
}

static int cmd_builtin_cd(cmd_state_t* cmd, int argc, char** argv) {
    int opt_d = 0;
    int ai;
    char* path;
    char full[MAX_PATH];
    const char* old_cd;

    if (argc <= 1) {
        cmd_write_stdout(cmd->cwd);
        cmd_write_stdout("\r\n");
        return 0;
    }
    ai = 1;
    if (argc > ai && strcmp(argv[ai], "/D") == 0) {
        opt_d = 1;
        ai++;
    }
    if (argc <= ai) {
        cmd_write_stdout(cmd->cwd);
        cmd_write_stdout("\r\n");
        return 0;
    }
    path = argv[ai];
    if (strcmp(path, "-") == 0) {
        old_cd = cmd_get_var(cmd, "_OLDPWD");
        if (old_cd) {
            path = (char*)old_cd;
        } else {
            return 0;
        }
    }
    if (path[0] && path[1] == ':') {
        if (path[2] == '\0' || (path[2] == '\\' && path[3] == '\0')) {
            char root[8];
            root[0] = path[0]; root[1] = ':'; root[2] = '\\'; root[3] = '\0';
            cmd_set_var(cmd, "_OLDPWD", cmd->cwd);
            strncpy(cmd->cwd, root, sizeof(cmd->cwd) - 1);
            cmd->cwd[sizeof(cmd->cwd) - 1] = '\0';
            return 0;
        }
    }
    if (!opt_d && path[0] && path[1] == ':' && cmd->cwd[0] != path[0]) {
        char root[8];
        root[0] = path[0]; root[1] = ':'; root[2] = '\\'; root[3] = '\0';
        strncpy(cmd->cwd, root, sizeof(cmd->cwd) - 1);
        cmd->cwd[sizeof(cmd->cwd) - 1] = '\0';
        return 0;
    }
    PathCombineA(full, cmd->cwd, path);
    PathCanonicalizeA(full, full);
    if (PathFileExistsA(full) || PathDirectoryExistsA(full) || PathIsRootA(full)) {
        PathAddBackslashA(full);
        PathRemoveBackslashA(full);
        {
            size_t fl = strlen(full);
            if (fl > 0 && full[fl - 1] != '\\') {
                full[fl] = '\\';
                full[fl + 1] = '\0';
            }
        }
        cmd_set_var(cmd, "_OLDPWD", cmd->cwd);
        strncpy(cmd->cwd, full, sizeof(cmd->cwd) - 1);
        cmd->cwd[sizeof(cmd->cwd) - 1] = '\0';
        return 0;
    } else {
        cmd_write_stdout("The system cannot find the path specified.\r\n");
        return 1;
    }
}

static int cmd_builtin_md(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    char full[MAX_PATH];
    int rc;
    (void)cmd;
    rc = 0;
    for (i = 1; i < argc; i++) {
        PathCombineA(full, cmd->cwd, argv[i]);
        PathCanonicalizeA(full, full);
        if (!CreateDirectoryA(full, NULL)) {
            fprintf(stdout, "A subdirectory or file %s already exists.\r\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}

static int cmd_builtin_rd(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    int ai;
    int quiet = 0;
    int recurse = 0;
    char full[MAX_PATH];
    int rc = 0;
    (void)cmd;
    ai = 1;
    for (ai = 1; ai < argc; ai++) {
        if (argv[ai][0] == '/') {
            if (strchr(argv[ai], 's') || strchr(argv[ai], 'S')) recurse = 1;
            if (strchr(argv[ai], 'q') || strchr(argv[ai], 'Q')) quiet = 1;
        } else break;
    }
    for (i = ai; i < argc; i++) {
        PathCombineA(full, cmd->cwd, argv[i]);
        PathCanonicalizeA(full, full);
        if (!RemoveDirectoryA(full)) {
            if (!recurse && !quiet) {
                fprintf(stdout, "The directory is not empty: %s\r\n", argv[i]);
            }
            rc = 1;
        }
    }
    (void)quiet;
    (void)recurse;
    return rc;
}

static int cmd_builtin_dir(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    char path[MAX_PATH];
    char pattern[MAX_PATH];
    char base[MAX_PATH];
    HANDLE hFind;
    char fdbuf[4096];
    int found;
    DWORD attr;
    uint64_t total_size;
    int file_count;
    int dir_count;
    (void)argc;
    (void)argv;

    pattern[0] = '\0';
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '/') {
            strncpy(pattern, argv[i], sizeof(pattern) - 1);
            pattern[sizeof(pattern) - 1] = '\0';
            break;
        }
    }
    if (pattern[0] == '\0') strcpy(pattern, "*");

    PathCombineA(path, cmd->cwd, pattern);
    PathCanonicalizeA(path, path);

    {
        const char* fn = PathFindFileNameA(path);
        if (fn && *fn) {
            size_t bl = (size_t)(fn - path);
            if (bl > 0 && bl < sizeof(base)) {
                memcpy(base, path, bl);
                base[bl] = '\0';
            } else {
                strcpy(base, path);
            }
        } else {
            strcpy(base, path);
        }
    }
    if (PathIsDirectoryA(base)) {
        PathCombineA(path, base, "*");
    }

    fprintf(stdout, " Directory of %s\r\n\r\n", base);

    total_size = 0;
    file_count = 0;
    dir_count = 0;

    hFind = FindFirstFileA(path, fdbuf);
    found = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            DWORD fa;
            const char* fn;
            DWORD fsh, fsl;
            (void)attr;
            fa = *(DWORD*)fdbuf;
            fn = (const char*)((char*)fdbuf + 44);
            fsh = *(DWORD*)((char*)fdbuf + 28);
            fsl = *(DWORD*)((char*)fdbuf + 32);
            {
                char line[256];
                char datestr[64];
                char sizestr[32];
                const char* type_str;
                uint64_t sz = ((uint64_t)fsh << 32) | fsl;

                if (fa & FILE_ATTRIBUTE_DIRECTORY) {
                    type_str = "<DIR>";
                    sizestr[0] = '\0';
                    dir_count++;
                } else {
                    type_str = "     ";
                    snprintf(sizestr, sizeof(sizestr), "%llu", (unsigned long long)sz);
                    total_size += sz;
                    file_count++;
                }
                strcpy(datestr, "08/08/2026  00:00");
                snprintf(line, sizeof(line), "%s %-5s %12s %s\r\n",
                         datestr, type_str, sizestr, fn);
                cmd_write_stdout(line);
                found++;
            }
        } while (FindNextFileA(hFind, fdbuf));
        FindClose(hFind);
    }
    fprintf(stdout, "              %d File(s) %16llu bytes\r\n", file_count, (unsigned long long)total_size);
    fprintf(stdout, "              %d Dir(s)\r\n", dir_count);
    return 0;
}

static int cmd_builtin_type(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    char full[MAX_PATH];
    FILE* f;
    char buf[512];
    size_t n;
    (void)cmd;
    if (argc < 2) {
        cmd_write_stdout("The syntax of the command is incorrect.\r\n");
        return 1;
    }
    for (i = 1; i < argc; i++) {
        PathCombineA(full, cmd->cwd, argv[i]);
        PathCanonicalizeA(full, full);
        f = fopen(full, "r");
        if (f == NULL) {
            fprintf(stdout, "The system cannot find the file specified: %s\r\n", argv[i]);
            continue;
        }
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            fwrite(buf, 1, n, stdout);
        }
        fclose(f);
    }
    return 0;
}

static int cmd_builtin_copy(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    int ai;
    int overwrite = 1;
    char dst[MAX_PATH];
    char src[MAX_PATH];
    (void)cmd;
    ai = 1;
    for (ai = 1; ai < argc; ai++) {
        if (argv[ai][0] == '/') {
            if (strcasecmp_local(argv[ai], "/Y") == 0) overwrite = 1;
            else if (strcasecmp_local(argv[ai], "/-Y") == 0) overwrite = 0;
        } else break;
    }
    if (argc - ai < 2) {
        cmd_write_stdout("The system cannot find the file specified.\r\n");
        return 1;
    }
    strncpy(dst, argv[argc - 1], sizeof(dst) - 1);
    dst[sizeof(dst) - 1] = '\0';
    for (i = ai; i < argc - 1; i++) {
        PathCombineA(src, cmd->cwd, argv[i]);
        PathCanonicalizeA(src, src);
        {
            char final_dst[MAX_PATH];
            PathCombineA(final_dst, cmd->cwd, dst);
            if (PathDirectoryExistsA(final_dst)) {
                PathAppendA(final_dst, PathFindFileNameA(src));
            }
            PathCanonicalizeA(final_dst, final_dst);
            if (!CopyFileA(src, final_dst, !overwrite)) {
                cmd_write_stdout("File not found or cannot copy.\r\n");
                return 1;
            }
        }
    }
    return 0;
}

static int cmd_builtin_del(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    char full[MAX_PATH];
    (void)cmd;
    if (argc < 2) {
        cmd_write_stdout("Required parameter missing.\r\n");
        return 1;
    }
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '/') continue;
        PathCombineA(full, cmd->cwd, argv[i]);
        PathCanonicalizeA(full, full);
        {
            char fdbuf[4096];
            HANDLE hf;
            char dir_path[MAX_PATH];
            char file_path[MAX_PATH];
            const char* fn = PathFindFileNameA(full);
            size_t bl;
            if (fn && *fn && (strchr(fn, '*') || strchr(fn, '?'))) {
                bl = (size_t)(fn - full);
                memcpy(dir_path, full, bl);
                dir_path[bl] = '\0';
                hf = FindFirstFileA(full, fdbuf);
                if (hf != INVALID_HANDLE_VALUE) {
                    do {
                        const char* cfn = (const char*)((char*)fdbuf + 44);
                        if (strcmp(cfn, ".") == 0 || strcmp(cfn, "..") == 0) continue;
                        PathCombineA(file_path, dir_path, cfn);
                        DeleteFileA(file_path);
                    } while (FindNextFileA(hf, fdbuf));
                    FindClose(hf);
                }
            } else {
                DeleteFileA(full);
            }
        }
    }
    return 0;
}

static int cmd_builtin_set(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    int expr_mode = 0;
    int prompt_mode = 0;
    char* eq;

    if (argc <= 1) {
        for (i = 0; i < cmd->nvars; i++) {
            if (cmd->vars[i]) cmd_write_stdout_line(cmd->vars[i]);
        }
        return 0;
    }
    if (argc >= 2) {
        if (strcasecmp_local(argv[1], "/A") == 0) {
            if (argc < 3) return 1;
            {
                char name[256];
                int vi;
                int val;
                eq = strchr(argv[2], '=');
                if (eq) {
                    size_t nl = (size_t)(eq - argv[2]);
                    memcpy(name, argv[2], nl);
                    name[nl] = '\0';
                    vi = cmd_eval_expr(eq + 1);
                    val = vi;
                    {
                        char tmp[32];
                        snprintf(tmp, sizeof(tmp), "%d", val);
                        cmd_set_var(cmd, name, tmp);
                        cmd_write_stdout_line(tmp);
                    }
                }
            }
            return 0;
        } else if (strcasecmp_local(argv[1], "/P") == 0) {
            expr_mode = 0;
            prompt_mode = 1;
            (void)prompt_mode;
            return 0;
        }
    }
    {
        char buf[4096];
        buf[0] = '\0';
        for (i = 1; i < argc; i++) {
            if (i > 1) strcat(buf, " ");
            strcat(buf, argv[i]);
        }
        eq = strchr(buf, '=');
        if (eq == NULL) {
            const char* v = cmd_get_var(cmd, buf);
            if (v) {
                fprintf(stdout, "%s=%s\r\n", buf, v);
            } else {
                fprintf(stdout, "Environment variable %s not defined\r\n", buf);
            }
        } else {
            *eq = '\0';
            cmd_set_var(cmd, buf, eq + 1);
        }
    }
    return 0;
}

static int cmd_builtin_cls(cmd_state_t* cmd, int argc, char** argv) {
    (void)cmd; (void)argc; (void)argv;
    cmd_write_stdout("\x1B[2J\x1B[H");
    return 0;
}

static int cmd_builtin_ver(cmd_state_t* cmd, int argc, char** argv) {
    (void)cmd; (void)argc; (void)argv;
    cmd_write_stdout_line("KenuxK Version 1.0 [Kernel Build 19045.2006]");
    return 0;
}

static int cmd_builtin_date(cmd_state_t* cmd, int argc, char** argv) {
    uint64_t ft;
    (void)cmd; (void)argc; (void)argv;
    GetSystemTimeAsFileTime(&ft);
    cmd_write_stdout_line("Sat 08/08/2026");
    return 0;
}

static int cmd_builtin_time(cmd_state_t* cmd, int argc, char** argv) {
    uint64_t ft;
    (void)cmd; (void)argc; (void)argv;
    GetSystemTimeAsFileTime(&ft);
    cmd_write_stdout_line("The current time is:  0:00:00.00");
    return 0;
}

static int cmd_builtin_path(cmd_state_t* cmd, int argc, char** argv) {
    if (argc <= 1) {
        const char* p = cmd_get_var(cmd, "PATH");
        if (p) fprintf(stdout, "PATH=%s\r\n", p);
        else cmd_write_stdout_line("PATH=;");
        return 0;
    }
    {
        char buf[8192];
        int i;
        buf[0] = '\0';
        for (i = 1; i < argc; i++) {
            if (i > 1) strcat(buf, " ");
            strcat(buf, argv[i]);
        }
        cmd_set_var(cmd, "PATH", buf);
    }
    return 0;
}

static int cmd_builtin_prompt(cmd_state_t* cmd, int argc, char** argv) {
    if (argc <= 1) {
        cmd_set_var(cmd, "PROMPT", "$P$G");
        return 0;
    }
    {
        char buf[512];
        int i;
        buf[0] = '\0';
        for (i = 1; i < argc; i++) {
            if (i > 1) strcat(buf, " ");
            strcat(buf, argv[i]);
        }
        cmd_set_var(cmd, "PROMPT", buf);
    }
    return 0;
}

static int cmd_builtin_exit(cmd_state_t* cmd, int argc, char** argv) {
    int code = 0;
    int ai = 1;
    if (argc > ai && strcasecmp_local(argv[ai], "/B") == 0) ai++;
    if (argc > ai) code = cmd_atoi_safe(argv[ai]);
    cmd->errorlevel = code;
    return 9999;
}

static int cmd_builtin_title(cmd_state_t* cmd, int argc, char** argv) {
    (void)cmd; (void)argc; (void)argv;
    return 0;
}

static int cmd_builtin_pause(cmd_state_t* cmd, int argc, char** argv) {
    (void)cmd; (void)argc; (void)argv;
    cmd_write_stdout("Press any key to continue . . . ");
    fflush(stdout);
    {
        int c = fgetc(stdin);
        (void)c;
    }
    cmd_write_stdout("\r\n");
    return 0;
}

static int cmd_builtin_call(cmd_state_t* cmd, int argc, char** argv) {
    if (argc < 2) return 1;
    if (argv[1][0] == ':') {
        return 0;
    }
    cmd_execute_script(cmd, argv[1]);
    return 0;
}

static int cmd_builtin_find(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    int opt_v = 0, opt_c = 0, opt_n = 0, opt_i = 0;
    int ai;
    const char* needle;
    (void)cmd;
    ai = 1;
    for (ai = 1; ai < argc; ai++) {
        if (argv[ai][0] != '/') break;
        {
            const char* op = argv[ai] + 1;
            while (*op) {
                if (*op == 'v' || *op == 'V') opt_v = 1;
                if (*op == 'c' || *op == 'C') opt_c = 1;
                if (*op == 'n' || *op == 'N') opt_n = 1;
                if (*op == 'i' || *op == 'I') opt_i = 1;
                op++;
            }
        }
    }
    if (ai >= argc) return 1;
    needle = argv[ai++];
    for (i = ai; i < argc; i++) {
        FILE* f;
        char line[4096];
        int lno = 0;
        int matches = 0;
        char full[MAX_PATH];
        PathCombineA(full, cmd->cwd, argv[i]);
        PathCanonicalizeA(full, full);
        f = fopen(full, "r");
        if (!f) continue;
        fprintf(stdout, "---------- %s\r\n", argv[i]);
        while (fgets(line, sizeof(line), f)) {
            int found;
            char* p;
            lno++;
            if (opt_i) {
                p = stristr_local(line, needle);
            } else {
                p = strstr(line, needle);
            }
            found = (p != NULL);
            if (opt_v) found = !found;
            if (found) {
                matches++;
                if (!opt_c) {
                    if (opt_n) fprintf(stdout, "[%d]", lno);
                    fputs(line, stdout);
                    {
                        size_t ll = strlen(line);
                        if (ll == 0 || line[ll - 1] != '\n') fputc('\n', stdout);
                    }
                }
            }
        }
        if (opt_c) fprintf(stdout, "  %d\r\n", matches);
        fclose(f);
    }
    return 0;
}

static int strcmp_void(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static int cmd_builtin_sort(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    int rev = 0;
    FILE* f;
    char** lines;
    int count;
    int cap;
    char buf[4096];
    (void)cmd;
    lines = NULL; count = 0; cap = 0;
    f = stdin;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '/') {
            if (strchr(argv[i], 'r') || strchr(argv[i], 'R')) rev = 1;
        } else {
            char full[MAX_PATH];
            PathCombineA(full, cmd->cwd, argv[i]);
            PathCanonicalizeA(full, full);
            f = fopen(full, "r");
            if (!f) { f = stdin; continue; }
        }
    }
    while (fgets(buf, sizeof(buf), f)) {
        size_t ll = strlen(buf);
        if (count >= cap) {
            int new_cap = cap ? cap * 2 : 64;
            char** new_lines = (char**)memory_alloc(sizeof(char*) * (size_t)new_cap);
            if (new_lines == NULL) break;
            if (lines && count > 0) {
                memcpy(new_lines, lines, sizeof(char*) * (size_t)count);
                memory_free(lines);
            }
            lines = new_lines;
            cap = new_cap;
        }
        if (ll > 0 && buf[ll - 1] == '\n') { buf[ll - 1] = '\0'; ll--; }
        lines[count] = cmd_strdup_local(buf);
        count++;
    }
    if (count) {
        qsort(lines, count, sizeof(char*), strcmp_void);
        if (rev) {
            int j;
            for (i = 0, j = count - 1; i < j; i++, j--) {
                char* t = lines[i]; lines[i] = lines[j]; lines[j] = t;
            }
        }
        for (i = 0; i < count; i++) {
            fprintf(stdout, "%s\r\n", lines[i]);
            if (lines[i]) memory_free(lines[i]);
        }
    }
    if (lines) memory_free(lines);
    if (f && f != stdin) fclose(f);
    return 0;
}

static int cmd_builtin_more(cmd_state_t* cmd, int argc, char** argv) {
    FILE* f;
    char line[4096];
    int lines_shown = 0;
    int i;
    (void)cmd;
    f = stdin;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '/') {
            char full[MAX_PATH];
            PathCombineA(full, cmd->cwd, argv[i]);
            PathCanonicalizeA(full, full);
            f = fopen(full, "r");
            if (!f) f = stdin;
            break;
        }
    }
    while (fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
        lines_shown++;
        if (lines_shown % 24 == 0) {
            fprintf(stdout, "-- More --");
            fflush(stdout);
            {
                int c = fgetc(stdin);
                if (c == 'q') break;
            }
            fprintf(stdout, "\r\x1B[K");
        }
    }
    if (f && f != stdin) fclose(f);
    return 0;
}

static int cmd_builtin_help(cmd_state_t* cmd, int argc, char** argv) {
    int i;
    if (argc > 1) {
        int idx = cmd_find_builtin(argv[1]);
        if (idx >= 0) {
            fprintf(stdout, "  %s: %s\r\n", cmd_builtins[idx].name, cmd_builtins[idx].help);
        } else {
            fprintf(stdout, "This command is not supported by the help utility.\r\n");
        }
        return 0;
    }
    for (i = 0; cmd_builtins[i].name; i++) {
        int j;
        int dup = 0;
        for (j = 0; j < i; j++) {
            if (cmd_builtins[i].fn == cmd_builtins[j].fn) { dup = 1; break; }
        }
        if (dup) continue;
        fprintf(stdout, "  %-10s  %s\r\n", cmd_builtins[i].name, cmd_builtins[i].help);
    }
    return 0;
}

static int cmd_builtin_ren(cmd_state_t* cmd, int argc, char** argv) {
    char src[MAX_PATH], dst[MAX_PATH], srcdir[MAX_PATH];
    char final_dst[MAX_PATH];
    const char* dfn;
    (void)cmd;
    if (argc < 3) {
        cmd_write_stdout("The syntax of the command is incorrect.\r\n");
        return 1;
    }
    PathCombineA(src, cmd->cwd, argv[1]);
    PathCanonicalizeA(src, src);
    {
        const char* fn = PathFindFileNameA(src);
        if (fn) {
            size_t bl = (size_t)(fn - src);
            memcpy(srcdir, src, bl);
            srcdir[bl] = '\0';
        } else {
            strcpy(srcdir, "");
        }
    }
    dfn = PathFindFileNameA(argv[2]);
    PathCombineA(final_dst, srcdir, dfn ? dfn : argv[2]);
    PathCanonicalizeA(final_dst, final_dst);
    if (!MoveFileA(src, final_dst)) {
        cmd_write_stdout("The system cannot find the file specified.\r\n");
        return 1;
    }
    (void)dst;
    return 0;
}

static int cmd_builtin_move(cmd_state_t* cmd, int argc, char** argv) {
    char src[MAX_PATH], dst[MAX_PATH], final_dst[MAX_PATH];
    int overwrite = 1;
    int ai = 1;
    (void)cmd;
    for (ai = 1; ai < argc; ai++) {
        if (argv[ai][0] == '/') {
            if (strcasecmp_local(argv[ai], "/Y") == 0) overwrite = 1;
            else if (strcasecmp_local(argv[ai], "/-Y") == 0) overwrite = 0;
        } else break;
    }
    if (argc - ai < 2) return 1;
    PathCombineA(src, cmd->cwd, argv[ai]);
    PathCanonicalizeA(src, src);
    PathCombineA(dst, cmd->cwd, argv[argc - 1]);
    if (PathDirectoryExistsA(dst)) {
        PathAppendA(dst, PathFindFileNameA(src));
    }
    PathCanonicalizeA(dst, dst);
    if (!overwrite && PathFileExistsA(dst)) {
        fprintf(stdout, "Overwrite %s? (Yes/No/All): ", dst);
        return 0;
    }
    strcpy(final_dst, dst);
    if (!MoveFileA(src, final_dst)) {
        if (overwrite) {
            DeleteFileA(final_dst);
            MoveFileA(src, final_dst);
        }
    }
    return 0;
}

static int cmd_resolve_and_run(cmd_state_t* cmd, char** argv, int argc) {
    const char* cmdname;
    char search[MAX_PATH];
    char* path_env;
    char* pathext_env;
    char pathext[512];
    char path_copy[4096];
    char* ctx1;
    char* ctx2;
    char* p;
    char* pe;

    if (argc <= 0 || argv[0] == NULL) return -1;
    cmdname = argv[0];

    if (PathFileExistsA(cmdname)) {
        strncpy(search, cmdname, sizeof(search) - 1);
        search[sizeof(search) - 1] = '\0';
    } else {
        const char* fn = PathFindFileNameA(cmdname);
        if (fn && *fn != cmdname) {
            PathCombineA(search, cmd->cwd, cmdname);
            if (!PathFileExistsA(search)) return -1;
        } else {
            search[0] = '\0';
            path_env = (char*)cmd_get_var(cmd, "PATH");
            pathext_env = (char*)cmd_get_var(cmd, "PATHEXT");
            if (!pathext_env) pathext_env = ".COM;.EXE;.BAT;.CMD";

            strncpy(pathext, pathext_env, sizeof(pathext) - 1);
            pathext[sizeof(pathext) - 1] = '\0';

            {
                char fulltest[MAX_PATH];
                strncpy(path_copy, path_env ? path_env : ".", sizeof(path_copy) - 1);
                path_copy[sizeof(path_copy) - 1] = '\0';

                for (p = strtok_local_r(path_copy, ";", &ctx1); p; p = strtok_local_r(NULL, ";", &ctx1)) {
                    pe = pathext;
                    PathCombineA(fulltest, p, cmdname);
                    if (PathFileExistsA(fulltest)) {
                        strncpy(search, fulltest, sizeof(search) - 1);
                        search[sizeof(search) - 1] = '\0';
                        break;
                    }
                    {
                        char tmp[512];
                        char pe_buf[512];
                        char* p2;
                        strncpy(pe_buf, pe, sizeof(pe_buf) - 1);
                        pe_buf[sizeof(pe_buf) - 1] = '\0';
                        for (p2 = strtok_local_r(pe_buf, ";", &ctx2); p2; p2 = strtok_local_r(NULL, ";", &ctx2)) {
                            char te[64];
                            strncpy(te, p2, sizeof(te) - 1);
                            te[sizeof(te) - 1] = '\0';
                            PathRenameExtensionA(tmp, cmdname, te);
                            PathCombineA(fulltest, p, tmp);
                            if (PathFileExistsA(fulltest)) {
                                strncpy(search, fulltest, sizeof(search) - 1);
                                search[sizeof(search) - 1] = '\0';
                                break;
                            }
                        }
                    }
                    if (search[0]) break;
                }
            }
            if (!search[0]) {
                {
                    char fulltest[MAX_PATH];
                    PathCombineA(fulltest, cmd->cwd, cmdname);
                    if (PathFileExistsA(fulltest)) {
                        strncpy(search, fulltest, sizeof(search) - 1);
                        search[sizeof(search) - 1] = '\0';
                    }
                }
            }
        }
    }
    if (!search[0]) return -1;

    {
        const char* ext = PathFindExtensionA(search);
        if (ext && (strcasecmp_local(ext, ".bat") == 0 || strcasecmp_local(ext, ".cmd") == 0)) {
            cmd_execute_script(cmd, search);
            return 0;
        }
    }

    {
        void* si[16];
        void* pi[4];
        char cl[4096];
        int i;
        cl[0] = '\0';
        for (i = 0; i < argc; i++) {
            if (i > 0) strcat(cl, " ");
            if (strchr(argv[i], ' ')) {
                strcat(cl, "\"");
                strcat(cl, argv[i]);
                strcat(cl, "\"");
            } else {
                strcat(cl, argv[i]);
            }
        }
        memset(si, 0, sizeof(si));
        memset(pi, 0, sizeof(pi));
        if (CreateProcessA(search, cl, NULL, NULL, FALSE, 0, NULL, cmd->cwd, si, pi)) {
            return 0;
        }
    }
    return -1;
}

int cmd_execute_line(cmd_state_t* cmd, const char* line_in) {
    char line_exp[CMD_MAX_LINE];
    char line[CMD_MAX_LINE];
    char* p;
    size_t ll;
    char* argv[CMD_MAX_ARGS];
    int argc;
    int bi;
    int result;

    if (cmd == NULL || line_in == NULL) return -1;

    cmd_expand(cmd, line_in, line_exp, sizeof(line_exp));
    strncpy(line, line_exp, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    ll = strlen(line);
    while (ll > 0 && (line[ll - 1] == '\r' || line[ll - 1] == '\n' || line[ll - 1] == ' ' || line[ll - 1] == '\t')) {
        line[--ll] = '\0';
    }

    if (ll == 0) return 0;

    if (cmd->if_state.skip_flag && !cmd->if_state.enabled) {
        cmd->if_state.skip_flag = 0;
    }

    if (cmd->if_state.skip_flag) {
        return 0;
    }

    {
        int suppress_echo = 0;
        p = line;
        if (*p == '@') {
            suppress_echo = 1;
            p++;
        }

        if (cmd->echo_on && !suppress_echo && cmd->interactive) {
            fprintf(stdout, "%s\r\n", p);
            fflush(stdout);
        }
    }

    p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '@') p++;

    if (strncasecmp_local(p, "echo ", 5) == 0 || strcasecmp_local(p, "echo") == 0) {
    } else if (strncasecmp_local(p, "@echo ", 6) == 0 || strcasecmp_local(p, "@echo") == 0) {
        p++;
    }

    {
        int match_idx;
        match_idx = -1;
        if (strncasecmp_local(p, "if ", 3) == 0 || strncasecmp_local(p, "if\t", 3) == 0) {
            int negate = 0;
            char* cond = p + 3;
            while (*cond == ' ') cond++;
            if (strncasecmp_local(cond, "not ", 4) == 0) { negate = 1; cond += 4; while (*cond == ' ') cond++; }
            int cond_true = 0;
            if (strncasecmp_local(cond, "errorlevel ", 11) == 0) {
                int n;
                cond += 11;
                while (*cond == ' ') cond++;
                n = atoi(cond);
                cond_true = (cmd->errorlevel >= n);
            } else if (strncasecmp_local(cond, "exist ", 6) == 0) {
                char name[MAX_PATH];
                char* ep = cond + 6;
                char* np = name;
                while (*ep == ' ') ep++;
                while (*ep && *ep != ' ' && *ep != '\t' && (np - name) < MAX_PATH - 1) *np++ = *ep++;
                *np = '\0';
                {
                    char full[MAX_PATH];
                    PathCombineA(full, cmd->cwd, name);
                    cond_true = PathFileExistsA(full) || PathDirectoryExistsA(full);
                }
            } else {
                char s1[256], s2[256];
                int s1i = 0, s2i = 0;
                char* cp = cond;
                while (*cp && *cp != '=' && s1i < 255) s1[s1i++] = *cp++;
                s1[s1i] = '\0';
                while (s1i > 0 && s1[s1i - 1] == ' ') s1[--s1i] = '\0';
                while (*cp == '=') cp++;
                while (*cp && *cp != ' ' && *cp != '\t' && s2i < 255) s2[s2i++] = *cp++;
                s2[s2i] = '\0';
                cond_true = (strcasecmp_local(s1, s2) == 0);
            }
            if (negate) cond_true = !cond_true;
            {
                char* then_part = strchr(cond, '(');
                char* then_str = cond;
                if (!then_part) {
                    while (*then_str && !(*then_str == ' ' && then_str[1])) then_str++;
                    while (*then_str == ' ') then_str++;
                } else {
                    char* close = strchr(then_part, ')');
                    then_str = then_part + 1;
                    if (close) *close = '\0';
                }
                if (!cond_true) {
                    cmd->if_state.skip_flag = 1;
                }
                cmd->if_state.enabled = 1;
                if (cond_true && *then_str) {
                    return cmd_execute_line(cmd, then_str);
                }
                return 0;
            }
        }

        if (strncasecmp_local(p, "goto ", 5) == 0 || strcasecmp_local(p, "goto") == 0) {
            char* lbl = p + 4;
            while (*lbl == ' ') lbl++;
            if (*lbl == ':') lbl++;
            strncpy(cmd->goto_state.target_label, lbl, sizeof(cmd->goto_state.target_label) - 1);
            cmd->goto_state.enabled = 1;
            return 0;
        }

        if (strncasecmp_local(p, "for ", 4) == 0 || strcasecmp_local(p, "for") == 0) {
            return 0;
        }

        if (strncasecmp_local(p, ":", 1) == 0) {
            return 0;
        }
    }

    argc = cmd_tokenize(p, argv, CMD_MAX_ARGS);
    if (argc == 0) return 0;

    {
        char upname[256];
        strncpy(upname, argv[0], sizeof(upname) - 1);
        upname[sizeof(upname) - 1] = '\0';
        str_to_upper(upname);
        if (strcmp(upname, "REM") == 0) return 0;
    }

    bi = cmd_find_builtin(argv[0]);
    if (bi >= 0) {
        result = cmd_builtins[bi].fn(cmd, argc, argv);
        if (result == 9999) return result;
        cmd->errorlevel = result;
        return result;
    }

    result = cmd_resolve_and_run(cmd, argv, argc);
    if (result < 0) {
        fprintf(stdout, "'%s' is not recognized as an internal or external command,\r\noperable program or batch file.\r\n", argv[0]);
        cmd->errorlevel = 9009;
        return 9009;
    }
    return 0;
}

int cmd_execute_script(cmd_state_t* cmd, const char* path) {
    FILE* f;
    char line[CMD_MAX_LINE];
    char lines[1024][CMD_MAX_LINE];
    cmd_label_t labels[CMD_MAX_LABELS];
    int nlines;
    int nlabels;
    int i;
    int pc;
    char full[MAX_PATH];
    int rc;

    if (cmd == NULL || path == NULL) return -1;

    PathCombineA(full, cmd->cwd, path);
    PathCanonicalizeA(full, full);

    f = fopen(full, "r");
    if (f == NULL) {
        fprintf(stdout, "The system cannot find the batch file: %s\r\n", path);
        return 1;
    }

    nlines = 0;
    nlabels = 0;
    while (fgets(line, sizeof(line), f) && nlines < 1024) {
        size_t ll = strlen(line);
        while (ll > 0 && (line[ll - 1] == '\r' || line[ll - 1] == '\n')) {
            line[--ll] = '\0';
        }
        strncpy(lines[nlines], line, CMD_MAX_LINE - 1);
        lines[nlines][CMD_MAX_LINE - 1] = '\0';

        {
            char* p = lines[nlines];
            while (*p == ' ' || *p == '\t') p++;
            if (*p == ':') {
                char* lbl = p + 1;
                if (nlabels < CMD_MAX_LABELS) {
                    strncpy(labels[nlabels].label, lbl, sizeof(labels[nlabels].label) - 1);
                    labels[nlabels].label[sizeof(labels[nlabels].label) - 1] = '\0';
                    {
                        size_t tl = strlen(labels[nlabels].label);
                        while (tl > 0 && (labels[nlabels].label[tl - 1] == ' ' || labels[nlabels].label[tl - 1] == '\t')) {
                            labels[nlabels].label[--tl] = '\0';
                        }
                    }
                    labels[nlabels].line_number = nlines;
                    nlabels++;
                }
            }
        }
        nlines++;
    }
    fclose(f);

    cmd->recurse_depth++;
    pc = 0;
    rc = 0;
    while (pc < nlines) {
        if (cmd->goto_state.enabled) {
            int found = -1;
            for (i = 0; i < nlabels; i++) {
                if (strcasecmp_local(labels[i].label, cmd->goto_state.target_label) == 0) {
                    found = labels[i].line_number;
                    break;
                }
            }
            cmd->goto_state.enabled = 0;
            if (found >= 0) {
                pc = found + 1;
                continue;
            } else {
                pc = nlines;
                break;
            }
        }

        rc = cmd_execute_line(cmd, lines[pc]);
        if (rc == 9999) {
            rc = cmd->errorlevel;
            break;
        }
        pc++;
    }
    cmd->recurse_depth--;

    return rc;
}

void cmd_print_prompt(cmd_state_t* cmd) {
    const char* prompt;
    const char* p;
    if (cmd == NULL) return;
    prompt = cmd_get_var(cmd, "PROMPT");
    if (prompt == NULL) prompt = "$P$G";
    p = prompt;
    while (*p) {
        if (*p == '$') {
            p++;
            switch (*p) {
                case 'P': case 'p':
                    fprintf(stdout, "%s", cmd->cwd);
                    break;
                case 'G': case 'g':
                    fputc('>', stdout);
                    break;
                case 'L': case 'l':
                    fputc('<', stdout);
                    break;
                case 'B': case 'b':
                    fputc('|', stdout);
                    break;
                case 'Q': case 'q':
                    fputc('=', stdout);
                    break;
                case '$':
                    fputc('$', stdout);
                    break;
                case 'T': case 't':
                    fprintf(stdout, "0:00:00.00");
                    break;
                case 'D': case 'd':
                    fprintf(stdout, "Sat 08/08/2026");
                    break;
                case 'V': case 'v':
                    fprintf(stdout, "KenuxK 1.0");
                    break;
                case '_':
                    fprintf(stdout, "\r\n");
                    break;
                case 0: continue;
                default:
                    fputc(*p, stdout);
                    break;
            }
            if (*p) p++;
        } else {
            fputc(*p, stdout);
            p++;
        }
    }
    fflush(stdout);
}

int cmd_interactive(cmd_state_t* cmd) {
    char line[CMD_MAX_LINE];
    int rc;
    if (cmd == NULL) return -1;
    cmd->interactive = 1;
    for (;;) {
        cmd_print_prompt(cmd);
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        rc = cmd_execute_line(cmd, line);
        if (rc == 9999) break;
    }
    return 0;
}

int cmd_main(int argc, char** argv) {
    cmd_state_t cmd;
    int rc;
    cmd_init(&cmd, (argc <= 1) ? 1 : 0);
    if (argc > 1) {
        int ai = 1;
        int interactive_fallback = 0;
        if (argc > ai && strcasecmp_local(argv[ai], "/C") == 0) {
            ai++;
            if (argc > ai) {
                char buf[4096];
                int bi;
                buf[0] = '\0';
                for (bi = ai; bi < argc; bi++) {
                    if (bi > ai) strcat(buf, " ");
                    strcat(buf, argv[bi]);
                }
                rc = cmd_execute_line(&cmd, buf);
                cmd_destroy(&cmd);
                return cmd.errorlevel;
            }
        } else if (argc > ai && strcasecmp_local(argv[ai], "/K") == 0) {
            ai++;
            if (argc > ai) {
                char buf[4096];
                int bi;
                buf[0] = '\0';
                for (bi = ai; bi < argc; bi++) {
                    if (bi > ai) strcat(buf, " ");
                    strcat(buf, argv[bi]);
                }
                cmd_execute_line(&cmd, buf);
            }
            interactive_fallback = 1;
        } else {
            const char* p = argv[ai];
            const char* ext = PathFindExtensionA(p);
            if (ext && (strcasecmp_local(ext, ".bat") == 0 || strcasecmp_local(ext, ".cmd") == 0)) {
                rc = cmd_execute_script(&cmd, p);
                cmd_destroy(&cmd);
                return rc;
            } else {
                char buf[4096];
                int bi;
                buf[0] = '\0';
                for (bi = ai; bi < argc; bi++) {
                    if (bi > ai) strcat(buf, " ");
                    strcat(buf, argv[bi]);
                }
                cmd_execute_line(&cmd, buf);
            }
        }
        if (interactive_fallback) {
            cmd_interactive(&cmd);
        }
    } else {
        cmd_interactive(&cmd);
    }
    rc = cmd.errorlevel;
    cmd_destroy(&cmd);
    return rc;
}

typedef struct _STARTUPINFOA_LOCAL {
    DWORD  cb;
    char*  lpReserved;
    char*  lpDesktop;
    char*  lpTitle;
    DWORD  dwX;
    DWORD  dwY;
    DWORD  dwXSize;
    DWORD  dwYSize;
    DWORD  dwXCountChars;
    DWORD  dwYCountChars;
    DWORD  dwFillAttribute;
    DWORD  dwFlags;
    WORD   wShowWindow;
    WORD   cbReserved2;
    BYTE*  lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOA_LOCAL;

typedef struct _PROCESS_INFORMATION_LOCAL {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION_LOCAL;

typedef struct {
    uint32_t pid;
} process_obj_t;

BOOL CreateProcessA(const char* lpApplicationName,
                    char* lpCommandLine,
                    void* lpProcessAttributes,
                    void* lpThreadAttributes,
                    BOOL bInheritHandles,
                    DWORD dwCreationFlags,
                    LPVOID lpEnvironment,
                    const char* lpCurrentDirectory,
                    void* lpStartupInfo,
                    void* lpProcessInformation)
{
    const char* appname;
    const char* curdir;
    win32_process_t* proc;
    process_obj_t* pobj;
    HANDLE ph;
    STARTUPINFOA_LOCAL* si;
    PROCESS_INFORMATION_LOCAL* pi;
    (void)lpProcessAttributes;
    (void)lpThreadAttributes;
    (void)bInheritHandles;
    (void)dwCreationFlags;
    (void)lpEnvironment;
    (void)lpStartupInfo;
    (void)lpProcessInformation;

    si = (STARTUPINFOA_LOCAL*)lpStartupInfo;
    pi = (PROCESS_INFORMATION_LOCAL*)lpProcessInformation;

    if (lpApplicationName && *lpApplicationName) {
        appname = lpApplicationName;
    } else if (lpCommandLine) {
        static char buf[MAX_PATH];
        const char* p = lpCommandLine;
        char* d = buf;
        int quoted = 0;
        while (*p == ' ') p++;
        if (*p == '"') {
            quoted = 1;
            p++;
        }
        while (*p && (d - buf) < MAX_PATH - 1) {
            if (quoted) {
                if (*p == '"') break;
            } else {
                if (*p == ' ') break;
            }
            *d++ = *p++;
        }
        *d = '\0';
        appname = buf;
    } else {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    curdir = lpCurrentDirectory;
    if (curdir == NULL) {
        curdir = "C:\\";
    }

    proc = win32_create_process(appname, lpCommandLine ? lpCommandLine : appname, curdir);
    if (proc == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    pobj = (process_obj_t*)memory_alloc(sizeof(process_obj_t));
    if (pobj == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    pobj->pid = proc->pid;

    ph = win32_handle_alloc(pobj, HANDLE_OBJECT_TYPE_PROCESS2, GENERIC_ALL);
    if (ph == NULL) {
        memory_free(pobj);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    if (pi) {
        pi->hProcess = ph;
        pi->hThread = win32_handle_alloc(NULL, 5, GENERIC_ALL);
        pi->dwProcessId = proc->pid;
        pi->dwThreadId = 1;
    }
    (void)si;
    SetLastError(0);
    return TRUE;
}

BOOL CreateProcessAsUserA(HANDLE hToken,
                          const char* lpApplicationName,
                          char* lpCommandLine,
                          void* lpProcessAttributes,
                          void* lpThreadAttributes,
                          BOOL bInheritHandles,
                          DWORD dwCreationFlags,
                          LPVOID lpEnvironment,
                          const char* lpCurrentDirectory,
                          void* lpStartupInfo,
                          void* lpProcessInformation)
{
    (void)hToken;
    return CreateProcessA(lpApplicationName, lpCommandLine, lpProcessAttributes,
                          lpThreadAttributes, bInheritHandles, dwCreationFlags,
                          lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                          lpProcessInformation);
}

BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode) {
    process_obj_t* pobj;
    win32_process_t* cproc;
    uint32_t cpid;
    if (hProcess == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    cpid = GetCurrentProcessId();
    cproc = win32_get_process(cpid);
    if (hProcess == GetCurrentProcess()) {
        if (cproc) {
            cproc->exit_code = uExitCode;
            cproc->is_running = FALSE;
        }
        return TRUE;
    }
    pobj = (process_obj_t*)win32_handle_to_object(hProcess, HANDLE_OBJECT_TYPE_PROCESS2);
    if (pobj == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    win32_terminate_process(pobj->pid, (uint32_t)uExitCode);
    return TRUE;
}

BOOL GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode) {
    process_obj_t* pobj;
    win32_process_t* proc;
    uint32_t cpid;
    if (lpExitCode == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (hProcess == GetCurrentProcess()) {
        cpid = GetCurrentProcessId();
        proc = win32_get_process(cpid);
        *lpExitCode = proc ? proc->exit_code : 0;
        return TRUE;
    }
    pobj = (process_obj_t*)win32_handle_to_object(hProcess, HANDLE_OBJECT_TYPE_PROCESS2);
    if (pobj == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    proc = win32_get_process(pobj->pid);
    if (proc == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *lpExitCode = proc->exit_code;
    return TRUE;
}

BOOL GetProcessTimes(HANDLE hProcess,
                     void* lpCreationTime,
                     void* lpExitTime,
                     void* lpKernelTime,
                     void* lpUserTime)
{
    process_obj_t* pobj;
    win32_process_t* proc;
    uint32_t cpid;
    if (hProcess == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (lpCreationTime) *(uint64_t*)lpCreationTime = 0ULL;
    if (lpExitTime) *(uint64_t*)lpExitTime = 0ULL;
    if (lpKernelTime) *(uint64_t*)lpKernelTime = 0ULL;
    if (lpUserTime) *(uint64_t*)lpUserTime = 0ULL;
    if (hProcess == GetCurrentProcess()) {
        cpid = GetCurrentProcessId();
        proc = win32_get_process(cpid);
    } else {
        pobj = (process_obj_t*)win32_handle_to_object(hProcess, HANDLE_OBJECT_TYPE_PROCESS2);
        if (pobj == NULL) {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }
        proc = win32_get_process(pobj->pid);
    }
    if (proc) {
        if (lpCreationTime) {
            *(uint64_t*)lpCreationTime = ((uint64_t)proc->creation_time_high << 32) | proc->creation_time_low;
        }
        if (lpKernelTime) *(uint64_t*)lpKernelTime = proc->kernel_time;
        if (lpUserTime) *(uint64_t*)lpUserTime = proc->user_time;
    }
    return TRUE;
}

typedef struct {
    int side;
} pipe_obj_t;

BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe,
                void* lpPipeAttributes, DWORD nSize)
{
    pipe_obj_t* pr;
    pipe_obj_t* pw;
    (void)lpPipeAttributes;
    (void)nSize;
    if (hReadPipe == NULL || hWritePipe == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    pr = (pipe_obj_t*)memory_alloc(sizeof(pipe_obj_t));
    pw = (pipe_obj_t*)memory_alloc(sizeof(pipe_obj_t));
    if (pr == NULL || pw == NULL) {
        if (pr) memory_free(pr);
        if (pw) memory_free(pw);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    pr->side = 0;
    pw->side = 1;
    *hReadPipe = win32_handle_alloc(pr, HANDLE_OBJECT_TYPE_PIPE, GENERIC_READ);
    *hWritePipe = win32_handle_alloc(pw, HANDLE_OBJECT_TYPE_PIPE, GENERIC_WRITE);
    if (*hReadPipe == NULL || *hWritePipe == NULL) {
        CloseHandle(*hReadPipe);
        CloseHandle(*hWritePipe);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    return TRUE;
}

BOOL DuplicateHandle(HANDLE hSourceProcessHandle,
                     HANDLE hSourceHandle,
                     HANDLE hTargetProcessHandle,
                     PHANDLE lpTargetHandle,
                     DWORD dwDesiredAccess,
                     BOOL bInheritHandle, DWORD dwOptions)
{
    uint32_t nh;
    (void)hSourceProcessHandle;
    (void)hTargetProcessHandle;
    (void)dwDesiredAccess;
    (void)bInheritHandle;
    (void)dwOptions;
    if (lpTargetHandle == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    nh = win32_handle_duplicate(hSourceHandle);
    if (nh == 0) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    *lpTargetHandle = (HANDLE)(uintptr_t)nh;
    return TRUE;
}

int shell32_init(void) {
    memset(g_tray_icons, 0, sizeof(g_tray_icons));
    g_tray_icon_count = 0;
    memset(&g_drag_data, 0, sizeof(g_drag_data));
    g_dragging = FALSE;
    return 0;
}

int cmd_init_global(void) {
    memset(g_cmd_global_vars, 0, sizeof(g_cmd_global_vars));
    g_cmd_global_var_count = 0;
    return 0;
}
