#include <winsock2.h>

#include "ftp.h"
#include "lookup.h"

#include <stdlib.h>
#include <string.h>

#define FTP_BANNER_CAPACITY 1024

static int WaitReadable(SOCKET socket_handle, DWORD timeout_ms)
{
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(socket_handle, &readable);

    TIMEVAL timeout = {
        (long)(timeout_ms / 1000),
        (long)((timeout_ms % 1000) * 1000)
    };
    return select(0, &readable, NULL, NULL, &timeout) > 0;
}

static int WaitWritable(SOCKET socket_handle, DWORD timeout_ms)
{
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(socket_handle, &writable);

    TIMEVAL timeout = {
        (long)(timeout_ms / 1000),
        (long)((timeout_ms % 1000) * 1000)
    };
    return select(0, NULL, &writable, NULL, &timeout) > 0;
}

static int FtpGreetingComplete(const char *response, size_t length)
{
    const char *line_end = NULL;
    for (size_t i = 0; i + 1 < length; ++i) {
        if (response[i] == '\r' && response[i + 1] == '\n') {
            line_end = response + i;
            break;
        }
    }
    if (line_end == NULL)
        return 0;

    if (line_end - response < 4 || response[3] != '-')
        return 1;

    char terminator[5] = { response[0], response[1], response[2], ' ', '\0' };
    for (const char *line = line_end + 2; line + 3 < response + length;) {
        const char *next = strstr(line, "\r\n");
        if (next == NULL)
            break;
        if (next - line >= 4 && memcmp(line, terminator, 4) == 0)
            return 1;
        line = next + 2;
    }
    return 0;
}

char *FtpGrab(HostUpCheck *check, uint16_t port)
{
    if (check == NULL || check->toptions == NULL)
        return NULL;
    if (IsOOB(MAX_LOOKUP_INDEX, port))
        return NULL;

    uint32_t services = SERVICES[port];
    if (!(services & SERVICE_FTP))
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
        if (!WaitWritable(socket_handle, check->toptions->probe.timeout_ms)) {
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

    char *response = calloc(FTP_BANNER_CAPACITY + 1, 1);
    if (response == NULL) {
        closesocket(socket_handle);
        return NULL;
    }

    size_t received = 0;
    while (received < FTP_BANNER_CAPACITY &&
        WaitReadable(socket_handle, check->toptions->probe.timeout_ms)) {
        int count = recv(socket_handle, response + received,
            (int)(FTP_BANNER_CAPACITY - received), 0);
        if (count <= 0)
            break;
        received += (size_t)count;
        response[received] = '\0';
        if (FtpGreetingComplete(response, received))
            break;
    }
    closesocket(socket_handle);

    if (received == 0) {
        free(response);
        return NULL;
    }
    return response;
}

char *FtpResponseFmt(const char *response)
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
