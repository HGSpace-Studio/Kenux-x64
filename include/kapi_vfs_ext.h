#ifndef KAPI_VFS_EXT_H
#define KAPI_VFS_EXT_H

#include <stdint.h>
#include <stddef.h>
#include "kapi_vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extended mount flags */
#define KAPI_VFS_MS_ENCRYPTED   (1ULL << 10)
#define KAPI_VFS_MS_SNAPSHOT    (1ULL << 11)
#define KAPI_VFS_MS_COMPRESSED  (1ULL << 12)

/* Extended file attributes */
typedef struct {
    uint64_t xattr_size;           /* Extended attribute size */
    uint64_t encryption_key_id;    /* Encryption key identifier */
    uint64_t snapshot_id;          /* Snapshot identifier */
    uint64_t compressed_size;      /* Compressed size (if compressed) */
    uint64_t sector_size;         /* Underlying sector size */
    uint32_t encryption_algorithm; /* Encryption algorithm */
    uint32_t compression_level;   /* Compression level */
    uint8_t  is_encrypted;        /* Is encrypted flag */
    uint8_t  is_compressed;       /* Is compressed flag */
    uint8_t  is_snapshot;         /* Is snapshot flag */
    uint8_t  reserved;            /* Reserved for future use */
} kapi_inode_xattr_t;

/* File system encryption context */
typedef struct kapi_fs_encrypt_ctx kapi_fs_encrypt_ctx_t;

/* Encryption algorithms */
#define KAPI_ENCRYPT_NONE          0
#define KAPI_ENCRYPT_AES_XTS       1
#define KAPI_ENCRYPT_AES_CBC       2
#define KAPI_ENCRYPT_CHACHA20      3

/* Compression algorithms */
#define KAPI_COMPRESS_NONE         0
#define KAPI_COMPRESS_LZ4          1
#define KAPI_COMPRESS_ZLIB         2
#define KAPI_COMPRESS_BZ2         3

/* Extended VFS operations */
int kapi_vfs_inode_get_xattr(kapi_inode_t* inode, kapi_inode_xattr_t* xattr);
int kapi_vfs_inode_set_xattr(kapi_inode_t* inode, const kapi_inode_xattr_t* xattr);

kapi_fs_encrypt_ctx_t* kapi_vfs_create_encrypt_context(uint32_t algorithm, 
                                                       const uint8_t* key, 
                                                       size_t key_len);
int kapi_vfs_encrypt_data(kapi_fs_encrypt_ctx_t* ctx, 
                         const uint8_t* in_data, size_t in_len,
                         uint8_t* out_data, size_t* out_len);
int kapi_vfs_decrypt_data(kapi_fs_encrypt_ctx_t* ctx, 
                         const uint8_t* in_data, size_t in_len,
                         uint8_t* out_data, size_t* out_len);
void kapi_vfs_destroy_encrypt_context(kapi_fs_encrypt_ctx_t* ctx);

/* Snapshot management */
typedef struct kapi_snapshot kapi_snapshot_t;

kapi_snapshot_t* kapi_vfs_create_snapshot(const char* fs_path, const char* snapshot_name);
int kapi_vfs_delete_snapshot(kapi_snapshot_t* snapshot);
int kapi_vfs_restore_snapshot(kapi_snapshot_t* snapshot);
int kapi_vfs_list_snapshots(const char* fs_path, kapi_snapshot_t** snapshots, int* count);

/* Extended mount operations */
int kapi_vfs_mount_encrypted(const char* source, const char* target, 
                            const char* fstype, uint64_t flags,
                            const uint8_t* encryption_key, size_t key_len);
int kapi_vfs_mount_compressed(const char* source, const char* target,
                            const char* fstype, uint64_t flags,
                            int compression_level);

#ifdef __cplusplus
}
#endif

#endif