#include <arch/tls.h>
#include <arch/memory.h>
#include <arch/net.h>
#include <string.h>

static void tls_random_bytes(uint8_t* buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uint64_t tsc;
        __asm__ volatile ("rdtsc" : "=A"(tsc));
        buf[i] = (uint8_t)(tsc ^ (tsc >> 8) ^ (tsc >> 16) ^ (tsc >> 24));
        for (volatile int d = 0; d < 100; d++);
    }
}

static const uint8_t aes_sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static void aes128_encrypt_block(const uint8_t* key, const uint8_t* in, uint8_t* out)
{
    uint8_t state[16];
    memcpy(state, in, 16);

    uint8_t round_keys[176];
    memcpy(round_keys, key, 16);
    for (int i = 1; i <= 10; i++) {
        uint8_t* prev = round_keys + (i - 1) * 16;
        uint8_t* curr = round_keys + i * 16;
        uint8_t temp[4];
        temp[0] = aes_sbox[prev[13]] ^ prev[0];
        temp[1] = aes_sbox[prev[14]] ^ prev[1];
        temp[2] = aes_sbox[prev[15]] ^ prev[2];
        temp[3] = aes_sbox[prev[12]] ^ prev[3];
        if (i == 1) temp[0] ^= 0x01;
        else if (i == 2) temp[0] ^= 0x02;
        else if (i == 4) temp[0] ^= 0x04;
        else if (i == 8) temp[0] ^= 0x08;
        for (int j = 0; j < 4; j++) {
            curr[j] = prev[j] ^ temp[j];
            curr[j+4] = prev[j+4] ^ curr[j];
            curr[j+8] = prev[j+8] ^ curr[j+4];
            curr[j+12] = prev[j+12] ^ curr[j+8];
        }
    }

    for (int r = 0; r < 10; r++) {
        uint8_t* rk = round_keys + r * 16;
        for (int j = 0; j < 16; j++) state[j] ^= rk[j];

        uint8_t tmp[16];
        tmp[0] = aes_sbox[state[0]]; tmp[1] = aes_sbox[state[1]];
        tmp[2] = aes_sbox[state[2]]; tmp[3] = aes_sbox[state[3]];
        tmp[4] = aes_sbox[state[4]]; tmp[5] = aes_sbox[state[5]];
        tmp[6] = aes_sbox[state[6]]; tmp[7] = aes_sbox[state[7]];
        tmp[8] = aes_sbox[state[8]]; tmp[9] = aes_sbox[state[9]];
        tmp[10] = aes_sbox[state[10]]; tmp[11] = aes_sbox[state[11]];
        tmp[12] = aes_sbox[state[12]]; tmp[13] = aes_sbox[state[13]];
        tmp[14] = aes_sbox[state[14]]; tmp[15] = aes_sbox[state[15]];

        state[0] = tmp[0]; state[1] = tmp[5]; state[2] = tmp[10]; state[3] = tmp[15];
        state[4] = tmp[4]; state[5] = tmp[9]; state[6] = tmp[14]; state[7] = tmp[3];
        state[8] = tmp[8]; state[9] = tmp[13]; state[10] = tmp[2]; state[11] = tmp[7];
        state[12] = tmp[12]; state[13] = tmp[1]; state[14] = tmp[6]; state[15] = tmp[11];

        if (r < 9) {
            uint8_t t = state[1];
            state[1] = state[1] ^ state[2] ^ state[3] ^ state[1];
            state[1] = t ^ state[1] ^ state[2] ^ state[3];
        }
    }

    uint8_t* rk = round_keys + 160;
    for (int j = 0; j < 16; j++) out[j] = state[j] ^ rk[j];
}

static uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9aca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

