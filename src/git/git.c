/*
 * Kenux OS - Git Version Control System (Minimal Implementation)
 * Main git functionality
 */

#include "git.h"

/* --- Simple SHA-1 implementation --- */
typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

static void SHA1Transform(uint32_t state[5], const unsigned char buffer[64]);

static void SHA1Init(SHA1_CTX *context) {
    context->count[0] = context->count[1] = 0;
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
}

static void SHA1Update(SHA1_CTX *context, const unsigned char *data, uint32_t len) {
    uint32_t i, j;
    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
    context->count[1] += (len >> 29);
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64 - j));
        SHA1Transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64) {
            SHA1Transform(context->state, &data[i]);
        }
        j = 0;
    } else {
        i = 0;
    }
    memcpy(&context->buffer[j], &data[i], len - i);
}

static void SHA1Final(unsigned char digest[20], SHA1_CTX *context) {
    unsigned char finalcount[8];
    unsigned char c;
    uint32_t i;
    for (i = 0; i < 8; i++)
        finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
    c = 0200;
    SHA1Update(context, &c, 1);
    while ((context->count[0] & 504) != 448) {
        c = 0000;
        SHA1Update(context, &c, 1);
    }
    SHA1Update(context, finalcount, 8);
    for (i = 0; i < 20; i++)
        digest[i] = (unsigned char)((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    memset(context, 0, sizeof(*context));
}

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
static void SHA1Transform(uint32_t state[5], const unsigned char buffer[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    uint32_t w[80], i, t;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)buffer[i * 4] << 24) | ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) | (uint32_t)buffer[i * 4 + 3];
    for (i = 16; i < 80; i++)
        w[i] = ROTL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
#define F1(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define F2(x, y, z) ((x) ^ (y) ^ (z))
#define F3(x, y, z) (((x) & (y)) | ((z) & ((x) | (y))))
    for (i = 0; i < 20; i++) { t = ROTL(a, 5) + F1(b, c, d) + e + w[i] + 0x5A827999; e = d; d = c; c = ROTL(b, 30); b = a; a = t; }
    for (; i < 40; i++) { t = ROTL(a, 5) + F2(b, c, d) + e + w[i] + 0x6ED9EBA1; e = d; d = c; c = ROTL(b, 30); b = a; a = t; }
    for (; i < 60; i++) { t = ROTL(a, 5) + F3(b, c, d) + e + w[i] + 0x8F1BBCDC; e = d; d = c; c = ROTL(b, 30); b = a; a = t; }
    for (; i < 80; i++) { t = ROTL(a, 5) + F2(b, c, d) + e + w[i] + 0xCA62C1D6; e = d; d = c; c = ROTL(b, 30); b = a; a = t; }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

/* --- Utilities --- */
int compute_sha1(const unsigned char *data, size_t len, unsigned char *out_hash) {
    SHA1_CTX ctx;
    SHA1Init(&ctx);
    SHA1Update(&ctx, data, (uint32_t)len);
    SHA1Final(out_hash, &ctx);
    return 0;
}

void sha1_to_hex(const unsigned char *hash, char *out_hex) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        out_hex[i * 2] = hex[(hash[i] >> 4) & 0x0F];
        out_hex[i * 2 + 1] = hex[hash[i] & 0x0F];
    }
    out_hex[40] = '\0';
}

int hex_to_sha1(const char *hex, unsigned char *out_hash) {
    if (strlen(hex) < 40) return -1;
    for (int i = 0; i < 20; i++) {
        char byte[3] = {hex[i * 2], hex[i * 2 + 1], 0};
        out_hash[i] = (unsigned char)strtoul(byte, NULL, 16);
    }
    return 0;
}

int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

int is_regular_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

char *read_entire_file(const char *path, size_t *out_size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return NULL; }
    size_t size = (size_t)st.st_size;
    char *buf = malloc(size + 1);
    if (!buf) { close(fd); return NULL; }
    ssize_t total = 0;
    while ((size_t)total < size) {
        ssize_t r = read(fd, buf + total, size - (size_t)total);
        if (r <= 0) break;
        total += (size_t)r;
    }
    close(fd);
    buf[total] = 0;
    if (out_size) *out_size = (size_t)total;
    return buf;
}

