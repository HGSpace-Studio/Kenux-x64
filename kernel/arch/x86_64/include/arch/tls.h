#ifndef ARCH_X86_64_TLS_H
#define ARCH_X86_64_TLS_H

#include <arch/types.h>

#define TLS_VERSION_1_2  0x0303
#define TLS_VERSION_1_3  0x0304

#define TLS_CIPHER_AES_128_GCM_SHA256   0x1301
#define TLS_CIPHER_AES_256_GCM_SHA384   0x1302
#define TLS_CIPHER_CHACHA20_POLY1305    0x1303
#define TLS_CIPHER_AES_128_CBC_SHA256   0x003C
#define TLS_CIPHER_AES_256_CBC_SHA384   0x003D

#define TLS_CONTENT_HANDSHAKE       22
#define TLS_CONTENT_APPLICATION     23
#define TLS_CONTENT_ALERT           21
#define TLS_CONTENT_CHANGE_CIPHER  20

#define TLS_HANDSHAKE_CLIENT_HELLO     1
#define TLS_HANDSHAKE_SERVER_HELLO     2
#define TLS_HANDSHAKE_CERTIFICATE      11
#define TLS_HANDSHAKE_SERVER_KEY_EXCH  12
#define TLS_HANDSHAKE_CERT_REQUEST     13
#define TLS_HANDSHAKE_SERVER_HELLO_DONE 14
#define TLS_HANDSHAKE_CLIENT_KEY_EXCH  16
#define TLS_HANDSHAKE_FINISHED         20

#define TLS_ALERT_WARNING    1
#define TLS_ALERT_FATAL      2

#define TLS_STATE_INIT          0
#define TLS_STATE_HELLO_SENT   1
#define TLS_STATE_HELLO_RECV   2
#define TLS_STATE_CERT_RECV    3
#define TLS_STATE_KEY_EXCH     4
#define TLS_STATE_READY        5
#define TLS_STATE_CLOSED       6
#define TLS_STATE_ERROR        7

#define TLS_MAX_RECORD_SIZE    16384
#define TLS_MAX_CIPHERS        16
#define TLS_MAX_CERT_LEN       4096
#define TLS_KEY_LEN            32
#define TLS_IV_LEN             12
#define TLS_TAG_LEN            16
#define TLS_NONCE_LEN          12

typedef struct {
    uint8_t content_type;
    uint16_t version;
    uint16_t length;
} __attribute__((packed)) tls_record_header_t;

typedef struct {
    uint8_t type;
    uint8_t length[3];
} __attribute__((packed)) tls_handshake_header_t;

typedef struct {
    uint8_t random[32];
    uint8_t session_id[32];
    uint8_t session_id_len;
    uint16_t cipher_suites[TLS_MAX_CIPHERS];
    uint16_t cipher_count;
    uint8_t compression;
} tls_client_hello_t;

typedef struct {
    uint8_t random[32];
    uint8_t session_id[32];
    uint8_t session_id_len;
    uint16_t cipher_suite;
    uint8_t compression;
} tls_server_hello_t;

typedef struct {
    uint8_t key[TLS_KEY_LEN];
    uint8_t iv[TLS_IV_LEN];
    uint8_t tag[TLS_TAG_LEN];
} tls_cipher_state_t;

typedef struct {
    int state;
    uint16_t version;
    uint16_t cipher_suite;
    int is_client;
    int socket_fd;

    uint8_t client_random[32];
    uint8_t server_random[32];
    uint8_t premaster_secret[48];
    uint8_t master_secret[48];

    tls_cipher_state_t encrypt;
    tls_cipher_state_t decrypt;

    uint8_t* cert_data;
    uint32_t cert_len;

    uint64_t seq_read;
    uint64_t seq_write;

    uint8_t* recv_buf;
    uint32_t recv_len;
    uint32_t recv_cap;

    void (*on_alert)(int level, int desc);
    void (*on_handshake_done)(void);
    void* user_data;
} tls_session_t;

typedef struct {
    int (*connect)(int fd);
    int (*accept)(int fd);
    int (*read)(tls_session_t* sess, void* buf, int len);
    int (*write)(tls_session_t* sess, const void* buf, int len);
    int (*close)(tls_session_t* sess);
} tls_ops_t;

void tls_init(void);
tls_session_t* tls_session_create(int is_client);
void tls_session_destroy(tls_session_t* sess);
int tls_connect(tls_session_t* sess, int socket_fd);
int tls_accept(tls_session_t* sess, int socket_fd);
int tls_read(tls_session_t* sess, void* buf, int len);
int tls_write(tls_session_t* sess, const void* buf, int len);
int tls_close(tls_session_t* sess);
int tls_get_state(tls_session_t* sess);
int tls_set_cipher(tls_session_t* sess, uint16_t cipher);
int tls_load_cert(tls_session_t* sess, const void* cert, uint32_t cert_len);
int tls_load_key(tls_session_t* sess, const void* key, uint32_t key_len);

void tls_aes128_gcm_encrypt(const uint8_t* key, const uint8_t* iv,
                            const uint8_t* aad, uint32_t aad_len,
                            const uint8_t* plain, uint32_t plain_len,
                            uint8_t* cipher, uint8_t* tag);
int tls_aes128_gcm_decrypt(const uint8_t* key, const uint8_t* iv,
                            const uint8_t* aad, uint32_t aad_len,
                            const uint8_t* cipher, uint32_t cipher_len,
                            const uint8_t* tag, uint8_t* plain);
void tls_sha256(const void* data, uint32_t len, uint8_t* out);
void tls_sha384(const void* data, uint32_t len, uint8_t* out);
void tls_hmac_sha256(const uint8_t* key, uint32_t key_len,
                     const void* data, uint32_t data_len, uint8_t* out);
void tls_prf(const uint8_t* secret, uint32_t secret_len,
             const char* label,
             const uint8_t* seed, uint32_t seed_len,
             uint8_t* out, uint32_t out_len);

#endif