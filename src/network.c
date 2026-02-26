#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSE_SOCKET close
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

#define NETWORK_DEFAULT_PASSPHRASE "tictactoe-cx-secure-lan"
#define NETWORK_HANDSHAKE_MAGIC 0x48584354u
#define NETWORK_FRAME_MAGIC 0x46584354u
#define NETWORK_PROTOCOL_VERSION 1u

#define PBKDF2_ITERS 200000
#define GCM_TAG_SIZE 16u
#define PACKET_BYTES ((size_t)sizeof(NetworkPacket))
#define FRAME_SIZE (4u + 8u + PACKET_BYTES + GCM_TAG_SIZE)

#define HANDSHAKE_CLIENT_HELLO 1u
#define HANDSHAKE_SERVER_HELLO 2u

#define HANDSHAKE_CLIENT_SIZE (4u + 1u + 1u + 2u + 16u + 12u + 4u + 32u)
#define HANDSHAKE_SERVER_SIZE (4u + 1u + 1u + 2u + 16u + 12u + 12u + 4u + 32u)

typedef struct {
    uint32_t magic;
    uint8_t type;
    uint8_t reserved[3];
    uint64_t client_nonce;
    uint64_t server_nonce;
    uint32_t proof;
    uint32_t reserved2;
} LegacyHandshakePacket;

typedef struct {
    uint32_t magic;
    uint32_t nonce;
    uint32_t mac;
    NetworkPacket packet;
} LegacySecureFrame;

static void copy_cstr_trunc(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    for (; i + 1 < dst_size && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void close_socket_if_open(int* sockfd) {
    if (!sockfd || *sockfd == INVALID_SOCKET) return;
    CLOSE_SOCKET(*sockfd);
    *sockfd = INVALID_SOCKET;
}

static void reset_security_state(Network* net) {
    if (!net) return;

    net->security_ready = false;
    net->security_mode = NET_SECURITY_NONE;

    memset(net->send_key, 0, sizeof(net->send_key));
    memset(net->recv_key, 0, sizeof(net->recv_key));
    memset(net->send_iv_prefix, 0, sizeof(net->send_iv_prefix));
    memset(net->recv_iv_prefix, 0, sizeof(net->recv_iv_prefix));
    net->tx_seq = 0;
    net->rx_seq = 0;

    net->legacy_session_key = 0;
    net->legacy_tx_nonce = 0;
    net->legacy_rx_nonce = 0;
}

static void write_u32_be(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static uint32_t read_u32_be(const uint8_t* in) {
    return ((uint32_t)in[0] << 24) |
           ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) |
           (uint32_t)in[3];
}

static void write_u64_be(uint8_t* out, uint64_t value) {
    out[0] = (uint8_t)(value >> 56);
    out[1] = (uint8_t)(value >> 48);
    out[2] = (uint8_t)(value >> 40);
    out[3] = (uint8_t)(value >> 32);
    out[4] = (uint8_t)(value >> 24);
    out[5] = (uint8_t)(value >> 16);
    out[6] = (uint8_t)(value >> 8);
    out[7] = (uint8_t)value;
}

static uint64_t read_u64_be(const uint8_t* in) {
    return ((uint64_t)in[0] << 56) |
           ((uint64_t)in[1] << 48) |
           ((uint64_t)in[2] << 40) |
           ((uint64_t)in[3] << 32) |
           ((uint64_t)in[4] << 24) |
           ((uint64_t)in[5] << 16) |
           ((uint64_t)in[6] << 8) |
           (uint64_t)in[7];
}

static int wait_socket_readable(int sockfd, int timeout_ms) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval timeout;
    struct timeval* timeout_ptr = NULL;
    if (timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        timeout_ptr = &timeout;
    }

    return select((int)(sockfd + 1), &readfds, NULL, NULL, timeout_ptr);
}

static void sleep_ms(int milliseconds) {
    if (milliseconds <= 0) {
        return;
    }

    struct timeval delay;
    delay.tv_sec = milliseconds / 1000;
    delay.tv_usec = (milliseconds % 1000) * 1000;
    (void)select(0, NULL, NULL, NULL, &delay);
}

static int peek_with_min_bytes(int sockfd,
                               uint8_t* out,
                               size_t out_len,
                               size_t min_bytes,
                               int timeout_ms) {
    if (!out || min_bytes == 0 || out_len < min_bytes || out_len > (size_t)INT_MAX) {
        return -1;
    }

    const int poll_interval_ms = 10;
    int elapsed_ms = 0;

    while (timeout_ms < 0 || elapsed_ms <= timeout_ms) {
        int got = recv(sockfd, (char*)out, (int)out_len, MSG_PEEK);
        if (got >= (int)min_bytes) {
            return got;
        }
        if (got <= 0) {
            return -1;
        }

        if (timeout_ms >= 0 && elapsed_ms >= timeout_ms) {
            break;
        }

        sleep_ms(poll_interval_ms);
        elapsed_ms += poll_interval_ms;
    }

    return -1;
}

static bool send_all(int sockfd, const char* data, size_t len) {
    size_t sent_total = 0;
    while (sent_total < len) {
        int sent = send(sockfd, data + sent_total, (int)(len - sent_total), 0);
        if (sent <= 0) return false;
        sent_total += (size_t)sent;
    }
    return true;
}

static bool recv_all(int sockfd, char* data, size_t len) {
    size_t recv_total = 0;
    while (recv_total < len) {
        int received = recv(sockfd, data + recv_total, (int)(len - recv_total), 0);
        if (received <= 0) return false;
        recv_total += (size_t)received;
    }
    return true;
}

static bool random_bytes(uint8_t* out, size_t len) {
    if (!out || len > INT_MAX) return false;
    return RAND_bytes(out, (int)len) == 1;
}

static bool hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[32]) {
    if (!key || !msg || !out || key_len > INT_MAX || msg_len > UINT_MAX) return false;

    unsigned int out_len = 0;
    unsigned char* result = HMAC(EVP_sha256(),
                                 key,
                                 (int)key_len,
                                 msg,
                                 (unsigned int)msg_len,
                                 out,
                                 &out_len);
    return result != NULL && out_len == 32;
}

