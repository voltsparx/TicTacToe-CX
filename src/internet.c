#include "internet.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <errno.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

static void write_error(char* err, size_t err_size, const char* msg) {
    if (!err || err_size == 0) {
        return;
    }
    if (!msg) {
        err[0] = '\0';
        return;
    }
    (void)snprintf(err, err_size, "%s", msg);
}

void internet_tunnel_init(InternetTunnel* tunnel) {
    if (!tunnel) {
        return;
    }
    memset(tunnel, 0, sizeof(*tunnel));
#ifndef _WIN32
    tunnel->pid = -1;
#endif
}

#ifdef _WIN32

bool internet_cloudflared_available(void) {
    return system("where cloudflared >NUL 2>&1") == 0;
}

bool internet_install_cloudflared(char* err, size_t err_size) {
    int rc = system("winget install --id Cloudflare.cloudflared -e --silent");
    if (rc == 0 && internet_cloudflared_available()) {
        return true;
    }
    write_error(err, err_size, "Automatic install failed. Install cloudflared manually or use WSL/Linux.");
    return false;
}

bool internet_extract_hostname(const char* input, char* hostname, size_t hostname_size) {
    const char* start = input;
    size_t len = 0;
    bool has_dot = false;

    if (!input || !hostname || hostname_size == 0) {
        return false;
    }

    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (strncmp(start, "https://", 8) == 0) {
        start += 8;
    } else if (strncmp(start, "http://", 7) == 0) {
        start += 7;
    }

    while (start[len] != '\0' &&
           start[len] != '/' &&
           !isspace((unsigned char)start[len]) &&
           start[len] != ':') {
        unsigned char ch = (unsigned char)start[len];
        if (ch == '.') {
            has_dot = true;
        } else if (ch != '-' && !isalnum(ch)) {
            return false;
        }
        len++;
    }
    if (len == 0 || len + 1 > hostname_size) {
        return false;
    }
    if (!has_dot || start[0] == '.' || start[0] == '-' || start[len - 1] == '.' || start[len - 1] == '-') {
        return false;
    }

    memcpy(hostname, start, len);
    hostname[len] = '\0';
    return true;
}

bool internet_tunnel_start_host(InternetTunnel* tunnel,
                                int game_port,
                                int timeout_seconds,
                                char* err,
                                size_t err_size) {
    (void)tunnel;
    (void)game_port;
    (void)timeout_seconds;
    write_error(err, err_size, "Internet tunnel automation is unavailable in native Windows build. Use WSL/Linux build.");
    return false;
}

bool internet_tunnel_start_client_proxy(InternetTunnel* tunnel,
                                        const char* hostname,
                                        int local_port,
                                        int timeout_seconds,
                                        char* err,
                                        size_t err_size) {
    (void)tunnel;
    (void)hostname;
    (void)local_port;
    (void)timeout_seconds;
    write_error(err, err_size, "Internet tunnel automation is unavailable in native Windows build. Use WSL/Linux build.");
    return false;
}

void internet_tunnel_stop(InternetTunnel* tunnel) {
    if (!tunnel) {
        return;
    }
    tunnel->active = false;
}

#else

static bool shell_cmd_ok(const char* cmd) {
    if (!cmd || cmd[0] == '\0') {
        return false;
    }
    return system(cmd) == 0;
}

bool internet_cloudflared_available(void) {
    return shell_cmd_ok("command -v cloudflared >/dev/null 2>&1");
}

static bool run_install_command(const char* cmd, char* err, size_t err_size) {
    if (!cmd || cmd[0] == '\0') {
        return false;
    }

    if (shell_cmd_ok(cmd) && internet_cloudflared_available()) {
        return true;
    }

    write_error(err, err_size, "Cloudflared install command failed.");
    return false;
}

