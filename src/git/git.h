/*
 * Kenux OS - Git Version Control System (Minimal)
 * Header file for git functionality
 */

#ifndef _GIT_H
#define _GIT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

#ifndef _WIN32
#include <unistd.h>
#else
/* Windows MinGW compatibility */
#include <io.h>
#include <process.h>
#include <direct.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* POSIX mkdir takes (path, mode); Windows mkdir takes only (path). */
static inline int kenux_mkdir(const char *p, mode_t m) { (void)m; return _mkdir(p); }
#define mkdir kenux_mkdir

/* realpath shim: resolve to absolute path. */
static inline char *kenux_realpath(const char *path, char *resolved) {
    if (!path) return NULL;
    if (!_fullpath(resolved, path, PATH_MAX)) return NULL;
    return resolved;
}
#define realpath kenux_realpath

#ifndef X_OK
#define X_OK 0
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif

/* Windows has no symlinks-by-default; treat S_ISLNK as always false. */
#ifndef S_ISLNK
#define S_ISLNK(m) 0
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#endif

#define GIT_DIR ".kenuxgit"
#define GIT_OBJECTS_DIR GIT_DIR "/objects"
#define GIT_REFS_DIR GIT_DIR "/refs"
#define GIT_HEAD_FILE GIT_DIR "/HEAD"
#define GIT_INDEX_FILE GIT_DIR "/index"
#define GIT_CONFIG_FILE GIT_DIR "/config"
#define GIT_LOG_FILE GIT_DIR "/logs/HEAD"

#define MAX_PATH_LEN 4096
#define MAX_SHA_LEN 41
#define MAX_MSG_LEN 8192
#define MAX_AUTHOR_LEN 256
#define MAX_OBJECTS 65536
#define MAX_INDEX_ENTRIES 4096
#define HASH_SIZE 20

/* Object types */
typedef enum {
    OBJ_BLOB,
    OBJ_TREE,
    OBJ_COMMIT,
    OBJ_TAG,
    OBJ_UNKNOWN
} ObjectType;

/* Git object */
typedef struct {
    char sha[MAX_SHA_LEN];
    ObjectType type;
    size_t size;
    unsigned char *data;
} GitObject;

/* Index entry (staging area) */
typedef struct {
    char path[MAX_PATH_LEN];
    char sha[MAX_SHA_LEN];
    mode_t mode;
    size_t size;
    time_t mtime;
    int stage;
} IndexEntry;

/* Commit object */
typedef struct {
    char tree_sha[MAX_SHA_LEN];
    char parent_sha[MAX_SHA_LEN];
    char author[MAX_AUTHOR_LEN];
    char committer[MAX_AUTHOR_LEN];
    time_t timestamp;
    char message[MAX_MSG_LEN];
} CommitData;

/* Tree entry */
typedef struct {
    mode_t mode;
    char name[MAX_PATH_LEN];
    char sha[MAX_SHA_LEN];
} TreeEntry;

/* Git state */
typedef struct {
    char repo_root[MAX_PATH_LEN];
    char git_dir[MAX_PATH_LEN];
    char head_ref[MAX_PATH_LEN];
    char head_sha[MAX_SHA_LEN];
    IndexEntry index[MAX_INDEX_ENTRIES];
    int index_count;
    int initialized;
} GitState;

/* Branch info */
typedef struct {
    char name[MAX_PATH_LEN];
    char sha[MAX_SHA_LEN];
} BranchInfo;

/* Function prototypes - core */
void git_init_state(GitState *state);
void git_cleanup_state(GitState *state);
int git_find_repo(GitState *state, const char *start_path);
int git_init_repo(const char *path, int bare);
int git_is_inside_work_tree(GitState *state);

/* Function prototypes - object database */
int git_hash_object(const unsigned char *data, size_t len, ObjectType type, char *out_sha);
int git_write_object(GitState *state, const GitObject *obj);
int git_read_object(GitState *state, const char *sha, GitObject *obj);
int git_object_exists(GitState *state, const char *sha);
void git_free_object(GitObject *obj);

/* Function prototypes - blob (file content) */
int git_create_blob_from_file(GitState *state, const char *path, char *out_sha);
int git_write_blob_to_file(GitState *state, const char *sha, const char *path);

/* Function prototypes - tree (directory listing) */
int git_create_tree_from_entries(GitState *state, const TreeEntry *entries, int count, char *out_sha);
int git_parse_tree(const GitObject *obj, TreeEntry *entries, int *out_count);

/* Function prototypes - commit */
int git_create_commit(GitState *state, const CommitData *data, char *out_sha);
int git_parse_commit(const GitObject *obj, CommitData *data);
int git_get_head_commit(GitState *state, char *out_sha);
int git_update_head(GitState *state, const char *sha);

/* Function prototypes - index / staging */
int git_load_index(GitState *state);
int git_save_index(GitState *state);
int git_add_to_index(GitState *state, const char *path, const char *sha, mode_t mode, size_t size, time_t mtime);
int git_remove_from_index(GitState *state, const char *path);
int git_find_index_entry(GitState *state, const char *path);
int git_clear_index(GitState *state);

/* Function prototypes - commands */
int cmd_init(GitState *state, int argc, char **argv);
int cmd_add(GitState *state, int argc, char **argv);
int cmd_commit(GitState *state, int argc, char **argv);
int cmd_status(GitState *state, int argc, char **argv);
int cmd_log(GitState *state, int argc, char **argv);
int cmd_diff(GitState *state, int argc, char **argv);
int cmd_branch(GitState *state, int argc, char **argv);
int cmd_checkout(GitState *state, int argc, char **argv);
int cmd_reset(GitState *state, int argc, char **argv);
int cmd_rm(GitState *state, int argc, char **argv);
int cmd_mv(GitState *state, int argc, char **argv);
int cmd_tag(GitState *state, int argc, char **argv);
int cmd_show(GitState *state, int argc, char **argv);
int cmd_ls_files(GitState *state, int argc, char **argv);
int cmd_write_tree(GitState *state, int argc, char **argv);
int cmd_commit_tree(GitState *state, int argc, char **argv);
int cmd_hash_object(GitState *state, int argc, char **argv);
int cmd_cat_file(GitState *state, int argc, char **argv);

/* Utilities */
int compute_sha1(const unsigned char *data, size_t len, unsigned char *out_hash);
void sha1_to_hex(const unsigned char *hash, char *out_hex);
int hex_to_sha1(const char *hex, unsigned char *out_hash);
int path_exists(const char *path);
int is_directory(const char *path);
int is_regular_file(const char *path);
char *read_entire_file(const char *path, size_t *out_size);
int write_entire_file(const char *path, const unsigned char *data, size_t size, mode_t mode);
int dir_mkdir_p(const char *path, mode_t mode);
const char *object_type_name(ObjectType type);
ObjectType object_type_from_name(const char *name);
void print_usage(void);

#endif /* _GIT_H */
