#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_CODE errno
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

bool network_init(Network* net) {
    if (!net) return false;
    
    memset(net, 0, sizeof(Network));
    net->sockfd = INVALID_SOCKET;
    net->connected = false;
    net->port = DEFAULT_PORT;
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif
    
    return true;
}

static int set_nonblocking(int sockfd) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool network_host(Network* net, int port) {
    if (!net) return false;
    
    net->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (net->sockfd == INVALID_SOCKET) {
        return false;
    }
    
    int opt = 1;
#ifdef _WIN32
    setsockopt(net->sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(net->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((uint16_t)port);
    
    if (bind(net->sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        CLOSE_SOCKET(net->sockfd);
        return false;
    }
    
    if (listen(net->sockfd, MAX_CLIENTS) == SOCKET_ERROR) {
        CLOSE_SOCKET(net->sockfd);
        return false;
    }
    
    set_nonblocking(net->sockfd);
    
    net->role = NET_HOST;
    net->connected = false;
    net->port = port;
    
    return true;
}

bool network_connect(Network* net, const char* ip, int port) {
    if (!net || !ip) return false;
    
    net->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (net->sockfd == INVALID_SOCKET) {
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);
    
#ifdef _WIN32
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
#else
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
#endif
    
    if (connect(net->sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        CLOSE_SOCKET(net->sockfd);
        return false;
    }
    
    set_nonblocking(net->sockfd);
    
    strncpy(net->host_ip, ip, 15);
    net->host_ip[15] = '\0';
    net->role = NET_CLIENT;
    net->connected = true;
    net->port = port;
    
    return true;
}

void network_close(Network* net) {
    if (!net) return;
    
    if (net->sockfd != INVALID_SOCKET) {
        CLOSE_SOCKET(net->sockfd);
        net->sockfd = INVALID_SOCKET;
    }
    
    net->connected = false;
    net->role = NET_NONE;
    
#ifdef _WIN32
    WSACleanup();
#endif
}

bool network_send_move(Network* net, uint8_t row, uint8_t col) {
    if (!net || net->sockfd == INVALID_SOCKET) return false;
    
    NetworkPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PACKET_MOVE;
    pkt.row = row;
    pkt.col = col;
    
    int sent = send(net->sockfd, (const char*)&pkt, sizeof(pkt), 0);
    return (sent == sizeof(pkt));
}

bool network_receive_move(Network* net, uint8_t* row, uint8_t* col, int timeout_ms) {
    if (!net || net->sockfd == INVALID_SOCKET) return false;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET((unsigned int)net->sockfd, &readfds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int ret = select((int)(net->sockfd + 1), &readfds, NULL, NULL, &tv);
    if (ret <= 0) return false;
    
    NetworkPacket pkt;
    int received = recv(net->sockfd, (char*)&pkt, sizeof(pkt), 0);
    
    if (received > 0 && pkt.type == PACKET_MOVE) {
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
    
    int sent = send(net->sockfd, (const char*)&pkt, sizeof(pkt), 0);
    return (sent == sizeof(pkt));
}

bool network_send_chat(Network* net, const char* msg) {
    if (!net || !msg) return false;
    
    NetworkPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PACKET_CHAT;
    strncpy(pkt.message, msg, BUFFER_SIZE - 1);
    
    int sent = send(net->sockfd, (const char*)&pkt, sizeof(pkt), 0);
    return (sent == sizeof(pkt));
}

bool network_receive_chat(Network* net, char* msg, int timeout_ms) {
    if (!net || !msg) return false;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET((unsigned int)net->sockfd, &readfds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int ret = select((int)(net->sockfd + 1), &readfds, NULL, NULL, &tv);
    if (ret <= 0) return false;
    
    NetworkPacket pkt;
    int received = recv(net->sockfd, (char*)&pkt, sizeof(pkt), 0);
    
    if (received > 0 && pkt.type == PACKET_CHAT) {
        strncpy(msg, pkt.message, BUFFER_SIZE - 1);
        return true;
    }
    
    return false;
}