static bool secure_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    if (!a || !b) return false;
    return CRYPTO_memcmp(a, b, len) == 0;
}

static bool derive_base_key(const char* passphrase, const uint8_t salt[16], uint8_t out_key[32]) {
    const char* secret = (passphrase && passphrase[0] != '\0') ? passphrase : NETWORK_DEFAULT_PASSPHRASE;
    return PKCS5_PBKDF2_HMAC(secret,
                             (int)strlen(secret),
                             salt,
                             16,
                             PBKDF2_ITERS,
                             EVP_sha256(),
                             32,
                             out_key) == 1;
}

static bool derive_direction_key(const uint8_t base_key[32],
                                 const char label[3],
                                 const uint8_t client_nonce[12],
                                 const uint8_t server_nonce[12],
                                 uint8_t out_key[32]) {
    uint8_t material[3 + 12 + 12];
    memcpy(material, label, 3);
    memcpy(material + 3, client_nonce, 12);
    memcpy(material + 15, server_nonce, 12);
    return hmac_sha256(base_key, 32, material, sizeof(material), out_key);
}

static bool derive_session_keys(const uint8_t base_key[32],
                                const uint8_t client_nonce[12],
                                const uint8_t server_nonce[12],
                                uint8_t key_c2s[32],
                                uint8_t key_s2c[32]) {
    const char c2s[3] = {'C', '2', 'S'};
    const char s2c[3] = {'S', '2', 'C'};
    return derive_direction_key(base_key, c2s, client_nonce, server_nonce, key_c2s) &&
           derive_direction_key(base_key, s2c, client_nonce, server_nonce, key_s2c);
}

static void build_iv(const uint8_t prefix[4], uint64_t seq, uint8_t iv[12]) {
    memcpy(iv, prefix, 4);
    write_u64_be(iv + 4, seq);
}

static bool aes_gcm_encrypt(const uint8_t key[32],
                            const uint8_t iv[12],
                            const uint8_t* plaintext,
                            int plaintext_len,
                            uint8_t* ciphertext,
                            uint8_t tag[GCM_TAG_SIZE]) {
    bool ok = false;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    int out_len = 0;
    int final_len = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) goto cleanup;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1) goto cleanup;
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) goto cleanup;
    if (EVP_EncryptUpdate(ctx, ciphertext, &out_len, plaintext, plaintext_len) != 1) goto cleanup;
    if (out_len != plaintext_len) goto cleanup;
    if (EVP_EncryptFinal_ex(ctx, ciphertext + out_len, &final_len) != 1) goto cleanup;
    if (final_len != 0) goto cleanup;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, tag) != 1) goto cleanup;

    ok = true;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