int write_entire_file(const char *path, const unsigned char *data, size_t size, mode_t mode) {
    char tmp[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) return -1;
    size_t total = 0;
    while (total < size) {
        ssize_t w = write(fd, data + total, size - total);
        if (w <= 0) { close(fd); unlink(tmp); return -1; }
        total += (size_t)w;
    }
    close(fd);
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

int dir_mkdir_p(const char *path, mode_t mode) {
    char tmp[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

const char *object_type_name(ObjectType type) {
    switch (type) {
        case OBJ_BLOB: return "blob";
        case OBJ_TREE: return "tree";
        case OBJ_COMMIT: return "commit";
        case OBJ_TAG: return "tag";
        default: return "unknown";
    }
}

ObjectType object_type_from_name(const char *name) {
    if (!name) return OBJ_UNKNOWN;
    if (strcmp(name, "blob") == 0) return OBJ_BLOB;
    if (strcmp(name, "tree") == 0) return OBJ_TREE;
    if (strcmp(name, "commit") == 0) return OBJ_COMMIT;
    if (strcmp(name, "tag") == 0) return OBJ_TAG;
    return OBJ_UNKNOWN;
}

/* --- Core git state management --- */
void git_init_state(GitState *state) {
    memset(state, 0, sizeof(*state));
}

void git_cleanup_state(GitState *state) {
    /* nothing dynamic allocated directly in state */
    (void)state;
}

int git_find_repo(GitState *state, const char *start_path) {
    char path[MAX_PATH_LEN];
    if (!start_path) start_path = ".";
    if (!realpath(start_path, path)) return -1;
    while (1) {
        char gitdir[MAX_PATH_LEN];
        snprintf(gitdir, sizeof(gitdir), "%s/%s", path, GIT_DIR);
        if (is_directory(gitdir)) {
            strncpy(state->repo_root, path, MAX_PATH_LEN - 1);
            strncpy(state->git_dir, gitdir, MAX_PATH_LEN - 1);
            state->initialized = 1;
            /* read HEAD */
            char head_file[MAX_PATH_LEN];
            snprintf(head_file, sizeof(head_file), "%s/HEAD", gitdir);
            char *content = read_entire_file(head_file, NULL);
            if (content) {
                content[strcspn(content, "\r\n")] = 0;
                if (strncmp(content, "ref: ", 5) == 0) {
                    strncpy(state->head_ref, content + 5, MAX_PATH_LEN - 1);
                    char refpath[MAX_PATH_LEN];
                    snprintf(refpath, sizeof(refpath), "%s/%s", gitdir, state->head_ref);
                    char *sha = read_entire_file(refpath, NULL);
                    if (sha) {
                        sha[strcspn(sha, "\r\n \t")] = 0;
                        strncpy(state->head_sha, sha, MAX_SHA_LEN - 1);
                        free(sha);
                    }
                } else if (strlen(content) >= 40) {
                    strncpy(state->head_sha, content, MAX_SHA_LEN - 1);
                }
                free(content);
            }
            return 0;
        }
        /* move up */
        char *slash = strrchr(path, '/');
        if (!slash) break;
        if (slash == path) break; /* root */
        *slash = 0;
    }
    return -1;
}

int git_is_inside_work_tree(GitState *state) {
    return state->initialized;
}

int git_init_repo(const char *path, int bare) {
    char root[MAX_PATH_LEN];
    if (!path) path = ".";
    if (realpath(path, root) == NULL) {
        /* create if not exists */
        if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
        if (realpath(path, root) == NULL) return -1;
    }
    char gitdir[MAX_PATH_LEN];
    if (bare) {
        snprintf(gitdir, sizeof(gitdir), "%s", root);
    } else {
        snprintf(gitdir, sizeof(gitdir), "%s/%s", root, GIT_DIR);
    }
    if (dir_mkdir_p(gitdir, 0755) != 0) return -1;
    char buf[MAX_PATH_LEN];
    snprintf(buf, sizeof(buf), "%s/objects", gitdir);
    dir_mkdir_p(buf, 0755);
    /* create xx subdirs for objects */
    for (int i = 0; i < 256; i++) {
        char sub[MAX_PATH_LEN];
        snprintf(sub, sizeof(sub), "%s/objects/%02x", gitdir, i);
        mkdir(sub, 0755);
    }
    snprintf(buf, sizeof(buf), "%s/refs/heads", gitdir);
    dir_mkdir_p(buf, 0755);
    snprintf(buf, sizeof(buf), "%s/refs/tags", gitdir);
    dir_mkdir_p(buf, 0755);
    snprintf(buf, sizeof(buf), "%s/logs", gitdir);
    dir_mkdir_p(buf, 0755);
    /* HEAD */
    snprintf(buf, sizeof(buf), "%s/HEAD", gitdir);
    write_entire_file(buf, (const unsigned char *)"ref: refs/heads/master\n",
                     strlen("ref: refs/heads/master\n"), 0644);
    /* config */
    snprintf(buf, sizeof(buf), "%s/config", gitdir);
    const char *config_content =
        "[core]\n\trepositoryformatversion = 0\n\tfilemode = true\n\tbare = ";
    char config[MAX_PATH_LEN * 2];
    snprintf(config, sizeof(config), "%s%s\n", config_content, bare ? "true" : "false");
    write_entire_file(buf, (const unsigned char *)config, strlen(config), 0644);
    /* description */
    snprintf(buf, sizeof(buf), "%s/description", gitdir);
    write_entire_file(buf, (const unsigned char *)
        "Unnamed repository; edit this file 'description' to name the repository.\n",
        84, 0644);
    printf("Initialized empty KenuxGit repository in %s/\n", gitdir);
    return 0;
}

/* --- Object database --- */
int git_hash_object(const unsigned char *data, size_t len, ObjectType type, char *out_sha) {
    const char *tname = object_type_name(type);
    char header[64];
    int hlen = snprintf(header, sizeof(header), "%s %zu", tname, len);
    if (hlen < 0) return -1;
    SHA1_CTX ctx;
    SHA1Init(&ctx);
    SHA1Update(&ctx, (const unsigned char *)header, (uint32_t)hlen);
    SHA1Update(&ctx, (const unsigned char *)"\0", 1);
    SHA1Update(&ctx, data, (uint32_t)len);
    unsigned char hash[20];
    SHA1Final(hash, &ctx);
    sha1_to_hex(hash, out_sha);
    return 0;
}

static char *object_path_for(GitState *state, const char *sha, char *out) {
    snprintf(out, MAX_PATH_LEN, "%s/objects/%c%c/%s",
             state->git_dir, sha[0], sha[1], sha + 2);
    return out;
}

int git_write_object(GitState *state, const GitObject *obj) {
    if (!state->initialized) return -1;
    char path[MAX_PATH_LEN];
    object_path_for(state, obj->sha, path);
    if (path_exists(path)) return 0; /* already exists */
    const char *tname = object_type_name(obj->type);
    /* build content: header + \0 + data (no zlib for minimal impl) */
    size_t hdrlen = (size_t)snprintf(NULL, 0, "%s %zu", tname, obj->size) + 1;
    size_t total = hdrlen + obj->size;
    unsigned char *buf = malloc(total);
    if (!buf) return -1;
    snprintf((char *)buf, hdrlen + 1, "%s %zu", tname, obj->size);
    buf[hdrlen - 1] = 0;
    memcpy(buf + hdrlen, obj->data, obj->size);
    /* ensure dir */
    char dir[MAX_PATH_LEN];
    snprintf(dir, sizeof(dir), "%s/objects/%c%c", state->git_dir, obj->sha[0], obj->sha[1]);
    dir_mkdir_p(dir, 0755);
    int rc = write_entire_file(path, buf, total, 0444);
    free(buf);
    return rc;
}

int git_read_object(GitState *state, const char *sha, GitObject *obj) {
    if (!state->initialized || !sha || strlen(sha) < 40) return -1;
    char path[MAX_PATH_LEN];
    object_path_for(state, sha, path);
    size_t size;
    char *content = read_entire_file(path, &size);
    if (!content) return -1;
    /* parse header: "<type> <size>\0<data>" */
    const char *type_end = strchr(content, ' ');
    if (!type_end) { free(content); return -1; }
    char type_name[32];
    size_t tlen = (size_t)(type_end - content);
    if (tlen >= sizeof(type_name)) tlen = sizeof(type_name) - 1;
    memcpy(type_name, content, tlen);
    type_name[tlen] = 0;
    obj->type = object_type_from_name(type_name);
    /* size */
    char *endp;
    unsigned long sz = strtoul(type_end + 1, &endp, 10);
    if (*endp != 0) { free(content); return -1; }
    endp++; /* skip NUL */
    if ((size_t)(endp - content) > size) { free(content); return -1; }
    obj->size = sz;
    obj->data = malloc(sz ? sz : 1);
    if (!obj->data) { free(content); return -1; }
    memcpy(obj->data, endp, sz);
    strncpy(obj->sha, sha, MAX_SHA_LEN - 1);
    free(content);
    return 0;
}

int git_object_exists(GitState *state, const char *sha) {
    if (!state->initialized || !sha || strlen(sha) < 40) return 0;
    char path[MAX_PATH_LEN];
    object_path_for(state, sha, path);
    return path_exists(path);
}

void git_free_object(GitObject *obj) {
    if (obj && obj->data) { free(obj->data); obj->data = NULL; }
}

/* --- Blob operations --- */
int git_create_blob_from_file(GitState *state, const char *path, char *out_sha) {
    size_t size;
    char *data = read_entire_file(path, &size);
    if (!data) return -1;
    GitObject obj;
    memset(&obj, 0, sizeof(obj));
    obj.type = OBJ_BLOB;
    obj.size = size;
    obj.data = (unsigned char *)data;
    git_hash_object(obj.data, obj.size, obj.type, obj.sha);
    int rc = git_write_object(state, &obj);
    if (rc == 0 && out_sha) strncpy(out_sha, obj.sha, MAX_SHA_LEN - 1);
    free(data);
    return rc;
}

int git_write_blob_to_file(GitState *state, const char *sha, const char *path) {
    GitObject obj;
    if (git_read_object(state, sha, &obj) != 0) return -1;
    if (obj.type != OBJ_BLOB) { git_free_object(&obj); return -1; }
    int rc = write_entire_file(path, obj.data, obj.size, 0644);
    git_free_object(&obj);
    return rc;
}

/* --- Tree operations --- */
int git_create_tree_from_entries(GitState *state, const TreeEntry *entries, int count, char *out_sha) {
    /* tree format: "<mode> <name>\0<20-byte sha>" binary */
    size_t capacity = 4096, pos = 0;
    unsigned char *buf = malloc(capacity);
    if (!buf) return -1;
    for (int i = 0; i < count; i++) {
        char mode_str[16];
        snprintf(mode_str, sizeof(mode_str), "%06o", entries[i].mode & 0777777);
        /* use 4 or 6 digit mode? git uses 6 (40000 dir) */
        int mlen = (int)strlen(mode_str);
        int nlen = (int)strlen(entries[i].name);
        size_t need = (size_t)(mlen + 1 + nlen + 1 + 20);
        if (pos + need > capacity) {
            capacity *= 2;
            buf = realloc(buf, capacity);
            if (!buf) return -1;
        }
        memcpy(buf + pos, mode_str, (size_t)mlen); pos += (size_t)mlen;
        buf[pos++] = ' ';
        memcpy(buf + pos, entries[i].name, (size_t)nlen); pos += (size_t)nlen;
        buf[pos++] = 0;
        unsigned char sha_bin[20];
        hex_to_sha1(entries[i].sha, sha_bin);
        memcpy(buf + pos, sha_bin, 20); pos += 20;
    }
    GitObject obj;
    memset(&obj, 0, sizeof(obj));
    obj.type = OBJ_TREE;
    obj.size = pos;
    obj.data = buf;
    git_hash_object(obj.data, obj.size, obj.type, obj.sha);
    int rc = git_write_object(state, &obj);
    if (rc == 0 && out_sha) strncpy(out_sha, obj.sha, MAX_SHA_LEN - 1);
    free(buf);
    return rc;
}

int git_parse_tree(const GitObject *obj, TreeEntry *entries, int *out_count) {
    if (obj->type != OBJ_TREE) return -1;
    size_t pos = 0;
    int count = 0;
    while (pos < obj->size) {
        /* mode string until space */
        char mode_str[16];
        size_t i = 0;
        while (pos < obj->size && obj->data[pos] != ' ' && i < sizeof(mode_str) - 1) {
            mode_str[i++] = (char)obj->data[pos++];
        }
        mode_str[i] = 0;
        if (pos >= obj->size || obj->data[pos] != ' ') return -1;
        pos++; /* skip space */
        /* name */
        char name[MAX_PATH_LEN];
        i = 0;
        while (pos < obj->size && obj->data[pos] != 0 && i < MAX_PATH_LEN - 1) {
            name[i++] = (char)obj->data[pos++];
        }
        name[i] = 0;
        if (pos >= obj->size) return -1;
        pos++; /* skip NUL */
        if (pos + 20 > obj->size) return -1;
        unsigned char sha_bin[20];
        memcpy(sha_bin, obj->data + pos, 20);
        pos += 20;
        entries[count].mode = (mode_t)strtoul(mode_str, NULL, 8);
        strncpy(entries[count].name, name, MAX_PATH_LEN - 1);
        sha1_to_hex(sha_bin, entries[count].sha);
        count++;
    }
    *out_count = count;
    return 0;
}

/* --- Commit operations --- */
int git_create_commit(GitState *state, const CommitData *data, char *out_sha) {
    char buf[MAX_MSG_LEN * 4];
    size_t pos = 0;
    pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "tree %s\n", data->tree_sha);
    if (data->parent_sha[0]) {
        pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "parent %s\n", data->parent_sha);
    }
    pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos,
                            "author %s %ld +0000\n",
                            data->author[0] ? data->author : "unknown <unknown@kenux>",
                            (long)(data->timestamp ? data->timestamp : time(NULL)));
    pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos,
                            "committer %s %ld +0000\n\n",
                            data->committer[0] ? data->committer : data->author[0] ? data->author : "unknown <unknown@kenux>",
                            (long)(data->timestamp ? data->timestamp : time(NULL)));
    pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%s\n",
                            data->message[0] ? data->message : "no commit message");
    GitObject obj;
    memset(&obj, 0, sizeof(obj));
    obj.type = OBJ_COMMIT;
    obj.size = pos;
    obj.data = (unsigned char *)buf;
    git_hash_object(obj.data, obj.size, obj.type, obj.sha);
    int rc = git_write_object(state, &obj);
    if (rc == 0 && out_sha) strncpy(out_sha, obj.sha, MAX_SHA_LEN - 1);
    return rc;
}

