#ifndef INTERNET_H
#define INTERNET_H

#include <stdbool.h>
#include <stddef.h>

#define INTERNET_HOSTNAME_MAX 256
#define INTERNET_URL_MAX 384

typedef struct {
    bool active;
    int local_port;
    char hostname[INTERNET_HOSTNAME_MAX];
    char public_url[INTERNET_URL_MAX];
#ifdef _WIN32
    void* process_handle;
#else
    int pid;
    char log_path[512];
#endif
} InternetTunnel;

void internet_tunnel_init(InternetTunnel* tunnel);
bool internet_cloudflared_available(void);
bool internet_install_cloudflared(char* err, size_t err_size);
bool internet_extract_hostname(const char* input, char* hostname, size_t hostname_size);
bool internet_tunnel_start_host(InternetTunnel* tunnel,
                                int game_port,
                                int timeout_seconds,
                                char* err,
                                size_t err_size);
bool internet_tunnel_start_client_proxy(InternetTunnel* tunnel,
                                        const char* hostname,
                                        int local_port,
                                        int timeout_seconds,
                                        char* err,
                                        size_t err_size);
void internet_tunnel_stop(InternetTunnel* tunnel);

#endif
