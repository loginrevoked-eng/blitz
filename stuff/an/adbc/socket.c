#include "adbc.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET adbc_socket_t;
#define ADBC_CLOSE_SOCKET closesocket
#define ADBC_SOCKET_ERROR SOCKET_ERROR
#else
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int adbc_socket_t;
#define ADBC_CLOSE_SOCKET close
#define ADBC_SOCKET_ERROR (-1)
#endif

static void set_timeout(adbc_socket_t sock, unsigned timeout_ms) {
#ifdef _WIN32
    DWORD timeout = (DWORD)timeout_ms;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
#else
    struct timeval timeout;
    timeout.tv_sec = (time_t)(timeout_ms / 1000u);
    timeout.tv_usec = (suseconds_t)((timeout_ms % 1000u) * 1000u);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

static int send_all(adbc_socket_t sock, const uint8_t *data, size_t size) {
    size_t sent = 0;
    while (sent < size) {
#ifdef _WIN32
        int count = send(sock, (const char *)data + sent,
                         (int)(size - sent), 0);
#else
        ssize_t count = send(sock, data + sent, size - sent, 0);
#endif
        if (count <= 0) {
            return -1;
        }
        sent += (size_t)count;
    }
    return 0;
}

int adbc_socket_probe(const char *host, const char *port, unsigned timeout_ms,
                      AdbcClient *client) {
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    adbc_socket_t sock = (adbc_socket_t)-1;
    uint8_t outbound[64];
    uint8_t inbound[4096];
    size_t outbound_size;
    int connected = 0;
#ifdef _WIN32
    WSADATA winsock_data;
    int winsock_started = WSAStartup(MAKEWORD(2, 2), &winsock_data) == 0;
    if (!winsock_started) {
        adbc_log_error("Winsock initialization failed");
        return -1;
    }
#endif

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &addresses) != 0) {
        adbc_log_error("could not resolve %s:%s", host, port);
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }

    for (address = addresses; address != NULL; address = address->ai_next) {
        sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (sock == (adbc_socket_t)-1) {
            continue;
        }
        set_timeout(sock, timeout_ms);
        if (connect(sock, address->ai_addr, (int)address->ai_addrlen) != ADBC_SOCKET_ERROR) {
            connected = 1;
            break;
        }
        ADBC_CLOSE_SOCKET(sock);
        sock = (adbc_socket_t)-1;
    }
    freeaddrinfo(addresses);

    if (!connected) {
        adbc_log_error("could not connect to %s:%s", host, port);
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }

    adbc_client_init(client);
    outbound_size = adbc_client_start(client, outbound, sizeof(outbound));
    if (outbound_size == 0 || send_all(sock, outbound, outbound_size) != 0) {
        adbc_log_error("failed to send ADB CNXN packet");
        ADBC_CLOSE_SOCKET(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }

    while (client->state == ADBC_WAITING) {
#ifdef _WIN32
        int count = recv(sock, (char *)inbound, (int)sizeof(inbound), 0);
#else
        ssize_t count = recv(sock, inbound, sizeof(inbound), 0);
#endif
        if (count <= 0) {
            adbc_log_error("timed out or disconnected waiting for ADB response");
            ADBC_CLOSE_SOCKET(sock);
#ifdef _WIN32
            WSACleanup();
#endif
            return -1;
        }
        (void)adbc_client_feed(client, inbound, (size_t)count);
    }

    ADBC_CLOSE_SOCKET(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return client->state == ADBC_PROTOCOL_ERROR ? -1 : 0;
}