static char *read_line_c(const char **pptr, const char *end) {
    const char *p = *pptr;
    while (p < end && *p && *p != '\n') p++;
    size_t len = (size_t)(p - *pptr);
    char *line = malloc(len + 1);
    if (!line) return NULL;
    memcpy(line, *pptr, len);
    line[len] = 0;
    *pptr = (p < end && *p == '\n') ? p + 1 : end;
    return line;
}

int git_parse_commit(const GitObject *obj, CommitData *data) {
    if (obj->type != OBJ_COMMIT) return -1;
    memset(data, 0, sizeof(*data));
    const char *start = (const char *)obj->data;
    const char *end = start + obj->size;
    const char *p = start;
    while (p < end) {
        if (*p == '\n') { p++; break; } /* end of headers */
        char *line = read_line_c(&p, end);
        if (!line) return -1;
        if (strncmp(line, "tree ", 5) == 0) {
            strncpy(data->tree_sha, line + 5, MAX_SHA_LEN - 1);
        } else if (strncmp(line, "parent ", 7) == 0) {
            strncpy(data->parent_sha, line + 7, MAX_SHA_LEN - 1);
        } else if (strncmp(line, "author ", 7) == 0) {
            char *lt = strchr(line + 7, '<');
            char *gt = strchr(line + 7, '>');
            if (lt && gt) {
                size_t len = (size_t)(gt - line - 5);
                if (len >= MAX_AUTHOR_LEN) len = MAX_AUTHOR_LEN - 1;
                memcpy(data->author, line + 7, len);
                data->author[len] = 0;
            } else {
                strncpy(data->author, line + 7, MAX_AUTHOR_LEN - 1);
            }
            char *ts = strchr(line + 7, '>');
            if (ts) data->timestamp = (time_t)strtol(ts + 2, NULL, 10);
        } else if (strncmp(line, "committer ", 10) == 0) {
            char *lt = strchr(line + 10, '<');
            char *gt = strchr(line + 10, '>');
            if (lt && gt) {
                size_t len = (size_t)(gt - line - 8);
                if (len >= MAX_AUTHOR_LEN) len = MAX_AUTHOR_LEN - 1;
                memcpy(data->committer, line + 10, len);
                data->committer[len] = 0;
            } else {
                strncpy(data->committer, line + 10, MAX_AUTHOR_LEN - 1);
            }
        }
        free(line);
    }
    /* message body */
    size_t msg_len = (size_t)(end - p);
    if (msg_len >= MAX_MSG_LEN) msg_len = MAX_MSG_LEN - 1;
    memcpy(data->message, p, msg_len);
    data->message[msg_len] = 0;
    if (data->timestamp == 0) data->timestamp = time(NULL);
    return 0;
}