static bool aes_gcm_decrypt(const uint8_t key[32],
                            const uint8_t iv[12],
                            const uint8_t* ciphertext,
                            int ciphertext_len,
                            const uint8_t tag[GCM_TAG_SIZE],
                            uint8_t* plaintext) {
    bool ok = false;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    int out_len = 0;
    int final_len = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) goto cleanup;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1) goto cleanup;
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) goto cleanup;
    if (EVP_DecryptUpdate(ctx, plaintext, &out_len, ciphertext, ciphertext_len) != 1) goto cleanup;
    if (out_len != ciphertext_len) goto cleanup;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, (void*)tag) != 1) goto cleanup;
    if (EVP_DecryptFinal_ex(ctx, plaintext + out_len, &final_len) != 1) goto cleanup;
    if (final_len != 0) goto cleanup;

    ok = true;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

static uint64_t rotl64(uint64_t x, int shift) {
    return (x << shift) | (x >> (64 - shift));
}

static uint64_t mix64(uint64_t value) {
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return value;
}

static uint64_t fnv1a64(const unsigned char* data, size_t len) {
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint64_t derive_legacy_key_seed(const char* passphrase) {
    const char* source = (passphrase && passphrase[0] != '\0') ? passphrase : NETWORK_DEFAULT_PASSPHRASE;
    return mix64(fnv1a64((const unsigned char*)source, strlen(source)) ^ 0x93b82a5f21f5d97dULL);
}

static uint64_t generate_nonce64(const Network* net) {
    uint64_t nonce = (uint64_t)(uintptr_t)net;
    nonce ^= ((uint64_t)time(NULL) << 32);
    nonce ^= (uint64_t)clock();
    nonce ^= ((uint64_t)(rand() & 0xffff) << 48);
    nonce ^= ((uint64_t)(rand() & 0xffff) << 32);
    nonce ^= ((uint64_t)(rand() & 0xffff) << 16);
    nonce ^= (uint64_t)(rand() & 0xffff);
    return mix64(nonce);
}

static uint32_t legacy_handshake_proof(uint64_t key_seed, uint8_t type, uint64_t client_nonce, uint64_t server_nonce) {
    uint64_t material = key_seed;
    material ^= mix64(client_nonce);
    material ^= rotl64(mix64(server_nonce), 17);
    material ^= ((uint64_t)type << 56);
    return (uint32_t)(mix64(material) & 0xffffffffu);
}

static uint64_t derive_legacy_session_key(uint64_t key_seed, uint64_t client_nonce, uint64_t server_nonce) {
    uint64_t material = key_seed;
    material ^= rotl64(client_nonce, 23);
    material ^= rotl64(server_nonce, 41);
    material ^= 0x9e3779b97f4a7c15ULL;
    return mix64(material);
}

static uint64_t legacy_stream_next(uint64_t* state) {
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * 2685821657736338717ULL;
}

static void legacy_crypt_buffer(uint8_t* data, size_t len, uint64_t session_key, uint32_t nonce) {
    uint64_t state = mix64(session_key ^ (((uint64_t)nonce << 32) | nonce) ^ 0xd6e8feb86659fd93ULL);
    size_t idx = 0;

    while (idx < len) {
        uint64_t block = legacy_stream_next(&state);
        for (int i = 0; i < 8 && idx < len; i++, idx++) {
            data[idx] ^= (uint8_t)(block >> (i * 8));
        }
    }
}

static uint32_t legacy_frame_mac(uint64_t session_key, uint32_t nonce, const uint8_t* data, size_t len) {
    uint64_t hash = 1469598103934665603ULL ^ mix64(session_key ^ nonce);
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)data[i];
        hash *= 1099511628211ULL;
    }
    hash ^= len;
    hash = mix64(hash ^ rotl64(session_key, 29));
    return (uint32_t)(hash & 0xffffffffu);
}
static bool send_openssl_packet(Network* net, const NetworkPacket* pkt) {
    if (!net || !pkt || !net->connected || net->sockfd == INVALID_SOCKET || !net->security_ready) {
        return false;
    }

    if (net->tx_seq == UINT64_MAX) {
        return false;
    }

    uint64_t seq = ++net->tx_seq;
    uint8_t iv[12];
    build_iv(net->send_iv_prefix, seq, iv);

    uint8_t frame[FRAME_SIZE];
    uint8_t* cipher_out = frame + 12;
    uint8_t* tag_out = cipher_out + PACKET_BYTES;

    if (!aes_gcm_encrypt(net->send_key, iv, (const uint8_t*)pkt, (int)PACKET_BYTES, cipher_out, tag_out)) {
        return false;
    }

    write_u32_be(frame, NETWORK_FRAME_MAGIC);
    write_u64_be(frame + 4, seq);

    return send_all(net->sockfd, (const char*)frame, sizeof(frame));
}

