#ifndef KAPI_CRYPTO_FS_H
#define KAPI_CRYPTO_FS_H

/*
 * Kenux Advanced OS Skeleton - Encrypted Filesystem Layer
 *
 * Provides transparent per-file AES-256 encryption with key derivation,
 * cached crypto contexts, and hook points into VFS read/write paths.
 * Implementation is a skeleton: data structures and API surface are
 * defined; crypto transforms are wired through pluggable backends.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_CRYPTO_FS_NAME_MAX     64
#define KAPI_CRYPTO_FS_KEY_LEN     32   /* AES-256 */
#define KAPI_CRYPTO_FS_IV_LEN      16
#define KAPI_CRYPTO_FS_SALT_LEN    16
#define KAPI_CRYPTO_FS_ITERATIONS  10000

#define KAPI_CRYPTO_FS_MODE_CBC    0
#define KAPI_CRYPTO_FS_MODE_GCM    1
#define KAPI_CRYPTO_FS_MODE_XTS    2

typedef struct kapi_crypto_ctx   kapi_crypto_ctx_t;
typedef struct kapi_crypto_key    kapi_crypto_key_t;

typedef enum {
    KAPI_CRYPTO_OK            = 0,
    KAPI_CRYPTO_EINVAL        = -1,
    KAPI_CRYPTO_ENOMEM        = -2,
    KAPI_CRYPTO_EKEY          = -3,
    KAPI_CRYPTO_EIO           = -4,
    KAPI_CRYPTO_ENOTSUP       = -5,
    KAPI_CRYPTO_EAUTH         = -6
} kapi_crypto_err_t;

struct kapi_crypto_key {
    uint8_t  raw[KAPI_CRYPTO_FS_KEY_LEN];
    uint8_t  salt[KAPI_CRYPTO_FS_SALT_LEN];
    uint32_t iterations;
    uint32_t refcount;
};

struct kapi_crypto_ctx {
    int              mode;        /* KAPI_CRYPTO_FS_MODE_* */
    kapi_crypto_key_t key;
    uint8_t          iv[KAPI_CRYPTO_FS_IV_LEN];
    int              initialized;
    int              cached;
    char             fs_name[KAPI_CRYPTO_FS_NAME_MAX];
};

/* Lifecycle */
int kapi_crypto_fs_init(void);
void kapi_crypto_fs_exit(void);

/* Key management */
int kapi_crypto_fs_derive_key(const char *passphrase,
                              const uint8_t *salt,
                              uint32_t iterations,
                              kapi_crypto_key_t *out);
int kapi_crypto_fs_add_key(const char *name, const kapi_crypto_key_t *key);
int kapi_crypto_fs_remove_key(const char *name);
int kapi_crypto_fs_load_keyring(const char *path);

/* Per-file encryption context */
kapi_crypto_ctx_t *kapi_crypto_fs_open(const char *path, int mode);
int kapi_crypto_fs_close(kapi_crypto_ctx_t *ctx);

/* Data transforms (operate on page-sized buffers) */
int kapi_crypto_fs_encrypt(kapi_crypto_ctx_t *ctx,
                           const void *plain, void *cipher,
                           size_t len);
int kapi_crypto_fs_decrypt(kapi_crypto_ctx_t *ctx,
                           const void *cipher, void *plain,
                           size_t len);

/* VFS hook points */
int kapi_crypto_fs_hook_read(void *buf, size_t len, uint64_t offset,
                             kapi_crypto_ctx_t *ctx);
int kapi_crypto_fs_hook_write(const void *buf, size_t len, uint64_t offset,
                              kapi_crypto_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* KAPI_CRYPTO_FS_H */