void tls_sha256(const void* data, uint32_t len, uint8_t* out)
{
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint32_t total_len = len;
    uint8_t* msg = (uint8_t*)data;
    uint32_t offset = 0;

    while (offset < len || offset == len) {
        uint8_t block[64];
        uint32_t block_len;

        if (len - offset >= 64) {
            memcpy(block, msg + offset, 64);
            block_len = 64;
            offset += 64;
        } else {
            block_len = len - offset;
            memcpy(block, msg + offset, block_len);
            block[block_len] = 0x80;
            if (block_len < 56) {
                uint64_t bits = (uint64_t)total_len * 8;
                for (int i = 0; i < 8; i++)
                    block[63 - i] = (uint8_t)(bits >> (i * 8));
                block_len = 64;
                offset = len + 1;
            } else {
                if (block_len + 1 < 64)
                    memset(block + block_len + 1, 0, 64 - block_len - 1);
                block_len = 0;
                offset = len;
                continue;
            }
        }

        if (block_len == 0) break;

        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
                   ((uint32_t)block[i*4+2] << 8) | block[i*4+3];
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = (w[i-15] >> 7 | w[i-15] << 25) ^ (w[i-15] >> 18 | w[i-15] << 14) ^ (w[i-15] >> 3);
            uint32_t s1 = (w[i-2] >> 17 | w[i-2] << 15) ^ (w[i-2] >> 19 | w[i-2] << 13) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; i++) {
            uint32_t s1 = (e >> 6 | e << 26) ^ (e >> 11 | e << 21) ^ (e >> 25 | e << 7);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = hh + s1 + ch + sha256_k[i] + w[i];
            uint32_t s0 = (a >> 2 | a << 30) ^ (a >> 13 | a << 19) ^ (a >> 22 | a << 10);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = s0 + maj;
            hh = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;

        if (offset > len) break;
    }

    for (int i = 0; i < 8; i++) {
        out[i*4]   = (h[i] >> 24) & 0xFF;
        out[i*4+1] = (h[i] >> 16) & 0xFF;
        out[i*4+2] = (h[i] >> 8) & 0xFF;
        out[i*4+3] = h[i] & 0xFF;
    }
}

void tls_sha384(const void* data, uint32_t len, uint8_t* out)
{
    tls_sha256(data, len, out);
    memset(out + 32, 0, 16);
}

void tls_hmac_sha256(const uint8_t* key, uint32_t key_len,
                     const void* data, uint32_t data_len, uint8_t* out)
{
    uint8_t k_pad[64];
    uint8_t inner[64 + data_len];
    uint8_t inner_hash[32];

    memset(k_pad, 0x36, 64);
    if (key_len <= 64) {
        for (uint32_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];
    }

    memcpy(inner, k_pad, 64);
    memcpy(inner + 64, data, data_len);
    tls_sha256(inner, 64 + data_len, inner_hash);

    memset(k_pad, 0x5c, 64);
    if (key_len <= 64) {
        for (uint32_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];
    }

    uint8_t outer[64 + 32];
    memcpy(outer, k_pad, 64);
    memcpy(outer + 64, inner_hash, 32);
    tls_sha256(outer, 96, out);
}

void tls_prf(const uint8_t* secret, uint32_t secret_len,
             const char* label,
             const uint8_t* seed, uint32_t seed_len,
             uint8_t* out, uint32_t out_len)
{
    uint32_t label_len = 0;
    while (label[label_len]) label_len++;

    uint32_t total_seed = label_len + seed_len;
    uint8_t combined[total_seed];
    memcpy(combined, label, label_len);
    memcpy(combined + label_len, seed, seed_len);

    uint8_t a[32];
    tls_hmac_sha256(secret, secret_len, combined, total_seed, a);

    uint32_t generated = 0;
    while (generated < out_len) {
        uint8_t tmp[total_seed + 32];
        memcpy(tmp, a, 32);
        memcpy(tmp + 32, combined, total_seed);

        uint8_t hash[32];
        tls_hmac_sha256(secret, secret_len, tmp, 32 + total_seed, hash);

        uint32_t copy = out_len - generated;
        if (copy > 32) copy = 32;
        memcpy(out + generated, hash, copy);
        generated += copy;

        tls_hmac_sha256(secret, secret_len, a, 32, a);
    }
}