static bool receive_openssl_packet(Network* net, NetworkPacket* pkt, int timeout_ms) {
    if (!net || !pkt || !net->connected || net->sockfd == INVALID_SOCKET || !net->security_ready) {
        return false;
    }

    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) {
        return false;
    }

    uint8_t frame[FRAME_SIZE];
    if (!recv_all(net->sockfd, (char*)frame, sizeof(frame))) {
        return false;
    }

    uint32_t magic = read_u32_be(frame);
    uint64_t seq = read_u64_be(frame + 4);
    const uint8_t* cipher_in = frame + 12;
    const uint8_t* tag_in = cipher_in + PACKET_BYTES;

    if (magic != NETWORK_FRAME_MAGIC || seq == 0 || seq <= net->rx_seq) {
        return false;
    }

    uint8_t iv[12];
    build_iv(net->recv_iv_prefix, seq, iv);

    if (!aes_gcm_decrypt(net->recv_key, iv, cipher_in, (int)PACKET_BYTES, tag_in, (uint8_t*)pkt)) {
        return false;
    }

    net->rx_seq = seq;
    return true;
}

static bool send_legacy_packet(Network* net, const NetworkPacket* pkt) {
    if (!net || !pkt || !net->connected || net->sockfd == INVALID_SOCKET || !net->security_ready) {
        return false;
    }

    LegacySecureFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.magic = NETWORK_FRAME_MAGIC;
    frame.nonce = ++net->legacy_tx_nonce;
    if (frame.nonce == 0) {
        frame.nonce = ++net->legacy_tx_nonce;
    }

    memcpy(&frame.packet, pkt, sizeof(frame.packet));
    legacy_crypt_buffer((uint8_t*)&frame.packet, sizeof(frame.packet), net->legacy_session_key, frame.nonce);
    frame.mac = legacy_frame_mac(net->legacy_session_key, frame.nonce, (const uint8_t*)&frame.packet, sizeof(frame.packet));

    return send_all(net->sockfd, (const char*)&frame, sizeof(frame));
}

static bool receive_legacy_packet(Network* net, NetworkPacket* pkt, int timeout_ms) {
    if (!net || !pkt || !net->connected || net->sockfd == INVALID_SOCKET || !net->security_ready) {
        return false;
    }

    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) {
        return false;
    }

    LegacySecureFrame frame;
    if (!recv_all(net->sockfd, (char*)&frame, sizeof(frame))) {
        return false;
    }

    if (frame.magic != NETWORK_FRAME_MAGIC || frame.nonce <= net->legacy_rx_nonce) {
        return false;
    }

    uint32_t expected = legacy_frame_mac(net->legacy_session_key, frame.nonce, (const uint8_t*)&frame.packet, sizeof(frame.packet));
    if (expected != frame.mac) {
        return false;
    }

    *pkt = frame.packet;
    legacy_crypt_buffer((uint8_t*)pkt, sizeof(*pkt), net->legacy_session_key, frame.nonce);
    net->legacy_rx_nonce = frame.nonce;
    return true;
}

static bool send_packet(Network* net, const NetworkPacket* pkt) {
    if (!network_is_secure(net)) {
        return false;
    }

    if (net->security_mode == NET_SECURITY_OPENSSL) {
        return send_openssl_packet(net, pkt);
    }
    if (net->security_mode == NET_SECURITY_LEGACY) {
        return send_legacy_packet(net, pkt);
    }
    return false;
}

static bool receive_packet(Network* net, NetworkPacket* pkt, int timeout_ms) {
    if (!network_is_secure(net)) {
        return false;
    }

    if (net->security_mode == NET_SECURITY_OPENSSL) {
        return receive_openssl_packet(net, pkt, timeout_ms);
    }
    if (net->security_mode == NET_SECURITY_LEGACY) {
        return receive_legacy_packet(net, pkt, timeout_ms);
    }
    return false;
}