int git_get_head_commit(GitState *state, char *out_sha) {
    if (!state->initialized) return -1;
    if (state->head_sha[0] == 0) return -1;
    if (out_sha) strncpy(out_sha, state->head_sha, MAX_SHA_LEN - 1);
    return 0;
}

int git_update_head(GitState *state, const char *sha) {
    if (!state->initialized) return -1;
    char path[MAX_PATH_LEN];
    if (state->head_ref[0]) {
        snprintf(path, sizeof(path), "%s/%s", state->git_dir, state->head_ref);
        dir_mkdir_p((*(strrchr(path, '/')) = 0, path), 0755);
        *strrchr(path, 0) = '/';
    } else {
        snprintf(path, sizeof(path), "%s/HEAD", state->git_dir);
    }
    char line[MAX_SHA_LEN + 2];
    int len = snprintf(line, sizeof(line), "%s\n", sha);
    int rc = write_entire_file(path, (unsigned char *)line, (size_t)len, 0644);
    if (rc == 0) strncpy(state->head_sha, sha, MAX_SHA_LEN - 1);
    return rc;
}

/* --- Index / staging area --- */
static int index_cmp(const void *a, const void *b) {
    return strcmp(((const IndexEntry *)a)->path, ((const IndexEntry *)b)->path);
}

int git_load_index(GitState *state) {
    state->index_count = 0;
    char idxfile[MAX_PATH_LEN];
    snprintf(idxfile, sizeof(idxfile), "%s/index", state->git_dir);
    size_t size;
    char *data = read_entire_file(idxfile, &size);
    if (!data) return 0; /* empty index is fine */
    /* custom binary-safe text format: "<mode> <size> <mtime> <sha> <path>\n" */
    const char *p = data;
    const char *end = data + size;
    while (p < end) {
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        size_t linelen = (size_t)(nl - p);
        if (linelen > 0) {
            char *line = malloc(linelen + 1);
            memcpy(line, p, linelen);
            line[linelen] = 0;
            unsigned mode;
            unsigned long sz;
            long mtim;
            char sha[MAX_SHA_LEN];
            char pathv[MAX_PATH_LEN];
            if (sscanf(line, "%o %lu %ld %40s %[^\n]",
                       &mode, &sz, &mtim, sha, pathv) >= 4) {
                if (state->index_count < MAX_INDEX_ENTRIES) {
                    IndexEntry *e = &state->index[state->index_count++];
                    e->mode = (mode_t)mode;
                    e->size = (size_t)sz;
                    e->mtime = (time_t)mtim;
                    strncpy(e->sha, sha, MAX_SHA_LEN - 1);
                    strncpy(e->path, pathv, MAX_PATH_LEN - 1);
                }
            }
            free(line);
        }
        p = nl + 1;
    }
    free(data);
    return 0;
}

int git_save_index(GitState *state) {
    char idxfile[MAX_PATH_LEN];
    snprintf(idxfile, sizeof(idxfile), "%s/index", state->git_dir);
    qsort(state->index, (size_t)state->index_count, sizeof(IndexEntry), index_cmp);
    size_t cap = 4096, pos = 0;
    char *buf = malloc(cap);
    if (!buf) return -1;
    for (int i = 0; i < state->index_count; i++) {
        IndexEntry *e = &state->index[i];
        char line[MAX_PATH_LEN * 2];
        int n = snprintf(line, sizeof(line), "%06o %zu %ld %s %s\n",
                         e->mode & 0777777, e->size, (long)e->mtime, e->sha, e->path);
        if (n < 0) continue;
        while (pos + (size_t)n + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
        memcpy(buf + pos, line, (size_t)n);
        pos += (size_t)n;
    }
    int rc = write_entire_file(idxfile, (unsigned char *)buf, pos, 0644);
    free(buf);
    return rc;
}

int git_add_to_index(GitState *state, const char *path, const char *sha, mode_t mode, size_t size, time_t mtime) {
    int idx = git_find_index_entry(state, path);
    if (idx < 0) {
        if (state->index_count >= MAX_INDEX_ENTRIES) return -1;
        idx = state->index_count++;
    }
    IndexEntry *e = &state->index[idx];
    strncpy(e->path, path, MAX_PATH_LEN - 1);
    strncpy(e->sha, sha, MAX_SHA_LEN - 1);
    e->mode = mode;
    e->size = size;
    e->mtime = mtime;
    e->stage = 0;
    return 0;
}

int git_remove_from_index(GitState *state, const char *path) {
    int idx = git_find_index_entry(state, path);
    if (idx < 0) return -1;
    for (int i = idx; i < state->index_count - 1; i++)
        state->index[i] = state->index[i + 1];
    state->index_count--;
    return 0;
}

int git_find_index_entry(GitState *state, const char *path) {
    for (int i = 0; i < state->index_count; i++) {
        if (strcmp(state->index[i].path, path) == 0) return i;
    }
    return -1;
}

int git_clear_index(GitState *state) {
    state->index_count = 0;
    return 0;
}

/* --- Commands --- */
int cmd_init(GitState *state, int argc, char **argv) {
    (void)state;
    int bare = 0;
    const char *path = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--bare") == 0) bare = 1;
        else if (argv[i][0] != '-') path = argv[i];
    }
    return git_init_repo(path, bare);
}