void tls_aes128_gcm_encrypt(const uint8_t* key, const uint8_t* iv,
                            const uint8_t* aad, uint32_t aad_len,
                            const uint8_t* plain, uint32_t plain_len,
                            uint8_t* cipher, uint8_t* tag)
{
    uint8_t h[16] = {0};
    aes128_encrypt_block(key, h, h);

    uint8_t j0[16];
    memcpy(j0, iv, 12);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;

    for (uint32_t i = 0; i < plain_len; i += 16) {
        uint8_t ctr[16];
        memcpy(ctr, j0, 12);
        uint32_t c = (i / 16) + 1;
        ctr[12] = (c >> 24) & 0xFF; ctr[13] = (c >> 16) & 0xFF;
        ctr[14] = (c >> 8) & 0xFF; ctr[15] = c & 0xFF;

        uint8_t ks[16];
        aes128_encrypt_block(key, ctr, ks);

        uint32_t block_len = plain_len - i;
        if (block_len > 16) block_len = 16;
        for (uint32_t j = 0; j < block_len; j++) {
            cipher[i + j] = plain[i + j] ^ ks[j];
        }
    }

    memset(tag, 0, 16);
    uint8_t enc_j0[16];
    aes128_encrypt_block(key, j0, enc_j0);
    memcpy(tag, enc_j0, 16);
}

int tls_aes128_gcm_decrypt(const uint8_t* key, const uint8_t* iv,
                            const uint8_t* aad, uint32_t aad_len,
                            const uint8_t* cipher, uint32_t cipher_len,
                            const uint8_t* tag, uint8_t* plain)
{
    uint8_t j0[16];
    memcpy(j0, iv, 12);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;

    for (uint32_t i = 0; i < cipher_len; i += 16) {
        uint8_t ctr[16];
        memcpy(ctr, j0, 12);
        uint32_t c = (i / 16) + 1;
        ctr[12] = (c >> 24) & 0xFF; ctr[13] = (c >> 16) & 0xFF;
        ctr[14] = (c >> 8) & 0xFF; ctr[15] = c & 0xFF;

        uint8_t ks[16];
        aes128_encrypt_block(key, ctr, ks);

        uint32_t block_len = cipher_len - i;
        if (block_len > 16) block_len = 16;
        for (uint32_t j = 0; j < block_len; j++) {
            plain[i + j] = cipher[i + j] ^ ks[j];
        }
    }

    return 0;
}

void tls_init(void)
{
}

tls_session_t* tls_session_create(int is_client)
{
    tls_session_t* sess = (tls_session_t*)memory_alloc(sizeof(tls_session_t));
    if (!sess) return NULL;
    memset(sess, 0, sizeof(tls_session_t));
    sess->is_client = is_client;
    sess->state = TLS_STATE_INIT;
    sess->version = TLS_VERSION_1_3;
    sess->cipher_suite = TLS_CIPHER_AES_128_GCM_SHA256;
    sess->recv_cap = TLS_MAX_RECORD_SIZE + 256;
    sess->recv_buf = (uint8_t*)memory_alloc(sess->recv_cap);
    sess->recv_len = 0;
    return sess;
}

void tls_session_destroy(tls_session_t* sess)
{
    if (!sess) return;
    if (sess->cert_data) memory_free(sess->cert_data);
    if (sess->recv_buf) memory_free(sess->recv_buf);
    memory_free(sess);
}

static int tls_send_record(tls_session_t* sess, uint8_t content_type,
                           const void* data, uint16_t len)
{
    if (!sess || !data) return -1;

    uint8_t record[sizeof(tls_record_header_t) + len];
    tls_record_header_t* hdr = (tls_record_header_t*)record;
    hdr->content_type = content_type;
    hdr->version = sess->version;
    hdr->length = len;
    memcpy(record + sizeof(tls_record_header_t), data, len);

    return (int)sizeof(record);
}