static bool host_perform_openssl_handshake(Network* net, int timeout_ms) {
    uint8_t client_msg[HANDSHAKE_CLIENT_SIZE];
    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) return false;
    if (!recv_all(net->sockfd, (char*)client_msg, sizeof(client_msg))) return false;

    if (read_u32_be(client_msg) != NETWORK_HANDSHAKE_MAGIC ||
        client_msg[4] != HANDSHAKE_CLIENT_HELLO ||
        client_msg[5] != NETWORK_PROTOCOL_VERSION) {
        return false;
    }

    const uint8_t* salt = client_msg + 8;
    const uint8_t* client_nonce = client_msg + 24;
    const uint8_t* client_prefix = client_msg + 36;
    const uint8_t* client_hmac = client_msg + 40;

    uint8_t base_key[32];
    if (!derive_base_key(net->passphrase, salt, base_key)) return false;

    uint8_t expected_hmac[32];
    if (!hmac_sha256(base_key, sizeof(base_key), client_msg, 40, expected_hmac) ||
        !secure_equal(expected_hmac, client_hmac, sizeof(expected_hmac))) {
        return false;
    }

    uint8_t server_nonce[12];
    uint8_t server_prefix[4];
    if (!random_bytes(server_nonce, sizeof(server_nonce)) ||
        !random_bytes(server_prefix, sizeof(server_prefix))) {
        return false;
    }

    uint8_t server_msg[HANDSHAKE_SERVER_SIZE];
    memset(server_msg, 0, sizeof(server_msg));
    write_u32_be(server_msg, NETWORK_HANDSHAKE_MAGIC);
    server_msg[4] = HANDSHAKE_SERVER_HELLO;
    server_msg[5] = NETWORK_PROTOCOL_VERSION;
    memcpy(server_msg + 8, salt, 16);
    memcpy(server_msg + 24, client_nonce, 12);
    memcpy(server_msg + 36, server_nonce, 12);
    memcpy(server_msg + 48, server_prefix, 4);

    if (!hmac_sha256(base_key, sizeof(base_key), server_msg, 52, server_msg + 52)) return false;
    if (!send_all(net->sockfd, (const char*)server_msg, sizeof(server_msg))) return false;

    uint8_t key_c2s[32];
    uint8_t key_s2c[32];
    if (!derive_session_keys(base_key, client_nonce, server_nonce, key_c2s, key_s2c)) return false;

    memcpy(net->send_key, key_s2c, sizeof(net->send_key));
    memcpy(net->recv_key, key_c2s, sizeof(net->recv_key));
    memcpy(net->send_iv_prefix, server_prefix, sizeof(net->send_iv_prefix));
    memcpy(net->recv_iv_prefix, client_prefix, sizeof(net->recv_iv_prefix));

    net->tx_seq = 0;
    net->rx_seq = 0;
    net->security_mode = NET_SECURITY_OPENSSL;
    net->security_ready = true;
    return true;
}

