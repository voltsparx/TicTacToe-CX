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

#define HANDSHAKE_CLIENT_HELLO 1u
#define HANDSHAKE_SERVER_HELLO 2u

#define HANDSHAKE_CLIENT_SIZE (4u + 1u + 1u + 2u + 16u + 12u + 4u + 32u)
#define HANDSHAKE_SERVER_SIZE (4u + 1u + 1u + 2u + 16u + 12u + 12u + 4u + 32u)
#define GCM_TAG_SIZE 16u
#define PACKET_BYTES ((size_t)sizeof(NetworkPacket))
#define FRAME_SIZE (4u + 8u + PACKET_BYTES + GCM_TAG_SIZE)

static void close_socket_if_open(int* sockfd) {
    if (!sockfd || *sockfd == INVALID_SOCKET) {
        return;
    }
    CLOSE_SOCKET(*sockfd);
    *sockfd = INVALID_SOCKET;
}

static void reset_security_state(Network* net) {
    if (!net) {
        return;
    }
    net->security_ready = false;
    memset(net->send_key, 0, sizeof(net->send_key));
    memset(net->recv_key, 0, sizeof(net->recv_key));
    memset(net->send_iv_prefix, 0, sizeof(net->send_iv_prefix));
    memset(net->recv_iv_prefix, 0, sizeof(net->recv_iv_prefix));
    net->tx_seq = 0;
    net->rx_seq = 0;
}

static void write_u32_be(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)(value);
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

static bool send_all(int sockfd, const char* data, size_t len) {
    size_t sent_total = 0;
    while (sent_total < len) {
        int sent = send(sockfd, data + sent_total, (int)(len - sent_total), 0);
        if (sent <= 0) {
            return false;
        }
        sent_total += (size_t)sent;
    }
    return true;
}

static bool recv_all(int sockfd, char* data, size_t len) {
    size_t recv_total = 0;
    while (recv_total < len) {
        int received = recv(sockfd, data + recv_total, (int)(len - recv_total), 0);
        if (received <= 0) {
            return false;
        }
        recv_total += (size_t)received;
    }
    return true;
}

static bool random_bytes(uint8_t* out, size_t len) {
    if (!out || len > INT_MAX) {
        return false;
    }
    return RAND_bytes(out, (int)len) == 1;
}

static bool hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[32]) {
    if (!key || !msg || !out || key_len > INT_MAX || msg_len > UINT_MAX) {
        return false;
    }

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
    if (!a || !b) {
        return false;
    }
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
    if (!ctx) {
        return false;
    }

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
    if (!ctx) {
        return false;
    }

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

static bool send_secure_packet(Network* net, const NetworkPacket* pkt) {
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

    if (!aes_gcm_encrypt(net->send_key,
                         iv,
                         (const uint8_t*)pkt,
                         (int)PACKET_BYTES,
                         cipher_out,
                         tag_out)) {
        return false;
    }

    write_u32_be(frame, NETWORK_FRAME_MAGIC);
    write_u64_be(frame + 4, seq);

    if (!send_all(net->sockfd, (const char*)frame, sizeof(frame))) {
        net->connected = false;
        close_socket_if_open(&net->sockfd);
        reset_security_state(net);
        return false;
    }

    return true;
}

static bool receive_secure_packet(Network* net, NetworkPacket* pkt, int timeout_ms) {
    if (!net || !pkt || !net->connected || net->sockfd == INVALID_SOCKET || !net->security_ready) {
        return false;
    }

    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) {
        return false;
    }

    uint8_t frame[FRAME_SIZE];
    if (!recv_all(net->sockfd, (char*)frame, sizeof(frame))) {
        net->connected = false;
        close_socket_if_open(&net->sockfd);
        reset_security_state(net);
        return false;
    }

    const uint32_t magic = read_u32_be(frame);
    const uint64_t seq = read_u64_be(frame + 4);
    const uint8_t* cipher_in = frame + 12;
    const uint8_t* tag_in = cipher_in + PACKET_BYTES;

    if (magic != NETWORK_FRAME_MAGIC || seq == 0 || seq <= net->rx_seq) {
        net->connected = false;
        close_socket_if_open(&net->sockfd);
        reset_security_state(net);
        return false;
    }

    uint8_t iv[12];
    build_iv(net->recv_iv_prefix, seq, iv);

    if (!aes_gcm_decrypt(net->recv_key,
                         iv,
                         cipher_in,
                         (int)PACKET_BYTES,
                         tag_in,
                         (uint8_t*)pkt)) {
        net->connected = false;
        close_socket_if_open(&net->sockfd);
        reset_security_state(net);
        return false;
    }

    net->rx_seq = seq;
    return true;
}

static bool send_packet(Network* net, const NetworkPacket* pkt) {
    if (!network_is_secure(net)) {
        return false;
    }
    return send_secure_packet(net, pkt);
}

static bool receive_packet(Network* net, NetworkPacket* pkt, int timeout_ms) {
    if (!network_is_secure(net)) {
        return false;
    }
    return receive_secure_packet(net, pkt, timeout_ms);
}

