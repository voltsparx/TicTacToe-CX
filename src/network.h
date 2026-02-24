#ifndef NETWORK_H
#define NETWORK_H

#include "game.h"
#include <stdbool.h>
#include <stdint.h>

#define DEFAULT_PORT 45678
#define MAX_CLIENTS 1
#define BUFFER_SIZE 256
#define NETWORK_PASSPHRASE_MAX 64

typedef enum {
    NET_NONE,
    NET_HOST,
    NET_CLIENT
} NetworkRole;

typedef enum {
    NET_SECURITY_NONE,
    NET_SECURITY_OPENSSL,
    NET_SECURITY_LEGACY
} NetworkSecurityMode;

typedef struct {
    int listen_sockfd;
    int sockfd;
    NetworkRole role;
    bool connected;
    bool security_ready;
    NetworkSecurityMode security_mode;
    char host_ip[16];
    int port;
    char passphrase[NETWORK_PASSPHRASE_MAX];
    uint8_t send_key[32];
    uint8_t recv_key[32];
    uint8_t send_iv_prefix[4];
    uint8_t recv_iv_prefix[4];
    uint64_t tx_seq;
    uint64_t rx_seq;
    uint64_t legacy_key_seed;
    uint64_t legacy_session_key;
    uint32_t legacy_tx_nonce;
    uint32_t legacy_rx_nonce;
} Network;

typedef struct {
    uint8_t type;
    uint8_t row;
    uint8_t col;
    uint8_t board[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    Player current_player;
    char message[BUFFER_SIZE];
} NetworkPacket;

#define PACKET_MOVE 1
#define PACKET_SYNC 2
#define PACKET_CHAT 3
#define PACKET_RESET 4
#define PACKET_QUIT 5

bool network_init(Network* net);
void network_set_passphrase(Network* net, const char* passphrase);
bool network_host(Network* net, int port);
bool network_accept(Network* net, int timeout_ms);
bool network_connect(Network* net, const char* ip, int port);
bool network_secure_handshake(Network* net, int timeout_ms);
bool network_is_secure(const Network* net);
void network_close(Network* net);
bool network_send_move(Network* net, uint8_t row, uint8_t col);
bool network_receive_move(Network* net, uint8_t* row, uint8_t* col, int timeout_ms);
bool network_sync_board(Network* net, Game* game);
bool network_send_chat(Network* net, const char* msg);
bool network_receive_chat(Network* net, char* msg, int timeout_ms);

#endif