bool internet_install_cloudflared(char* err, size_t err_size) {
#ifdef __APPLE__
    if (shell_cmd_ok("command -v brew >/dev/null 2>&1")) {
        return run_install_command("brew install cloudflared", err, err_size);
    }
    write_error(err, err_size, "Homebrew is required to auto-install cloudflared on macOS.");
    return false;
#elif defined(__ANDROID__)
    return run_install_command("pkg install -y cloudflared", err, err_size);
#else
    if (shell_cmd_ok("command -v apt-get >/dev/null 2>&1")) {
        return run_install_command(
            "if command -v sudo >/dev/null 2>&1; then sudo apt-get update && sudo apt-get install -y cloudflared; else apt-get update && apt-get install -y cloudflared; fi",
            err, err_size);
    }
    if (shell_cmd_ok("command -v dnf >/dev/null 2>&1")) {
        return run_install_command(
            "if command -v sudo >/dev/null 2>&1; then sudo dnf install -y cloudflared; else dnf install -y cloudflared; fi",
            err, err_size);
    }
    if (shell_cmd_ok("command -v pacman >/dev/null 2>&1")) {
        return run_install_command(
            "if command -v sudo >/dev/null 2>&1; then sudo pacman -Sy --noconfirm cloudflared; else pacman -Sy --noconfirm cloudflared; fi",
            err, err_size);
    }

    write_error(err, err_size, "Could not detect a supported package manager for cloudflared.");
    return false;
#endif
}

bool internet_extract_hostname(const char* input, char* hostname, size_t hostname_size) {
    const char* start = input;
    size_t len = 0;
    bool has_dot = false;

    if (!input || !hostname || hostname_size == 0) {
        return false;
    }

    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (strncmp(start, "https://", 8) == 0) {
        start += 8;
    } else if (strncmp(start, "http://", 7) == 0) {
        start += 7;
    }

    while (start[len] != '\0' &&
           start[len] != '/' &&
           !isspace((unsigned char)start[len]) &&
           start[len] != ':') {
        unsigned char ch = (unsigned char)start[len];
        if (ch == '.') {
            has_dot = true;
        } else if (ch != '-' && !isalnum(ch)) {
            return false;
        }
        len++;
    }
    if (len == 0 || len + 1 > hostname_size) {
        return false;
    }
    if (!has_dot || start[0] == '.' || start[0] == '-' || start[len - 1] == '.' || start[len - 1] == '-') {
        return false;
    }

    memcpy(hostname, start, len);
    hostname[len] = '\0';
    return true;
}

static bool try_read_public_url(const char* path, char* out_url, size_t out_url_size) {
    FILE* fp = NULL;
    char line[1024];

    if (!path || !out_url || out_url_size == 0) {
        return false;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return false;
    }

    while (fgets(line, sizeof(line), fp)) {
        const char* begin = strstr(line, "https://");
        if (!begin) {
            continue;
        }

        if (!strstr(begin, "trycloudflare.com")) {
            continue;
        }

        size_t i = 0;
        while (begin[i] != '\0' &&
               !isspace((unsigned char)begin[i]) &&
               i + 1 < out_url_size) {
            out_url[i] = begin[i];
            i++;
        }
        out_url[i] = '\0';

        fclose(fp);
        return i > 0;
    }

    fclose(fp);
    return false;
}

static bool process_is_alive(pid_t pid) {
    if (pid <= 0) {
        return false;
    }
    return kill(pid, 0) == 0 || errno == EPERM;
}

static bool spawn_cloudflared(InternetTunnel* tunnel,
                              const char* const argv[],
                              char* err,
                              size_t err_size) {
    int fd = -1;
    pid_t pid = -1;
    const char* tmpdir = getenv("TMPDIR");
    if (!tmpdir || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }

    if (!tunnel || !argv || !argv[0]) {
        write_error(err, err_size, "Invalid tunnel start parameters.");
        return false;
    }

    (void)snprintf(tunnel->log_path,
                   sizeof(tunnel->log_path),
                   "%s/tictactoe-cx-cloudflared-%ld-%d.log",
                   tmpdir,
                   (long)time(NULL),
                   (int)getpid());

    fd = open(tunnel->log_path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd < 0) {
        write_error(err, err_size, "Failed to create cloudflared log file.");
        return false;
    }

    pid = fork();
    if (pid < 0) {
        close(fd);
        write_error(err, err_size, "Failed to fork cloudflared process.");
        return false;
    }

    if (pid == 0) {
        (void)dup2(fd, STDOUT_FILENO);
        (void)dup2(fd, STDERR_FILENO);
        close(fd);
        execvp("cloudflared", (char* const*)argv);
        _exit(127);
    }

    close(fd);
    tunnel->pid = (int)pid;
    return true;
}

static bool wait_for_local_proxy(int local_port, int timeout_seconds, pid_t pid) {
    struct sockaddr_in addr;
    int sockfd = -1;
    const int total_loops = timeout_seconds * 10;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)local_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    for (int i = 0; i < total_loops; i++) {
        if (!process_is_alive(pid)) {
            return false;
        }

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd >= 0) {
            if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                close(sockfd);
                return true;
            }
            close(sockfd);
        }

        usleep(100000);
    }

    return false;
}