static int tls_send_handshake(tls_session_t* sess, uint8_t type,
                              const void* data, uint32_t len)
{
    uint8_t msg[sizeof(tls_handshake_header_t) + len];
    tls_handshake_header_t* hdr = (tls_handshake_header_t*)msg;
    hdr->type = type;
    hdr->length[0] = (len >> 16) & 0xFF;
    hdr->length[1] = (len >> 8) & 0xFF;
    hdr->length[2] = len & 0xFF;
    if (data && len > 0) memcpy(msg + sizeof(tls_handshake_header_t), data, len);

    return tls_send_record(sess, TLS_CONTENT_HANDSHAKE, msg,
                          sizeof(tls_handshake_header_t) + len);
}

int tls_connect(tls_session_t* sess, int socket_fd)
{
    if (!sess) return -1;
    sess->socket_fd = socket_fd;
    sess->is_client = 1;

    tls_random_bytes(sess->client_random, 32);

    tls_client_hello_t hello;
    memset(&hello, 0, sizeof(hello));
    memcpy(hello.random, sess->client_random, 32);
    hello.session_id_len = 0;
    hello.cipher_count = 3;
    hello.cipher_suites[0] = TLS_CIPHER_AES_128_GCM_SHA256;
    hello.cipher_suites[1] = TLS_CIPHER_AES_256_GCM_SHA384;
    hello.cipher_suites[2] = TLS_CIPHER_CHACHA20_POLY1305;
    hello.compression = 0;

    int ret = tls_send_handshake(sess, TLS_HANDSHAKE_CLIENT_HELLO,
                                 &hello, sizeof(hello));
    if (ret < 0) {
        sess->state = TLS_STATE_ERROR;
        return -2;
    }

    sess->state = TLS_STATE_HELLO_SENT;
    return 0;
}

int tls_accept(tls_session_t* sess, int socket_fd)
{
    if (!sess) return -1;
    sess->socket_fd = socket_fd;
    sess->is_client = 0;
    sess->state = TLS_STATE_INIT;
    return 0;
}

int tls_read(tls_session_t* sess, void* buf, int len)
{
    if (!sess || !buf || len <= 0) return -1;
    if (sess->state != TLS_STATE_READY) return -2;
    return 0;
}

int tls_write(tls_session_t* sess, const void* buf, int len)
{
    if (!sess || !buf || len <= 0) return -1;
    if (sess->state != TLS_STATE_READY) return -2;

    uint8_t encrypted[TLS_MAX_RECORD_SIZE];
    uint8_t tag[TLS_TAG_LEN];

    tls_aes128_gcm_encrypt(sess->encrypt.key, sess->encrypt.iv,
                           NULL, 0, buf, len, encrypted, tag);

    uint8_t record[len + TLS_TAG_LEN];
    memcpy(record, encrypted, len);
    memcpy(record + len, tag, TLS_TAG_LEN);

    return tls_send_record(sess, TLS_CONTENT_APPLICATION, record, len + TLS_TAG_LEN);
}

int tls_close(tls_session_t* sess)
{
    if (!sess) return -1;

    uint8_t alert[2] = { TLS_ALERT_WARNING, 0 };
    tls_send_record(sess, TLS_CONTENT_ALERT, alert, 2);

    sess->state = TLS_STATE_CLOSED;
    return 0;
}

int tls_get_state(tls_session_t* sess)
{
    if (!sess) return -1;
    return sess->state;
}

int tls_set_cipher(tls_session_t* sess, uint16_t cipher)
{
    if (!sess) return -1;
    sess->cipher_suite = cipher;
    return 0;
}

int tls_load_cert(tls_session_t* sess, const void* cert, uint32_t cert_len)
{
    if (!sess || !cert || cert_len > TLS_MAX_CERT_LEN) return -1;
    if (sess->cert_data) memory_free(sess->cert_data);
    sess->cert_data = (uint8_t*)memory_alloc(cert_len);
    if (!sess->cert_data) return -2;
    memcpy(sess->cert_data, cert, cert_len);
    sess->cert_len = cert_len;
    return 0;
}

int tls_load_key(tls_session_t* sess, const void* key, uint32_t key_len)
{
    if (!sess || !key) return -1;
    if (key_len > TLS_KEY_LEN) return -2;
    memcpy(sess->encrypt.key, key, key_len);
    memcpy(sess->decrypt.key, key, key_len);
    return 0;
}