static bool host_perform_handshake(Network* net, int timeout_ms) {
    uint8_t client_msg[HANDSHAKE_CLIENT_SIZE];
    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) {
        return false;
    }

    if (!recv_all(net->sockfd, (char*)client_msg, sizeof(client_msg))) {
        return false;
    }

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
    if (!derive_base_key(net->passphrase, salt, base_key)) {
        return false;
    }

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

    if (!hmac_sha256(base_key, sizeof(base_key), server_msg, 52, server_msg + 52)) {
        return false;
    }

    if (!send_all(net->sockfd, (const char*)server_msg, sizeof(server_msg))) {
        return false;
    }

    uint8_t key_c2s[32];
    uint8_t key_s2c[32];
    if (!derive_session_keys(base_key, client_nonce, server_nonce, key_c2s, key_s2c)) {
        return false;
    }

    memcpy(net->send_key, key_s2c, sizeof(net->send_key));
    memcpy(net->recv_key, key_c2s, sizeof(net->recv_key));
    memcpy(net->send_iv_prefix, server_prefix, sizeof(net->send_iv_prefix));
    memcpy(net->recv_iv_prefix, client_prefix, sizeof(net->recv_iv_prefix));

    net->tx_seq = 0;
    net->rx_seq = 0;
    net->security_ready = true;
    return true;
}

static bool client_perform_handshake(Network* net, int timeout_ms) {
    uint8_t salt[16];
    uint8_t client_nonce[12];
    uint8_t client_prefix[4];
    if (!random_bytes(salt, sizeof(salt)) ||
        !random_bytes(client_nonce, sizeof(client_nonce)) ||
        !random_bytes(client_prefix, sizeof(client_prefix))) {
        return false;
    }

    uint8_t base_key[32];
    if (!derive_base_key(net->passphrase, salt, base_key)) {
        return false;
    }

    uint8_t client_msg[HANDSHAKE_CLIENT_SIZE];
    memset(client_msg, 0, sizeof(client_msg));
    write_u32_be(client_msg, NETWORK_HANDSHAKE_MAGIC);
    client_msg[4] = HANDSHAKE_CLIENT_HELLO;
    client_msg[5] = NETWORK_PROTOCOL_VERSION;
    memcpy(client_msg + 8, salt, 16);
    memcpy(client_msg + 24, client_nonce, 12);
    memcpy(client_msg + 36, client_prefix, 4);

    if (!hmac_sha256(base_key, sizeof(base_key), client_msg, 40, client_msg + 40)) {
        return false;
    }

    if (!send_all(net->sockfd, (const char*)client_msg, sizeof(client_msg))) {
        return false;
    }

    int ready = wait_socket_readable(net->sockfd, timeout_ms);
    if (ready <= 0) {
        return false;
    }

    uint8_t server_msg[HANDSHAKE_SERVER_SIZE];
    if (!recv_all(net->sockfd, (char*)server_msg, sizeof(server_msg))) {
        return false;
    }

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
    if (!derive_session_keys(base_key, client_nonce, server_nonce, key_c2s, key_s2c)) {
        return false;
    }

    memcpy(net->send_key, key_c2s, sizeof(net->send_key));
    memcpy(net->recv_key, key_s2c, sizeof(net->recv_key));
    memcpy(net->send_iv_prefix, client_prefix, sizeof(net->send_iv_prefix));
    memcpy(net->recv_iv_prefix, server_prefix, sizeof(net->recv_iv_prefix));

    net->tx_seq = 0;
    net->rx_seq = 0;
    net->security_ready = true;
    return true;
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
    strncpy(net->passphrase, source, NETWORK_PASSPHRASE_MAX - 1);
    net->passphrase[NETWORK_PASSPHRASE_MAX - 1] = '\0';
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

    const char* addr_ptr = inet_ntoa(client_addr.sin_addr);
    if (addr_ptr) {
        strncpy(net->host_ip, addr_ptr, sizeof(net->host_ip) - 1);
        net->host_ip[sizeof(net->host_ip) - 1] = '\0';
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

    strncpy(net->host_ip, ip, sizeof(net->host_ip) - 1);
    net->host_ip[sizeof(net->host_ip) - 1] = '\0';
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
        ok = host_perform_handshake(net, timeout_ms);
    } else if (net->role == NET_CLIENT) {
        ok = client_perform_handshake(net, timeout_ms);
    }

    if (!ok) {
        net->connected = false;
        close_socket_if_open(&net->sockfd);
        reset_security_state(net);
    }

    return ok;
}

bool network_is_secure(const Network* net) {
    return net && net->connected && net->security_ready;
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
    strncpy(pkt.message, msg, BUFFER_SIZE - 1);
    return send_packet(net, &pkt);
}

bool network_receive_chat(Network* net, char* msg, int timeout_ms) {
    if (!net || !msg) return false;

    NetworkPacket pkt;
    if (receive_packet(net, &pkt, timeout_ms) && pkt.type == PACKET_CHAT) {
        strncpy(msg, pkt.message, BUFFER_SIZE - 1);
        msg[BUFFER_SIZE - 1] = '\0';
        return true;
    }
    return false;
}