static bool client_perform_openssl_handshake(Network* net, int timeout_ms) {
    uint8_t salt[16];
    uint8_t client_nonce[12];
    uint8_t client_prefix[4];
    if (!random_bytes(salt, sizeof(salt)) ||
        !random_bytes(client_nonce, sizeof(client_nonce)) ||
        !random_bytes(client_prefix, sizeof(client_prefix))) {
        return false;
    }

    uint8_t base_key[32];
    if (!derive_base_key(net->passphrase, salt, base_key)) return false;

    uint8_t client_msg[HANDSHAKE_CLIENT_SIZE];
    memset(client_msg, 0, sizeof(client_msg));
    write_u32_be(client_msg, NETWORK_HANDSHAKE_MAGIC);
    client_msg[4] = HANDSHAKE_CLIENT_HELLO;
    client_msg[5] = NETWORK_PROTOCOL_VERSION;
    memcpy(client_msg + 8, salt, 16);
    memcpy(client_msg + 24, client_nonce, 12);
    memcpy(client_msg + 36, client_prefix, 4);
    if (!hmac_sha256(base_key, sizeof(base_key), client_msg, 40, client_msg + 40)) return false;

    if (!send_all(net->sockfd, (const char*)client_msg, sizeof(client_msg))) return false;

    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) return false;

    uint8_t server_msg[HANDSHAKE_SERVER_SIZE];
    if (!recv_all(net->sockfd, (char*)server_msg, sizeof(server_msg))) return false;

    if (read_u32_be(server_msg) != NETWORK_HANDSHAKE_MAGIC ||
        server_msg[4] != HANDSHAKE_SERVER_HELLO ||
        server_msg[5] != NETWORK_PROTOCOL_VERSION) {
        return false;
    }

    if (!secure_equal(server_msg + 8, salt, sizeof(salt)) ||
        !secure_equal(server_msg + 24, client_nonce, sizeof(client_nonce))) {
        return false;
    }

    uint8_t expected_hmac[32];
    if (!hmac_sha256(base_key, sizeof(base_key), server_msg, 52, expected_hmac) ||
        !secure_equal(expected_hmac, server_msg + 52, sizeof(expected_hmac))) {
        return false;
    }

    const uint8_t* server_nonce = server_msg + 36;
    const uint8_t* server_prefix = server_msg + 48;

    uint8_t key_c2s[32];
    uint8_t key_s2c[32];
    if (!derive_session_keys(base_key, client_nonce, server_nonce, key_c2s, key_s2c)) return false;

    memcpy(net->send_key, key_c2s, sizeof(net->send_key));
    memcpy(net->recv_key, key_s2c, sizeof(net->recv_key));
    memcpy(net->send_iv_prefix, client_prefix, sizeof(net->send_iv_prefix));
    memcpy(net->recv_iv_prefix, server_prefix, sizeof(net->recv_iv_prefix));

    net->tx_seq = 0;
    net->rx_seq = 0;
    net->security_mode = NET_SECURITY_OPENSSL;
    net->security_ready = true;
    return true;
}

static bool host_perform_legacy_handshake(Network* net, int timeout_ms) {
    LegacyHandshakePacket hello;
    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) return false;
    if (!recv_all(net->sockfd, (char*)&hello, sizeof(hello))) return false;

    if (hello.magic != NETWORK_HANDSHAKE_MAGIC || hello.type != HANDSHAKE_CLIENT_HELLO) {
        return false;
    }

    uint32_t expected = legacy_handshake_proof(net->legacy_key_seed, HANDSHAKE_CLIENT_HELLO, hello.client_nonce, 0);
    if (hello.proof != expected) {
        return false;
    }

    LegacyHandshakePacket ack;
    memset(&ack, 0, sizeof(ack));
    ack.magic = NETWORK_HANDSHAKE_MAGIC;
    ack.type = HANDSHAKE_SERVER_HELLO;
    ack.client_nonce = hello.client_nonce;
    ack.server_nonce = generate_nonce64(net);
    ack.proof = legacy_handshake_proof(net->legacy_key_seed, HANDSHAKE_SERVER_HELLO, ack.client_nonce, ack.server_nonce);

    if (!send_all(net->sockfd, (const char*)&ack, sizeof(ack))) {
        return false;
    }

    net->legacy_session_key = derive_legacy_session_key(net->legacy_key_seed, ack.client_nonce, ack.server_nonce);
    net->legacy_tx_nonce = 0;
    net->legacy_rx_nonce = 0;
    net->security_mode = NET_SECURITY_LEGACY;
    net->security_ready = true;
    return true;
}

static bool client_perform_legacy_handshake(Network* net, int timeout_ms) {
    LegacyHandshakePacket hello;
    memset(&hello, 0, sizeof(hello));
    hello.magic = NETWORK_HANDSHAKE_MAGIC;
    hello.type = HANDSHAKE_CLIENT_HELLO;
    hello.client_nonce = generate_nonce64(net);
    hello.proof = legacy_handshake_proof(net->legacy_key_seed, HANDSHAKE_CLIENT_HELLO, hello.client_nonce, 0);

    if (!send_all(net->sockfd, (const char*)&hello, sizeof(hello))) {
        return false;
    }

    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) {
        return false;
    }

    LegacyHandshakePacket ack;
    memset(&ack, 0, sizeof(ack));
    if (!recv_all(net->sockfd, (char*)&ack, sizeof(ack))) {
        return false;
    }

    if (ack.magic != NETWORK_HANDSHAKE_MAGIC ||
        ack.type != HANDSHAKE_SERVER_HELLO ||
        ack.client_nonce != hello.client_nonce) {
        return false;
    }

    uint32_t expected = legacy_handshake_proof(net->legacy_key_seed, HANDSHAKE_SERVER_HELLO, hello.client_nonce, ack.server_nonce);
    if (ack.proof != expected) {
        return false;
    }

    net->legacy_session_key = derive_legacy_session_key(net->legacy_key_seed, hello.client_nonce, ack.server_nonce);
    net->legacy_tx_nonce = 0;
    net->legacy_rx_nonce = 0;
    net->security_mode = NET_SECURITY_LEGACY;
    net->security_ready = true;
    return true;
}