static int add_recursive(GitState *state, const char *path) {
    if (is_regular_file(path)) {
        char sha[MAX_SHA_LEN];
        if (git_create_blob_from_file(state, path, sha) != 0) {
            fprintf(stderr, "error: unable to hash file %s\n", path);
            return -1;
        }
        struct stat st;
        stat(path, &st);
        return git_add_to_index(state, path, sha, st.st_mode, (size_t)st.st_size, st.st_mtime);
    }
    if (is_directory(path)) {
        /* skip our own .git dir */
        if (strcmp(path, GIT_DIR) == 0) return 0;
        DIR *d = opendir(path);
        if (!d) return -1;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char sub[MAX_PATH_LEN];
            snprintf(sub, sizeof(sub), "%s/%s", path, ent->d_name);
            add_recursive(state, sub);
        }
        closedir(d);
        return 0;
    }
    return -1;
}

int cmd_add(GitState *state, int argc, char **argv) {
    if (!state->initialized) { fprintf(stderr, "fatal: not a git repository\n"); return 128; }
    git_load_index(state);
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) continue;
        if (add_recursive(state, argv[i]) != 0 && !is_directory(argv[i])) {
            fprintf(stderr, "fatal: pathspec '%s' did not match any files\n", argv[i]);
            git_save_index(state);
            return 128;
        }
    }
    return git_save_index(state);
}

static int build_tree_from_index(GitState *state, char *out_sha) {
    TreeEntry *entries = malloc(sizeof(TreeEntry) * (size_t)state->index_count);
    if (!entries) return -1;
    int count = 0;
    for (int i = 0; i < state->index_count; i++) {
        IndexEntry *e = &state->index[i];
        entries[count].mode = e->mode ? e->mode : 0100644;
        if (!S_ISDIR(entries[count].mode) && !S_ISREG(entries[count].mode) && !S_ISLNK(entries[count].mode)) {
            entries[count].mode = 0100644;
        }
        strncpy(entries[count].name, e->path, MAX_PATH_LEN - 1);
        strncpy(entries[count].sha, e->sha, MAX_SHA_LEN - 1);
        count++;
    }
    int rc = git_create_tree_from_entries(state, entries, count, out_sha);
    free(entries);
    return rc;
}

int cmd_commit(GitState *state, int argc, char **argv) {
    if (!state->initialized) { fprintf(stderr, "fatal: not a git repository\n"); return 128; }
    const char *msg = NULL;
    int allow_empty = 0;
    char message_buf[MAX_MSG_LEN];
    message_buf[0] = 0;
    for (int i = 0; i < argc; i++) {
        if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--message") == 0) && i + 1 < argc) {
            msg = argv[++i];
        } else if (strcmp(argv[i], "--allow-empty") == 0) {
            allow_empty = 1;
        }
    }
    if (msg) snprintf(message_buf, sizeof(message_buf), "%s", msg);
    git_load_index(state);
    if (state->index_count == 0 && !allow_empty && state->head_sha[0] == 0) {
        fprintf(stderr, "nothing to commit (create/copy files and use \"git add\")\n");
        return 1;
    }
    char tree_sha[MAX_SHA_LEN];
    if (build_tree_from_index(state, tree_sha) != 0) {
        fprintf(stderr, "fatal: unable to build tree\n");
        return 128;
    }
    CommitData cd;
    memset(&cd, 0, sizeof(cd));
    strncpy(cd.tree_sha, tree_sha, MAX_SHA_LEN - 1);
    if (state->head_sha[0]) strncpy(cd.parent_sha, state->head_sha, MAX_SHA_LEN - 1);
    const char *author_env = getenv("GIT_AUTHOR_NAME");
    const char *email_env = getenv("GIT_AUTHOR_EMAIL");
    char author_buf[MAX_AUTHOR_LEN];
    if (author_env) snprintf(author_buf, sizeof(author_buf), "%s <%s>", author_env, email_env ? email_env : "user@kenux");
    else snprintf(author_buf, sizeof(author_buf), "Kenux User <user@kenuxos.local>");
    strncpy(cd.author, author_buf, MAX_AUTHOR_LEN - 1);
    strncpy(cd.committer, author_buf, MAX_AUTHOR_LEN - 1);
    cd.timestamp = time(NULL);
    if (msg) strncpy(cd.message, msg, MAX_MSG_LEN - 1);
    else strncpy(cd.message, "(no message)", MAX_MSG_LEN - 1);
    char new_sha[MAX_SHA_LEN];
    if (git_create_commit(state, &cd, new_sha) != 0) {
        fprintf(stderr, "fatal: unable to create commit\n");
        return 128;
    }
    if (git_update_head(state, new_sha) != 0) {
        fprintf(stderr, "fatal: unable to update HEAD\n");
        return 128;
    }
    /* determine branch name */
    const char *branch = state->head_ref[0] ? (strrchr(state->head_ref, '/') ? strrchr(state->head_ref, '/') + 1 : state->head_ref) : "HEAD";
    if (cd.parent_sha[0] == 0)
        printf("[(root-commit) %.7s] %s\n", new_sha, cd.message);
    else
        printf("[%s %.7s] %s\n", branch, new_sha, cd.message);
    return 0;
}

static int file_changed_from_index(GitState *state, int idx) {
    IndexEntry *e = &state->index[idx];
    struct stat st;
    if (stat(e->path, &st) != 0) return 1; /* deleted */
    if ((size_t)st.st_size != e->size) return 1;
    if (st.st_mtime != e->mtime) return 1;
    return 0;
}

int cmd_status(GitState *state, int argc, char **argv) {
    (void)argc; (void)argv;
    if (!state->initialized) { fprintf(stderr, "fatal: not a git repository\n"); return 128; }
    git_load_index(state);
    const char *branch = state->head_ref[0] ? (strrchr(state->head_ref, '/') ? strrchr(state->head_ref, '/') + 1 : state->head_ref) : "HEAD";
    printf("On branch %s\n\n", branch);
    printf("Changes to be committed:\n  (use \"git reset HEAD <file>...\" to unstage)\n\n");
    int staged = 0;
    /* for minimal status: if index != HEAD tree, show staged */
    if (state->index_count > 0 || state->head_sha[0] == 0) {
        for (int i = 0; i < state->index_count; i++) {
            printf("\tnew file:   %s\n", state->index[i].path);
            staged++;
        }
    }
    if (!staged) printf("\t(no changes added to commit)\n");
    printf("\n");
    printf("Changes not staged for commit:\n  (use \"git add/rm <file>...\" to update what will be committed)\n\n");
    int modified = 0;
    for (int i = 0; i < state->index_count; i++) {
        if (file_changed_from_index(state, i)) {
            struct stat st;
            if (stat(state->index[i].path, &st) == 0) {
                printf("\tmodified:   %s\n", state->index[i].path);
                modified++;
            } else {
                printf("\tdeleted:    %s\n", state->index[i].path);
                modified++;
            }
        }
    }
    if (!modified) printf("\t(no changes added to commit)\n");
    printf("\n");
    printf("Untracked files:\n  (use \"git add <file>...\" to include in what will be committed)\n\n");
    int untracked = 0;
    DIR *d = opendir(state->repo_root);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            if (strcmp(ent->d_name, GIT_DIR) == 0) continue;
            int found = 0;
            for (int i = 0; i < state->index_count; i++) {
                if (strcmp(state->index[i].path, ent->d_name) == 0) { found = 1; break; }
            }
            if (!found) {
                printf("\t%s\n", ent->d_name);
                untracked++;
            }
        }
        closedir(d);
    }
    if (!untracked) printf("\t(no files)\n");
    return 0;
}

