#include <winsock2.h>

#include "lookup.h"
#include "rtsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RTSP_RESPONSE_CAPACITY 2048

static int WaitSocket(SOCKET socket_handle, int writable, DWORD timeout_ms)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(socket_handle, &set);

    TIMEVAL timeout = {
        (long)(timeout_ms / 1000),
        (long)((timeout_ms % 1000) * 1000)
    };
    return writable
        ? select(0, NULL, &set, NULL, &timeout) > 0
        : select(0, &set, NULL, NULL, &timeout) > 0;
}

static SOCKET ConnectRtsp(HostUpCheck *check, uint16_t port)
{
    SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET)
        return INVALID_SOCKET;

    u_long nonblocking = 1;
    ioctlsocket(socket_handle, FIONBIO, &nonblocking);

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl((uint32_t)check->toptions->probe.addr);

    int connected = connect(socket_handle, (struct sockaddr *)&address,
        sizeof(address));
    if (connected == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) {
            closesocket(socket_handle);
            return INVALID_SOCKET;
        }
        if (!WaitSocket(socket_handle, 1,
                check->toptions->probe.timeout_ms)) {
            closesocket(socket_handle);
            return INVALID_SOCKET;
        }

        int socket_error = 0;
        int error_size = sizeof(socket_error);
        if (getsockopt(socket_handle, SOL_SOCKET, SO_ERROR,
                (char *)&socket_error, &error_size) == SOCKET_ERROR ||
            socket_error != 0) {
            closesocket(socket_handle);
            return INVALID_SOCKET;
        }
    }
    return socket_handle;
}

static int SendRequest(SOCKET socket_handle, const char *request,
    size_t request_length, DWORD timeout_ms)
{
    size_t sent = 0;
    while (sent < request_length) {
        if (!WaitSocket(socket_handle, 1, timeout_ms))
            return 0;
        int count = send(socket_handle, request + sent,
            (int)(request_length - sent), 0);
        if (count == SOCKET_ERROR || count == 0)
            return 0;
        sent += (size_t)count;
    }
    return 1;
}

char *RtspGrab(HostUpCheck *check, uint16_t port)
{
    if (check == NULL || check->toptions == NULL ||
        IsOOB(MAX_LOOKUP_INDEX, port))
        return NULL;

    uint32_t services = SERVICES[port];
    if (!(services & SERVICE_RTSP))
        return NULL;

    SOCKET socket_handle = ConnectRtsp(check, port);
    if (socket_handle == INVALID_SOCKET)
        return NULL;

    char request[128];
    int request_length = snprintf(request, sizeof(request),
        "OPTIONS * RTSP/1.0\r\nCSeq: 1\r\nUser-Agent: probe/1.0\r\n\r\n");
    if (request_length <= 0 || (size_t)request_length >= sizeof(request) ||
        !SendRequest(socket_handle, request, (size_t)request_length,
            check->toptions->probe.timeout_ms)) {
        closesocket(socket_handle);
        return NULL;
    }

    char *response = calloc(RTSP_RESPONSE_CAPACITY + 1, 1);
    if (response == NULL) {
        closesocket(socket_handle);
        return NULL;
    }

    size_t received = 0;
    while (received < RTSP_RESPONSE_CAPACITY &&
        WaitSocket(socket_handle, 0, check->toptions->probe.timeout_ms)) {
        int count = recv(socket_handle, response + received,
            (int)(RTSP_RESPONSE_CAPACITY - received), 0);
        if (count <= 0)
            break;
        received += (size_t)count;
        response[received] = '\0';
        if (strstr(response, "\r\n\r\n") != NULL ||
            strstr(response, "\n\n") != NULL)
            break;
    }
    closesocket(socket_handle);

    if (received < 5 || strncmp(response, "RTSP/", 5) != 0) {
        free(response);
        return NULL;
    }
    return response;
}

char *RtspResponseFmt(const char *response)
{
    if (response == NULL)
        return NULL;

    size_t length = strlen(response);
    const char *end = response + length;
    const char *boundary = strstr(response, "\r\n\r\n");
    if (boundary == NULL)
        boundary = strstr(response, "\n\n");
    if (boundary != NULL)
        end = boundary;

    size_t header_length = (size_t)(end - response);
    char *formatted = malloc(header_length + 2);
    if (formatted == NULL)
        return NULL;

    size_t output = 0;
    for (size_t i = 0; i < header_length; ++i) {
        if (response[i] == '\r') {
            if (i + 1 < header_length && response[i + 1] == '\n')
                ++i;
            formatted[output++] = '\n';
        } else {
            formatted[output++] = response[i];
        }
    }
    if (output == 0 || formatted[output - 1] != '\n')
        formatted[output++] = '\n';
    formatted[output] = '\0';
    return formatted;
}