static NetworkSecurityMode detect_client_handshake_mode(Network* net, int timeout_ms) {
    if (!net || !net->connected || net->sockfd == INVALID_SOCKET) {
        return NET_SECURITY_NONE;
    }

    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) {
        return NET_SECURITY_NONE;
    }

    const int probe_timeout_ms = (timeout_ms >= 0 && timeout_ms < 1000) ? timeout_ms : 1000;
    uint8_t probe[8];
    int got = peek_with_min_bytes(net->sockfd, probe, sizeof(probe), 6, probe_timeout_ms);
    if (got < 6) {
        return NET_SECURITY_NONE;
    }

    const uint8_t openssl_magic_be[4] = {0x48, 0x58, 0x43, 0x54};
    if (memcmp(probe, openssl_magic_be, 4) == 0 &&
        probe[4] == HANDSHAKE_CLIENT_HELLO &&
        probe[5] == NETWORK_PROTOCOL_VERSION) {
        return NET_SECURITY_OPENSSL;
    }

    uint32_t native_magic = 0;
    memcpy(&native_magic, probe, sizeof(native_magic));
    if (native_magic == NETWORK_HANDSHAKE_MAGIC && probe[4] == HANDSHAKE_CLIENT_HELLO) {
        return NET_SECURITY_LEGACY;
    }

    return NET_SECURITY_NONE;
}
bool network_init(Network* net) {
    if (!net) return false;

    memset(net, 0, sizeof(Network));
    net->listen_sockfd = INVALID_SOCKET;
    net->sockfd = INVALID_SOCKET;
    net->connected = false;
    net->port = DEFAULT_PORT;
    reset_security_state(net);
    network_set_passphrase(net, NULL);

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif

    return true;
}

void network_set_passphrase(Network* net, const char* passphrase) {
    if (!net) {
        return;
    }

    const char* source = (passphrase && passphrase[0] != '\0') ? passphrase : NETWORK_DEFAULT_PASSPHRASE;
    copy_cstr_trunc(net->passphrase, sizeof(net->passphrase), source);
    net->legacy_key_seed = derive_legacy_key_seed(net->passphrase);
    reset_security_state(net);
}