int cmd_log(GitState *state, int argc, char **argv) {
    (void)argc; (void)argv;
    if (!state->initialized) { fprintf(stderr, "fatal: not a git repository\n"); return 128; }
    char sha[MAX_SHA_LEN];
    if (git_get_head_commit(state, sha) != 0) {
        fprintf(stderr, "fatal: your current branch '%s' does not have any commits yet\n",
                state->head_ref[0] ? (strrchr(state->head_ref, '/') ? strrchr(state->head_ref, '/') + 1 : state->head_ref) : "HEAD");
        return 128;
    }
    int n = 0;
    int max_count = -1;
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "-n", 2) == 0 && argv[i][2]) max_count = atoi(argv[i] + 2);
        else if (strcmp(argv[i], "--oneline") == 0) { /* treat as flag */ }
        else if (argv[i][0] == '-') max_count = atoi(argv[i] + 1);
    }
    char cur[MAX_SHA_LEN] = "";
    strncpy(cur, sha, MAX_SHA_LEN - 1);
    while (cur[0] && (max_count < 0 || n < max_count)) {
        GitObject obj;
        if (git_read_object(state, cur, &obj) != 0) break;
        CommitData data;
        if (git_parse_commit(&obj, &data) != 0) { git_free_object(&obj); break; }
        char short_sha[8];
        memcpy(short_sha, cur, 7);
        short_sha[7] = 0;
        /* message first line */
        char firstline[MAX_MSG_LEN];
        snprintf(firstline, sizeof(firstline), "%s", data.message);
        firstline[strcspn(firstline, "\n")] = 0;
        printf("\033[33mcommit %s\033[0m\n", cur);
        printf("Author: %s\n", data.author);
        char *timestr = ctime(&data.timestamp);
        if (timestr) printf("Date:   %s", timestr);
        printf("\n    %s\n\n", firstline);
        strncpy(cur, data.parent_sha, MAX_SHA_LEN - 1);
        git_free_object(&obj);
        n++;
    }
    return 0;
}

int cmd_diff(GitState *state, int argc, char **argv) {
    (void)argc; (void)argv;
    if (!state->initialized) { fprintf(stderr, "fatal: not a git repository\n"); return 128; }
    git_load_index(state);
    for (int i = 0; i < state->index_count; i++) {
        IndexEntry *e = &state->index[i];
        if (!file_changed_from_index(state, i)) continue;
        /* read current file + indexed blob and print unified diff (minimal) */
        size_t new_size;
        char *new_data = read_entire_file(e->path, &new_size);
        GitObject old;
        int ok = git_read_object(state, e->sha, &old) == 0;
        if (ok) {
            printf("diff --git a/%s b/%s\n", e->path, e->path);
            printf("index %.7s..XXXXXXX 100644\n", e->sha);
            printf("--- a/%s\n+++ b/%s\n", e->path, e->path);
            /* byte level +/- lines heuristic */
            const char *oldp = (const char *)old.data;
            const char *olde = oldp + old.size;
            const char *newp = new_data;
            const char *newe = newp + new_size;
            size_t lineno_old = 1, lineno_new = 1;
            while (oldp < olde || newp < newe) {
                const char *olon = oldp;
                while (oldp < olde && *oldp != '\n') oldp++;
                size_t olen = (size_t)(oldp - olon);
                int old_has_nl = (oldp < olde);
                if (old_has_nl) oldp++;

                const char *nlon = newp;
                while (newp < newe && *newp != '\n') newp++;
                size_t nlen = (size_t)(newp - nlon);
                int new_has_nl = (newp < newe);
                if (new_has_nl) newp++;

                int same = (olen == nlen) && (memcmp(olon, nlon, olen) == 0);
                if (same) {
                    lineno_old++; lineno_new++;
                } else {
                    printf("@@ -%zu +%zu @@\n", lineno_old, lineno_new);
                    if (olon < olde || olen > 0) {
                        printf("-");
                        fwrite(olon, 1, olen, stdout);
                        if (!old_has_nl) printf("\n\\ No newline at end of file");
                        printf("\n");
                    }
                    if (nlon < newe || nlen > 0) {
                        printf("+");
                        fwrite(nlon, 1, nlen, stdout);
                        if (!new_has_nl) printf("\n\\ No newline at end of file");
                        printf("\n");
                    }
                    lineno_old++; lineno_new++;
                }
            }
            git_free_object(&old);
        }
        if (new_data) free(new_data);
    }
    return 0;
}

int cmd_branch(GitState *state, int argc, char **argv) {
    if (!state->initialized) { fprintf(stderr, "fatal: not a git repository\n"); return 128; }
    int list = 0, delete = 0;
    const char *name = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--delete") == 0) delete = 1;
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) list = 1;
        else if (argv[i][0] != '-') name = argv[i];
    }
    if (!name || list) {
        char heads_dir[MAX_PATH_LEN];
        snprintf(heads_dir, sizeof(heads_dir), "%s/refs/heads", state->git_dir);
        DIR *d = opendir(heads_dir);
        if (d) {
            const char *cur = state->head_ref[0] ? (strrchr(state->head_ref, '/') ? strrchr(state->head_ref, '/') + 1 : state->head_ref) : NULL;
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (ent->d_name[0] == '.') continue;
                int is_cur = cur && strcmp(cur, ent->d_name) == 0;
                printf("%c %s\n", is_cur ? '*' : ' ', ent->d_name);
            }
            closedir(d);
        }
        return 0;
    }
    if (delete) {
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/refs/heads/%s", state->git_dir, name);
        if (unlink(path) != 0) { fprintf(stderr, "error: branch '%s' not found.\n", name); return 1; }
        printf("Deleted branch %s\n", name);
        return 0;
    }
    /* create branch */
    if (state->head_sha[0] == 0) {
        fprintf(stderr, "fatal: Not a valid object name: '%s'.\n", name);
        return 128;
    }
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/refs/heads/%s", state->git_dir, name);
    dir_mkdir_p((*(strrchr(path, '/')) = 0, path), 0755);
    *strrchr(path, 0) = '/';
    char line[MAX_SHA_LEN + 2];
    int len = snprintf(line, sizeof(line), "%s\n", state->head_sha);
    return write_entire_file(path, (unsigned char *)line, (size_t)len, 0644);
}

