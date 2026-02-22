#ifndef NETWORK_H
#define NETWORK_H

#include "game.h"
#include <stdbool.h>

#define DEFAULT_PORT 45678
#define MAX_CLIENTS 1
#define BUFFER_SIZE 256

typedef enum {
    NET_NONE,
    NET_HOST,
    NET_CLIENT
} NetworkRole;

typedef struct {
    int sockfd;
    NetworkRole role;
    bool connected;
    char host_ip[16];
    int port;
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
bool network_host(Network* net, int port);
bool network_connect(Network* net, const char* ip, int port);
void network_close(Network* net);
bool network_send_move(Network* net, uint8_t row, uint8_t col);
bool network_receive_move(Network* net, uint8_t* row, uint8_t* col, int timeout_ms);
bool network_sync_board(Network* net, Game* game);
bool network_send_chat(Network* net, const char* msg);
bool network_receive_chat(Network* net, char* msg, int timeout_ms);

#endif