bool network_host(Network* net, int port) {
    if (!net || port <= 0 || port > 65535) {
        return false;
    }

    close_socket_if_open(&net->listen_sockfd);
    close_socket_if_open(&net->sockfd);
    reset_security_state(net);

    net->listen_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (net->listen_sockfd == INVALID_SOCKET) {
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(net->listen_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(net->listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((uint16_t)port);

    if (bind(net->listen_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        close_socket_if_open(&net->listen_sockfd);
        return false;
    }

    if (listen(net->listen_sockfd, MAX_CLIENTS) == SOCKET_ERROR) {
        close_socket_if_open(&net->listen_sockfd);
        return false;
    }

    net->role = NET_HOST;
    net->connected = false;
    net->sockfd = INVALID_SOCKET;
    net->port = port;
    return true;
}

bool network_accept(Network* net, int timeout_ms) {
    if (!net || net->role != NET_HOST || net->connected || net->listen_sockfd == INVALID_SOCKET) {
        return false;
    }

    int ready = wait_socket_readable(net->listen_sockfd, timeout_ms);
    if (ready <= 0) {
        return false;
    }

    struct sockaddr_in client_addr;
#ifdef _WIN32
    int addr_len = sizeof(client_addr);
#else
    socklen_t addr_len = sizeof(client_addr);
#endif

    int client_sock = accept(net->listen_sockfd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) {
        return false;
    }

    net->sockfd = client_sock;
    net->connected = true;
    reset_security_state(net);
    /* Single-client session: no further accepts needed after connection. */
    close_socket_if_open(&net->listen_sockfd);

    const char* addr_ptr = inet_ntoa(client_addr.sin_addr);
    if (addr_ptr) {
        copy_cstr_trunc(net->host_ip, sizeof(net->host_ip), addr_ptr);
    } else {
        net->host_ip[0] = '\0';
    }

    return true;
}

bool network_connect(Network* net, const char* ip, int port) {
    if (!net || !ip || port <= 0 || port > 65535) {
        return false;
    }

    close_socket_if_open(&net->listen_sockfd);
    close_socket_if_open(&net->sockfd);
    reset_security_state(net);

    net->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (net->sockfd == INVALID_SOCKET) {
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) != 1) {
        close_socket_if_open(&net->sockfd);
        return false;
    }

    if (connect(net->sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        close_socket_if_open(&net->sockfd);
        return false;
    }

    copy_cstr_trunc(net->host_ip, sizeof(net->host_ip), ip);
    net->role = NET_CLIENT;
    net->connected = true;
    net->port = port;
    return true;
}

bool network_secure_handshake(Network* net, int timeout_ms) {
    if (!net || !net->connected || net->sockfd == INVALID_SOCKET || net->role == NET_NONE) {
        return false;
    }

    bool ok = false;

    if (net->role == NET_HOST) {
        NetworkSecurityMode mode = detect_client_handshake_mode(net, timeout_ms);
        if (mode == NET_SECURITY_OPENSSL) {
            ok = host_perform_openssl_handshake(net, timeout_ms);
        } else if (mode == NET_SECURITY_LEGACY) {
            ok = host_perform_legacy_handshake(net, timeout_ms);
        }
    } else {
        ok = client_perform_openssl_handshake(net, timeout_ms);
        if (!ok) {
            char ip_copy[sizeof(net->host_ip)];
            int port = net->port;
            copy_cstr_trunc(ip_copy, sizeof(ip_copy), net->host_ip);

            close_socket_if_open(&net->sockfd);
            net->connected = false;
            reset_security_state(net);

            if (network_connect(net, ip_copy, port)) {
                ok = client_perform_legacy_handshake(net, timeout_ms);
            }
        }
    }

    if (!ok) {
        close_socket_if_open(&net->sockfd);
        net->connected = false;
        reset_security_state(net);
    }

    return ok;
}

bool network_is_secure(const Network* net) {
    return net && net->connected && net->security_ready && net->security_mode != NET_SECURITY_NONE;
}

void network_close(Network* net) {
    if (!net) return;

    close_socket_if_open(&net->sockfd);
    close_socket_if_open(&net->listen_sockfd);

    net->connected = false;
    net->role = NET_NONE;
    net->host_ip[0] = '\0';
    reset_security_state(net);

#ifdef _WIN32
    WSACleanup();
#endif
}

bool network_send_move(Network* net, uint8_t row, uint8_t col) {
    if (!net) return false;

    NetworkPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PACKET_MOVE;
    pkt.row = row;
    pkt.col = col;
    return send_packet(net, &pkt);
}

bool network_receive_move(Network* net, uint8_t* row, uint8_t* col, int timeout_ms) {
    if (!net) return false;

    NetworkPacket pkt;
    if (receive_packet(net, &pkt, timeout_ms) && pkt.type == PACKET_MOVE) {
        if (row) *row = pkt.row;
        if (col) *col = pkt.col;
        return true;
    }

    return false;
}

bool network_sync_board(Network* net, Game* game) {
    if (!net || !game) return false;

    NetworkPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PACKET_SYNC;
    memcpy(pkt.board, game->board, sizeof(game->board));
    pkt.current_player = game->current_player;

    return send_packet(net, &pkt);
}

bool network_send_chat(Network* net, const char* msg) {
    if (!net || !msg) return false;

    NetworkPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PACKET_CHAT;
    copy_cstr_trunc(pkt.message, sizeof(pkt.message), msg);

    return send_packet(net, &pkt);
}

bool network_receive_chat(Network* net, char* msg, int timeout_ms) {
    if (!net || !msg) return false;

    NetworkPacket pkt;
    if (receive_packet(net, &pkt, timeout_ms) && pkt.type == PACKET_CHAT) {
        copy_cstr_trunc(msg, BUFFER_SIZE, pkt.message);
        return true;
    }

    return false;
}