int cmd_checkout(GitState *state, int argc, char **argv) {
    if (!state->initialized) { fprintf(stderr, "fatal: not a git repository\n"); return 128; }
    if (argc < 1) { fprintf(stderr, "usage: git checkout <branch|commit>\n"); return 1; }
    const char *name = argv[0];
    /* try as branch first */
    char branch_ref[MAX_PATH_LEN];
    snprintf(branch_ref, sizeof(branch_ref), "refs/heads/%s", name);
    char refpath[MAX_PATH_LEN];
    snprintf(refpath, sizeof(refpath), "%s/%s", state->git_dir, branch_ref);
    char sha[MAX_SHA_LEN] = "";
    int is_branch = 0;
    size_t sz;
    char *data = read_entire_file(refpath, &sz);
    if (data) {
        data[strcspn(data, "\r\n \t")] = 0;
        strncpy(sha, data, MAX_SHA_LEN - 1);
        free(data);
        is_branch = 1;
    } else if (strlen(name) >= 40) {
        strncpy(sha, name, MAX_SHA_LEN - 1);
    } else {
        fprintf(stderr, "error: pathspec '%s' did not match any branch or commit\n", name);
        return 1;
    }
    GitObject obj;
    if (git_read_object(state, sha, &obj) != 0) {
        fprintf(stderr, "fatal: reference is not a tree: %s\n", sha);
        return 128;
    }
    CommitData cd;
    if (git_parse_commit(&obj, &cd) != 0) { git_free_object(&obj); return 128; }
    git_free_object(&obj);

    /* checkout tree: update index and write files */
    GitObject tree;
    if (git_read_object(state, cd.tree_sha, &tree) != 0) {
        fprintf(stderr, "fatal: invalid tree\n"); return 128;
    }
    TreeEntry entries[MAX_INDEX_ENTRIES];
    int count = 0;
    if (git_parse_tree(&tree, entries, &count) != 0) {
        git_free_object(&tree); return 128;
    }
    git_clear_index(state);
    for (int i = 0; i < count; i++) {
        git_add_to_index(state, entries[i].name, entries[i].sha, entries[i].mode, 0, 0);
        git_write_blob_to_file(state, entries[i].sha, entries[i].name);
    }
    git_free_object(&tree);
    git_save_index(state);

    /* update HEAD */
    char head_path[MAX_PATH_LEN];
    snprintf(head_path, sizeof(head_path), "%s/HEAD", state->git_dir);
    char head_line[MAX_PATH_LEN];
    int hlen;
    if (is_branch) {
        hlen = snprintf(head_line, sizeof(head_line), "ref: %s\n", branch_ref);
        strncpy(state->head_ref, branch_ref, MAX_PATH_LEN - 1);
    } else {
        hlen = snprintf(head_line, sizeof(head_line), "%s\n", sha);
        state->head_ref[0] = 0;
    }
    write_entire_file(head_path, (unsigned char *)head_line, (size_t)hlen, 0644);
    strncpy(state->head_sha, sha, MAX_SHA_LEN - 1);
    printf("Note: checking out '%s'.\n\nYou are in 'detached HEAD' state.\n", name);
    return 0;
}

int cmd_reset(GitState *state, int argc, char **argv) {
    (void)argc; (void)argv;
    if (!state->initialized) return 128;
    /* soft reset: move HEAD to first arg, leave index & work tree */
    const char *target = (argc > 0) ? argv[argc - 1] : "HEAD";
    char sha[MAX_SHA_LEN];
    if (strcmp(target, "HEAD") == 0 || state->head_sha[0] == 0) {
        strncpy(sha, state->head_sha, MAX_SHA_LEN - 1);
    } else {
        strncpy(sha, target, MAX_SHA_LEN - 1);
    }
    if (sha[0] == 0) return 0;
    return git_update_head(state, sha);
}

int cmd_rm(GitState *state, int argc, char **argv) {
    if (!state->initialized) return 128;
    git_load_index(state);
    int cached = 0, force = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--cached") == 0) cached = 1;
        else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) force = 1;
        else {
            int idx = git_find_index_entry(state, argv[i]);
            if (idx < 0) {
                fprintf(stderr, "fatal: pathspec '%s' did not match any files\n", argv[i]);
                continue;
            }
            git_remove_from_index(state, argv[i]);
            if (!cached) unlink(argv[i]);
        }
    }
    return git_save_index(state);
}

int cmd_mv(GitState *state, int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: git mv <src> <dst>\n"); return 1; }
    if (!state->initialized) return 128;
    git_load_index(state);
    const char *src = argv[0];
    const char *dst = argv[1];
    int idx = git_find_index_entry(state, src);
    if (idx < 0) { fprintf(stderr, "fatal: bad source, skipping %s\n", src); return 1; }
    IndexEntry saved = state->index[idx];
    git_remove_from_index(state, src);
    strncpy(saved.path, dst, MAX_PATH_LEN - 1);
    if (state->index_count >= MAX_INDEX_ENTRIES) return 1;
    state->index[state->index_count++] = saved;
    rename(src, dst);
    return git_save_index(state);
}

int cmd_tag(GitState *state, int argc, char **argv) {
    if (!state->initialized) return 128;
    if (argc < 1) {
        char tag_dir[MAX_PATH_LEN];
        snprintf(tag_dir, sizeof(tag_dir), "%s/refs/tags", state->git_dir);
        DIR *d = opendir(tag_dir);
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (ent->d_name[0] != '.') printf("%s\n", ent->d_name);
            }
            closedir(d);
        }
        return 0;
    }
    const char *name = argv[0];
    const char *target = state->head_sha;
    if (strlen(target) < 40) { fprintf(stderr, "fatal: HEAD is detached or missing\n"); return 128; }
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/refs/tags/%s", state->git_dir, name);
    dir_mkdir_p((*(strrchr(path, '/')) = 0, path), 0755);
    *strrchr(path, 0) = '/';
    char line[MAX_SHA_LEN + 2];
    int len = snprintf(line, sizeof(line), "%s\n", target);
    return write_entire_file(path, (unsigned char *)line, (size_t)len, 0644);
}

int cmd_show(GitState *state, int argc, char **argv) {
    if (argc < 1 || !state->initialized) { fprintf(stderr, "usage: git show <object>\n"); return 1; }
    GitObject obj;
    if (git_read_object(state, argv[0], &obj) != 0) {
        fprintf(stderr, "fatal: ambiguous argument '%s': unknown revision\n", argv[0]); return 128;
    }
    if (obj.type == OBJ_COMMIT) {
        CommitData data;
        git_parse_commit(&obj, &data);
        printf("commit %s\nAuthor: %s\nDate:   %s\n\n    %s\n",
               obj.sha, data.author, data.timestamp ? ctime(&data.timestamp) : "unknown\n",
               data.message);
    } else if (obj.type == OBJ_TREE) {
        TreeEntry entries[MAX_INDEX_ENTRIES];
        int cnt = 0;
        git_parse_tree(&obj, entries, &cnt);
        for (int i = 0; i < cnt; i++) {
            printf("%06o %s %s\t%s\n", entries[i].mode & 0777777,
                   object_type_name(S_ISDIR(entries[i].mode) ? OBJ_TREE : OBJ_BLOB),
                   entries[i].sha, entries[i].name);
        }
    } else {
        fwrite(obj.data, 1, obj.size, stdout);
    }
    git_free_object(&obj);
    return 0;
}

