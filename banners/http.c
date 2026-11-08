#include <winsock2.h>

#include "http.h"
#include "lookup.h"
#include "pool/cidr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_BANNER_CAPACITY 1024

char *HttpResponseFmt(const char *response)
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

static int WaitSocket(SOCKET socket_handle, int writable, DWORD timeout_ms)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(socket_handle, &set);

    TIMEVAL timeout = {
        (long)(timeout_ms / 1000),
        (long)((timeout_ms % 1000) * 1000)
    };
    int ready = writable
        ? select(0, NULL, &set, NULL, &timeout)
        : select(0, &set, NULL, NULL, &timeout);
    return ready > 0;
}

char *HttpGrab(HostUpCheck *check, uint16_t port)
{
    if (check == NULL || check->toptions == NULL)
        return NULL;
    if (IsOOB(MAX_LOOKUP_INDEX, port))
        return NULL;

    uint32_t services = SERVICES[port];
    if (!(services & SERVICE_HTTP))
        return NULL;

    SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET)
        return NULL;

    u_long nonblocking = 1;
    ioctlsocket(socket_handle, FIONBIO, &nonblocking);

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl((uint32_t)check->toptions->probe.addr);

    int connected = connect(socket_handle, (struct sockaddr *)&address, sizeof(address));
    if (connected == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) {
            closesocket(socket_handle);
            return NULL;
        }
        if (!WaitSocket(socket_handle, 1, check->toptions->probe.timeout_ms)) {
            closesocket(socket_handle);
            return NULL;
        }
        int socket_error = 0;
        int error_size = sizeof(socket_error);
        getsockopt(socket_handle, SOL_SOCKET, SO_ERROR,
            (char *)&socket_error, &error_size);
        if (socket_error != 0) {
            closesocket(socket_handle);
            return NULL;
        }
    }

    char *host = Ipv4Repr(check->toptions->probe.addr);
    if (host == NULL) {
        closesocket(socket_handle);
        return NULL;
    }

    char request[512];
    int request_size = snprintf(request, sizeof(request),
        "GET / HTTP/1.0\r\nHost: %s\r\nUser-Agent: probe/1.0\r\nConnection: close\r\n\r\n",
        host);
    free(host);
    if (request_size <= 0 || (size_t)request_size >= sizeof(request)) {
        closesocket(socket_handle);
        return NULL;
    }

    size_t sent = 0;
    while (sent < (size_t)request_size) {
        if (!WaitSocket(socket_handle, 1, check->toptions->probe.timeout_ms)) {
            closesocket(socket_handle);
            return NULL;
        }
        int count = send(socket_handle, request + sent,
            request_size - (int)sent, 0);
        if (count == SOCKET_ERROR) {
            closesocket(socket_handle);
            return NULL;
        }
        sent += (size_t)count;
    }

    char *response = calloc(HTTP_BANNER_CAPACITY + 1, 1);
    if (response == NULL) {
        closesocket(socket_handle);
        return NULL;
    }

    size_t received = 0;
    while (received < HTTP_BANNER_CAPACITY &&
        WaitSocket(socket_handle, 0, check->toptions->probe.timeout_ms)) {
        int count = recv(socket_handle, response + received,
            (int)(HTTP_BANNER_CAPACITY - received), 0);
        if (count <= 0)
            break;
        received += (size_t)count;
        response[received] = '\0';
        if (strstr(response, "\r\n\r\n") != NULL)
            break;
    }
    closesocket(socket_handle);

    if (received < 5 || strncmp(response, "HTTP/", 5) != 0) {
        free(response);
        return NULL;
    }
    return response;
}
