#include <winsock2.h>

#include "lookup.h"
#include "ssh.h"

#include <stdlib.h>
#include <string.h>

#define SSH_BANNER_CAPACITY 1024

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

static int HasSshIdentification(const char *response, size_t length)
{
    const char *line = response;
    const char *end = response + length;
    while (line < end) {
        const char *line_end = memchr(line, '\n', (size_t)(end - line));
        if (line_end == NULL)
            return 0;

        const char *content_end = line_end;
        if (content_end > line && content_end[-1] == '\r')
            --content_end;
        if (content_end - line >= 4 && memcmp(line, "SSH-", 4) == 0)
            return 1;
        line = line_end + 1;
    }
    return 0;
}

char *SshGrab(HostUpCheck *check, uint16_t port)
{
    if (check == NULL || check->toptions == NULL ||
        IsOOB(MAX_LOOKUP_INDEX, port))
        return NULL;

    uint32_t services = SERVICES[port];
    if (!(services & SERVICE_SSH))
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

    int connected = connect(socket_handle, (struct sockaddr *)&address,
        sizeof(address));
    if (connected == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) {
            closesocket(socket_handle);
            return NULL;
        }
        if (!WaitSocket(socket_handle, 1,
                check->toptions->probe.timeout_ms)) {
            closesocket(socket_handle);
            return NULL;
        }

        int socket_error = 0;
        int error_size = sizeof(socket_error);
        if (getsockopt(socket_handle, SOL_SOCKET, SO_ERROR,
                (char *)&socket_error, &error_size) == SOCKET_ERROR ||
            socket_error != 0) {
            closesocket(socket_handle);
            return NULL;
        }
    }

    char *response = calloc(SSH_BANNER_CAPACITY + 1, 1);
    if (response == NULL) {
        closesocket(socket_handle);
        return NULL;
    }

    size_t received = 0;
    while (received < SSH_BANNER_CAPACITY &&
        WaitSocket(socket_handle, 0, check->toptions->probe.timeout_ms)) {
        int count = recv(socket_handle, response + received,
            (int)(SSH_BANNER_CAPACITY - received), 0);
        if (count <= 0)
            break;
        received += (size_t)count;
        response[received] = '\0';
        if (HasSshIdentification(response, received))
            break;
    }
    closesocket(socket_handle);

    if (!HasSshIdentification(response, received)) {
        free(response);
        return NULL;
    }
    return response;
}

char *SshResponseFmt(const char *response)
{
    if (response == NULL)
        return NULL;

    size_t length = strlen(response);
    char *formatted = malloc(length + 2);
    if (formatted == NULL)
        return NULL;

    size_t output = 0;
    for (size_t i = 0; i < length; ++i) {
        if (response[i] == '\r') {
            if (i + 1 < length && response[i + 1] == '\n')
                ++i;
            formatted[output++] = '\n';
        } else if (response[i] == '\n') {
            formatted[output++] = '\n';
        } else if ((unsigned char)response[i] >= 0x20 || response[i] == '\t') {
            formatted[output++] = response[i];
        }
    }

    if (output == 0 || formatted[output - 1] != '\n')
        formatted[output++] = '\n';
    formatted[output] = '\0';
    return formatted;
}