bool internet_tunnel_start_host(InternetTunnel* tunnel,
                                int game_port,
                                int timeout_seconds,
                                char* err,
                                size_t err_size) {
    char target[64];
    const char* argv[6];
    char url[INTERNET_URL_MAX];
    pid_t pid = -1;
    int loops = 0;

    if (!tunnel || game_port <= 0 || game_port > 65535) {
        write_error(err, err_size, "Invalid host tunnel port.");
        return false;
    }

    internet_tunnel_init(tunnel);
    (void)snprintf(target, sizeof(target), "tcp://127.0.0.1:%d", game_port);

    argv[0] = "cloudflared";
    argv[1] = "tunnel";
    argv[2] = "--url";
    argv[3] = target;
    argv[4] = "--no-autoupdate";
    argv[5] = NULL;

    if (!spawn_cloudflared(tunnel, argv, err, err_size)) {
        return false;
    }

    pid = (pid_t)tunnel->pid;
    if (timeout_seconds <= 0) {
        timeout_seconds = 20;
    }

    loops = timeout_seconds * 5;
    for (int i = 0; i < loops; i++) {
        if (!process_is_alive(pid)) {
            internet_tunnel_stop(tunnel);
            write_error(err, err_size, "cloudflared exited while creating host tunnel.");
            return false;
        }

        memset(url, 0, sizeof(url));
        if (try_read_public_url(tunnel->log_path, url, sizeof(url))) {
            (void)snprintf(tunnel->public_url, sizeof(tunnel->public_url), "%s", url);
            if (!internet_extract_hostname(url, tunnel->hostname, sizeof(tunnel->hostname))) {
                internet_tunnel_stop(tunnel);
                write_error(err, err_size, "Could not parse cloudflared hostname.");
                return false;
            }
            tunnel->active = true;
            tunnel->local_port = game_port;
            return true;
        }

        usleep(200000);
    }

    internet_tunnel_stop(tunnel);
    write_error(err, err_size, "Timed out waiting for cloudflared public link.");
    return false;
}

bool internet_tunnel_start_client_proxy(InternetTunnel* tunnel,
                                        const char* hostname,
                                        int local_port,
                                        int timeout_seconds,
                                        char* err,
                                        size_t err_size) {
    char target[64];
    const char* argv[9];
    pid_t pid = -1;

    if (!tunnel || !hostname || hostname[0] == '\0' || local_port <= 0 || local_port > 65535) {
        write_error(err, err_size, "Invalid client tunnel parameters.");
        return false;
    }

    internet_tunnel_init(tunnel);
    (void)snprintf(target, sizeof(target), "127.0.0.1:%d", local_port);

    argv[0] = "cloudflared";
    argv[1] = "access";
    argv[2] = "tcp";
    argv[3] = "--hostname";
    argv[4] = hostname;
    argv[5] = "--url";
    argv[6] = target;
    argv[7] = "--no-autoupdate";
    argv[8] = NULL;

    if (!spawn_cloudflared(tunnel, argv, err, err_size)) {
        return false;
    }

    pid = (pid_t)tunnel->pid;
    if (timeout_seconds <= 0) {
        timeout_seconds = 12;
    }

    if (!wait_for_local_proxy(local_port, timeout_seconds, pid)) {
        internet_tunnel_stop(tunnel);
        write_error(err, err_size, "Timed out waiting for local cloudflared proxy.");
        return false;
    }

    tunnel->active = true;
    tunnel->local_port = local_port;
    (void)snprintf(tunnel->hostname, sizeof(tunnel->hostname), "%s", hostname);
    return true;
}

void internet_tunnel_stop(InternetTunnel* tunnel) {
    pid_t pid = -1;
    int status = 0;

    if (!tunnel) {
        return;
    }

    pid = (pid_t)tunnel->pid;
    if (pid > 0) {
        (void)kill(pid, SIGTERM);
        for (int i = 0; i < 20; i++) {
            pid_t done = waitpid(pid, &status, WNOHANG);
            if (done == pid) {
                break;
            }
            usleep(100000);
        }

        if (process_is_alive(pid)) {
            (void)kill(pid, SIGKILL);
        }
        (void)waitpid(pid, &status, WNOHANG);
    }

    if (tunnel->log_path[0] != '\0') {
        (void)unlink(tunnel->log_path);
    }

    internet_tunnel_init(tunnel);
}

#endif
