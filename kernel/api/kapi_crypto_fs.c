/*
 * Kenux Advanced OS Skeleton - Encrypted Filesystem implementation
 *
 * Skeleton implementation: defines state tables and returns KAPI/crypto
 * error codes. Crypto transforms are TODO until a backend is selected.
 */

#include "kapi_crypto_fs.h"
#include "kapi.h"

#include <string.h>

#define KAPI_CRYPTO_FS_MAX_KEYS 64

static struct {
    char              name[KAPI_CRYPTO_FS_NAME_MAX];
    kapi_crypto_key_t key;
    int               used;
} kapi_crypto_fs_keyring[KAPI_CRYPTO_FS_MAX_KEYS];

static int kapi_crypto_fs_initialized = 0;

int kapi_crypto_fs_init(void)
{
    if (kapi_crypto_fs_initialized) {
        return KAPI_CRYPTO_OK;
    }
    memset(kapi_crypto_fs_keyring, 0, sizeof(kapi_crypto_fs_keyring));
    kapi_crypto_fs_initialized = 1;
    return KAPI_CRYPTO_OK;
}

void kapi_crypto_fs_exit(void)
{
    /* Securely zeroize keys before teardown */
    for (int i = 0; i < KAPI_CRYPTO_FS_MAX_KEYS; i++) {
        memset(&kapi_crypto_fs_keyring[i].key, 0,
               sizeof(kapi_crypto_fs_keyring[i].key));
        kapi_crypto_fs_keyring[i].used = 0;
    }
    kapi_crypto_fs_initialized = 0;
}

int kapi_crypto_fs_derive_key(const char *passphrase,
                              const uint8_t *salt,
                              uint32_t iterations,
                              kapi_crypto_key_t *out)
{
    if (!passphrase || !salt || !out) {
        return KAPI_CRYPTO_EINVAL;
    }
    /* TODO: PBKDF2/Argon2 backend; placeholder uses zeroed key. */
    memset(out, 0, sizeof(*out));
    memcpy(out->salt, salt, KAPI_CRYPTO_FS_SALT_LEN);
    out->iterations = iterations ? iterations : KAPI_CRYPTO_FS_ITERATIONS;
    out->refcount = 0;
    return KAPI_CRYPTO_OK;
}

int kapi_crypto_fs_add_key(const char *name, const kapi_crypto_key_t *key)
{
    if (!name || !key || !kapi_crypto_fs_initialized) {
        return KAPI_CRYPTO_EINVAL;
    }
    for (int i = 0; i < KAPI_CRYPTO_FS_MAX_KEYS; i++) {
        if (!kapi_crypto_fs_keyring[i].used) {
            kapi_crypto_fs_keyring[i].used = 1;
            strncpy(kapi_crypto_fs_keyring[i].name, name,
                    KAPI_CRYPTO_FS_NAME_MAX - 1);
            memcpy(&kapi_crypto_fs_keyring[i].key, key, sizeof(*key));
            return KAPI_CRYPTO_OK;
        }
    }
    return KAPI_CRYPTO_ENOMEM;
}

int kapi_crypto_fs_remove_key(const char *name)
{
    if (!name) {
        return KAPI_CRYPTO_EINVAL;
    }
    for (int i = 0; i < KAPI_CRYPTO_FS_MAX_KEYS; i++) {
        if (kapi_crypto_fs_keyring[i].used &&
            strncmp(kapi_crypto_fs_keyring[i].name, name,
                    KAPI_CRYPTO_FS_NAME_MAX) == 0) {
            memset(&kapi_crypto_fs_keyring[i].key, 0,
                   sizeof(kapi_crypto_fs_keyring[i].key));
            kapi_crypto_fs_keyring[i].used = 0;
            return KAPI_CRYPTO_OK;
        }
    }
    return KAPI_CRYPTO_EINVAL;
}

int kapi_crypto_fs_load_keyring(const char *path)
{
    (void)path;
    /* TODO: load keys from VFS path */
    return KAPI_CRYPTO_ENOTSUP;
}

kapi_crypto_ctx_t *kapi_crypto_fs_open(const char *path, int mode)
{
    (void)path;
    (void)mode;
    /* TODO: locate backing file and resolve key */
    return NULL;
}

int kapi_crypto_fs_close(kapi_crypto_ctx_t *ctx)
{
    if (!ctx) {
        return KAPI_CRYPTO_EINVAL;
    }
    memset(ctx, 0, sizeof(*ctx));
    return KAPI_CRYPTO_OK;
}

int kapi_crypto_fs_encrypt(kapi_crypto_ctx_t *ctx,
                           const void *plain, void *cipher,
                           size_t len)
{
    if (!ctx || !plain || !cipher) {
        return KAPI_CRYPTO_EINVAL;
    }
    /* TODO: dispatch to AES backend per ctx->mode */
    memcpy(cipher, plain, len);
    return (int)len;
}

int kapi_crypto_fs_decrypt(kapi_crypto_ctx_t *ctx,
                           const void *cipher, void *plain,
                           size_t len)
{
    if (!ctx || !cipher || !plain) {
        return KAPI_CRYPTO_EINVAL;
    }
    /* TODO: dispatch to AES backend per ctx->mode; verify auth tag */
    memcpy(plain, cipher, len);
    return (int)len;
}

int kapi_crypto_fs_hook_read(void *buf, size_t len, uint64_t offset,
                             kapi_crypto_ctx_t *ctx)
{
    (void)buf; (void)len; (void)offset; (void)ctx;
    /* TODO: integrate with VFS read path */
    return KAPI_CRYPTO_ENOTSUP;
}

int kapi_crypto_fs_hook_write(const void *buf, size_t len, uint64_t offset,
                              kapi_crypto_ctx_t *ctx)
{
    (void)buf; (void)len; (void)offset; (void)ctx;
    /* TODO: integrate with VFS write path */
    return KAPI_CRYPTO_ENOTSUP;
}