int cmd_ls_files(GitState *state, int argc, char **argv) {
    (void)argc; (void)argv;
    if (!state->initialized) return 128;
    git_load_index(state);
    for (int i = 0; i < state->index_count; i++)
        printf("%s\n", state->index[i].path);
    return 0;
}

int cmd_write_tree(GitState *state, int argc, char **argv) {
    (void)argc; (void)argv;
    if (!state->initialized) return 128;
    git_load_index(state);
    char sha[MAX_SHA_LEN];
    if (build_tree_from_index(state, sha) != 0) return 1;
    printf("%s\n", sha);
    return 0;
}

int cmd_commit_tree(GitState *state, int argc, char **argv) {
    if (argc < 1 || !state->initialized) { fprintf(stderr, "usage: git commit-tree <tree> [-p parent] -m msg\n"); return 1; }
    CommitData cd;
    memset(&cd, 0, sizeof(cd));
    strncpy(cd.tree_sha, argv[0], MAX_SHA_LEN - 1);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) strncpy(cd.parent_sha, argv[++i], MAX_SHA_LEN - 1);
        else if ((strcmp(argv[i], "-m") == 0) && i + 1 < argc) strncpy(cd.message, argv[++i], MAX_MSG_LEN - 1);
    }
    snprintf(cd.author, sizeof(cd.author), "Kenux User <user@kenux>");
    strncpy(cd.committer, cd.author, MAX_AUTHOR_LEN - 1);
    cd.timestamp = time(NULL);
    char sha[MAX_SHA_LEN];
    if (git_create_commit(state, &cd, sha) != 0) return 1;
    printf("%s\n", sha);
    return 0;
}

int cmd_hash_object(GitState *state, int argc, char **argv) {
    int write = 0, show = 0;
    ObjectType type = OBJ_BLOB;
    const char *path = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0) write = 1;
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) type = object_type_from_name(argv[++i]);
        else if (strcmp(argv[i], "--stdin") == 0) show = 1;
        else if (argv[i][0] != '-') path = argv[i];
    }
    if (!path) { fprintf(stderr, "usage: git hash-object [-w] [-t type] <file>\n"); return 1; }
    size_t size;
    char *data = read_entire_file(path, &size);
    if (!data) return 1;
    char sha[MAX_SHA_LEN];
    git_hash_object((unsigned char *)data, size, type, sha);
    printf("%s\n", sha);
    if (write && state->initialized) {
        GitObject obj;
        obj.type = type; obj.size = size; obj.data = (unsigned char *)data;
        strncpy(obj.sha, sha, MAX_SHA_LEN - 1);
        git_write_object(state, &obj);
    }
    free(data);
    return 0;
}

int cmd_cat_file(GitState *state, int argc, char **argv) {
    if (argc < 2 || !state->initialized) {
        fprintf(stderr, "usage: git cat-file (-t|-s|-p|<type>) <object>\n"); return 1;
    }
    const char *flag = argv[0];
    const char *sha = argv[1];
    GitObject obj;
    if (git_read_object(state, sha, &obj) != 0) { fprintf(stderr, "fatal: Not a valid object name %s\n", sha); return 128; }
    if (strcmp(flag, "-t") == 0) printf("%s\n", object_type_name(obj.type));
    else if (strcmp(flag, "-s") == 0) printf("%zu\n", obj.size);
    else if (strcmp(flag, "-e") == 0) { /* exists */ }
    else if (strcmp(flag, "-p") == 0 || strcmp(flag, object_type_name(obj.type)) == 0) {
        if (obj.type == OBJ_COMMIT) {
            CommitData d; git_parse_commit(&obj, &d);
            printf("tree %s\n", d.tree_sha);
            if (d.parent_sha[0]) printf("parent %s\n", d.parent_sha);
            printf("author %s %ld +0000\n", d.author, (long)d.timestamp);
            printf("committer %s %ld +0000\n\n", d.committer, (long)d.timestamp);
            printf("%s", d.message);
        } else if (obj.type == OBJ_TREE) {
            TreeEntry entries[MAX_INDEX_ENTRIES];
            int c = 0;
            git_parse_tree(&obj, entries, &c);
            for (int i = 0; i < c; i++)
                printf("%06o %s %s\t%s\n", entries[i].mode & 0777777,
                       object_type_name(S_ISDIR(entries[i].mode) ? OBJ_TREE : OBJ_BLOB),
                       entries[i].sha, entries[i].name);
        } else {
            fwrite(obj.data, 1, obj.size, stdout);
        }
    }
    git_free_object(&obj);
    return 0;
}

/* --- Main --- */
static struct {
    const char *name;
    int (*fn)(GitState *, int, char **);
} commands[] = {
    { "init", cmd_init },
    { "add", cmd_add },
    { "commit", cmd_commit },
    { "status", cmd_status },
    { "log", cmd_log },
    { "diff", cmd_diff },
    { "branch", cmd_branch },
    { "checkout", cmd_checkout },
    { "reset", cmd_reset },
    { "rm", cmd_rm },
    { "mv", cmd_mv },
    { "tag", cmd_tag },
    { "show", cmd_show },
    { "ls-files", cmd_ls_files },
    { "write-tree", cmd_write_tree },
    { "commit-tree", cmd_commit_tree },
    { "hash-object", cmd_hash_object },
    { "cat-file", cmd_cat_file },
};

void print_usage(void) {
    printf("usage: kenux-git <command> [<args>]\n\nCommon commands:\n");
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        printf("   %s\n", commands[i].name);
    }
    printf("\nThese are common KenuxGit commands. See 'kenux-git help' for more information.\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }
    const char *cmd = argv[1];
    /* init doesn't need repo, but others do */
    static GitState state;
    git_init_state(&state);

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(); return 0;
    }
    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "version") == 0) {
        printf("kenux-git version 1.0.0-kenux\n"); return 0;
    }

    if (strcmp(cmd, "init") != 0) {
        if (git_find_repo(&state, ".") != 0) {
            fprintf(stderr, "fatal: not a git repository (or any of the parent directories): %s\n", GIT_DIR);
            return 128;
        }
    }

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(commands[i].name, cmd) == 0) {
            int rc = commands[i].fn(&state, argc - 2, argv + 2);
            git_cleanup_state(&state);
            return rc;
        }
    }
    fprintf(stderr, "kenux-git: '%s' is not a kenux-git command. See 'kenux-git --help'.\n", cmd);
    return 1;
}
