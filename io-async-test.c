#include <winsock2.h>
#include <ws2tcpip.h>

#include "io-async.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ParseUnsigned(const char *text, unsigned long minimum,
    unsigned long maximum, unsigned long *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);
    if (text[0] == '\0' || end == text || *end != '\0' ||
        parsed < minimum || parsed > maximum)
        return 0;
    *value = parsed;
    return 1;
}

static IoAsyncTcpState StateForError(int error)
{
    if (error == 0)
        return IO_ASYNC_TCP_OPEN;
    if (error == WSAECONNREFUSED)
        return IO_ASYNC_TCP_CLOSED;
    if (error == WSAETIMEDOUT)
        return IO_ASYNC_TCP_TIMEOUT;
    return IO_ASYNC_TCP_ERROR;
}

static void ProbeSequential(uint32_t address, uint16_t port,
    DWORD timeout_ms, IoAsyncTcpResult *result)
{
    result->port = port;
    result->state = IO_ASYNC_TCP_ERROR;
    result->error = 0;

    SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET) {
        result->error = WSAGetLastError();
        return;
    }

    u_long nonblocking = 1;
    if (ioctlsocket(socket_handle, FIONBIO, &nonblocking) == SOCKET_ERROR) {
        result->error = WSAGetLastError();
        closesocket(socket_handle);
        return;
    }

    struct sockaddr_in target = { 0 };
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    target.sin_addr.s_addr = htonl(address);

    if (connect(socket_handle, (struct sockaddr *)&target,
            sizeof(target)) == 0) {
        result->state = IO_ASYNC_TCP_OPEN;
        closesocket(socket_handle);
        return;
    }

    int error = WSAGetLastError();
    if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS &&
        error != WSAEALREADY) {
        result->state = StateForError(error);
        result->error = error;
        closesocket(socket_handle);
        return;
    }

    fd_set writable;
    fd_set exceptional;
    FD_ZERO(&writable);
    FD_ZERO(&exceptional);
    FD_SET(socket_handle, &writable);
    FD_SET(socket_handle, &exceptional);
    TIMEVAL timeout = {
        (long)(timeout_ms / 1000),
        (long)((timeout_ms % 1000) * 1000)
    };
    int ready = select(0, NULL, &writable, &exceptional, &timeout);
    if (ready == 0) {
        result->state = IO_ASYNC_TCP_TIMEOUT;
        result->error = WSAETIMEDOUT;
    } else if (ready == SOCKET_ERROR) {
        result->state = IO_ASYNC_TCP_ERROR;
        result->error = WSAGetLastError();
    } else {
        int socket_error = 0;
        int error_length = sizeof(socket_error);
        if (getsockopt(socket_handle, SOL_SOCKET, SO_ERROR,
                (char *)&socket_error, &error_length) == SOCKET_ERROR)
            socket_error = WSAGetLastError();
        result->state = StateForError(socket_error);
        result->error = socket_error;
    }
    closesocket(socket_handle);
}

static const char *StateName(IoAsyncTcpState state)
{
    switch (state) {
    case IO_ASYNC_TCP_OPEN: return "open";
    case IO_ASYNC_TCP_CLOSED: return "closed";
    case IO_ASYNC_TCP_TIMEOUT: return "timeout";
    case IO_ASYNC_TCP_ERROR: return "error";
    default: return "unknown";
    }
}

int main(int argc, char **argv)
{
    int async_mode = 0;
    int show_summary = 0;
    char **arguments = calloc((size_t)argc, sizeof(*arguments));
    if (arguments == NULL) {
        fprintf(stderr, "out of memory\n");
        return EXIT_FAILURE;
    }

    int argument_count = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--async") == 0)
            async_mode = 1;
        else if (strcmp(argv[i], "--summary") == 0)
            show_summary = 1;
        else
            arguments[argument_count++] = argv[i];
    }

    if (argument_count < 3) {
        fprintf(stderr,
            "usage: %s [--async] [--summary] <ipv4> <timeout-ms> <port> [port ...]\n",
            argv[0]);
        free(arguments);
        return EXIT_FAILURE;
    }

    unsigned long timeout_ms;
    if (!ParseUnsigned(arguments[1], 1, 60000, &timeout_ms)) {
        fprintf(stderr, "timeout must be between 1 and 60000 ms\n");
        free(arguments);
        return EXIT_FAILURE;
    }

    size_t port_count = (size_t)(argument_count - 2);
    uint16_t *ports = calloc(port_count, sizeof(*ports));
    IoAsyncTcpResult *results = calloc(port_count, sizeof(*results));
    if (ports == NULL || results == NULL) {
        fprintf(stderr, "out of memory\n");
        free(arguments);
        free(ports);
        free(results);
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < port_count; ++i) {
        unsigned long port;
        if (!ParseUnsigned(arguments[i + 2], 1, UINT16_MAX, &port)) {
            fprintf(stderr, "invalid port: %s\n", arguments[i + 2]);
            free(arguments);
            free(ports);
            free(results);
            return EXIT_FAILURE;
        }
        ports[i] = (uint16_t)port;
    }

    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        free(arguments);
        free(ports);
        free(results);
        return EXIT_FAILURE;
    }

    struct in_addr address;
    if (InetPtonA(AF_INET, arguments[0], &address) != 1) {
        fprintf(stderr, "invalid IPv4 address: %s\n", arguments[0]);
        WSACleanup();
        free(arguments);
        free(ports);
        free(results);
        return EXIT_FAILURE;
    }

    uint64_t started = GetTickCount64();
    int batch_error = 0;
    int ok = 1;
    if (async_mode) {
        ok = IoIocpTcpProbeBatch(ntohl(address.s_addr), ports, port_count,
            (DWORD)timeout_ms, results, &batch_error);
    } else {
        for (size_t i = 0; i < port_count; ++i)
            ProbeSequential(ntohl(address.s_addr), ports[i],
                (DWORD)timeout_ms, &results[i]);
    }
    uint64_t elapsed_ms = GetTickCount64() - started;

    if (!ok) {
        fprintf(stderr, "async TCP batch failed (error %d)\n", batch_error);
        WSACleanup();
        free(arguments);
        free(ports);
        free(results);
        return EXIT_FAILURE;
    }

    size_t open_count = 0;
    size_t closed_count = 0;
    size_t timeout_count = 0;
    size_t error_count = 0;
    for (size_t i = 0; i < port_count; ++i) {
        printf("%s:%u %s", arguments[0], (unsigned)results[i].port,
            StateName(results[i].state));
        if (results[i].error != 0)
            printf(" (error %d)", results[i].error);
        putchar('\n');

        switch (results[i].state) {
        case IO_ASYNC_TCP_OPEN: ++open_count; break;
        case IO_ASYNC_TCP_CLOSED: ++closed_count; break;
        case IO_ASYNC_TCP_TIMEOUT: ++timeout_count; break;
        case IO_ASYNC_TCP_ERROR: ++error_count; break;
        }
    }

    if (show_summary) {
        printf("mode=%s elapsed=%llu ms connections=%zu open=%zu closed=%zu timeout=%zu errors=%zu\n",
            async_mode ? "IOCP" : "sequential",
            (unsigned long long)elapsed_ms, port_count, open_count,
            closed_count, timeout_count, error_count);
    }

    WSACleanup();
    free(arguments);
    free(ports);
    free(results);
    return EXIT_SUCCESS;
}
