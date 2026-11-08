#include <winsock2.h>

#include "tcp.h"

TcpResult ProbeTcp(const HostUpCheckProbeOptions *options, uint16_t port)
{
    SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET)
        return TCP_RESULT_ERROR;

    u_long nonblocking = 1;
    ioctlsocket(socket_handle, FIONBIO, &nonblocking);

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl((uint32_t)options->addr);

    int connected = connect(socket_handle, (struct sockaddr *)&address, sizeof(address));
    if (connected == 0) {
        closesocket(socket_handle);
        return TCP_RESULT_SYN_ACK_RECEIVED;
    }

    int error = WSAGetLastError();
    if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) {
        closesocket(socket_handle);
        return error == WSAECONNREFUSED ? TCP_RESULT_RESET_RECEIVED : TCP_RESULT_ERROR;
    }

    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(socket_handle, &writable);
    TIMEVAL timeout = {
        (long)(options->timeout_ms / 1000),
        (long)((options->timeout_ms % 1000) * 1000)
    };
    int ready = select(0, NULL, &writable, NULL, &timeout);
    error = 0;
    int error_size = sizeof(error);
    if (ready > 0)
        getsockopt(socket_handle, SOL_SOCKET, SO_ERROR, (char *)&error, &error_size);
    closesocket(socket_handle);

    if (ready == 0)
        return TCP_RESULT_TIMEOUT;
    if (ready < 0)
        return TCP_RESULT_ERROR;
    if (error == 0)
        return TCP_RESULT_SYN_ACK_RECEIVED;
    if (error == WSAECONNREFUSED)
        return TCP_RESULT_RESET_RECEIVED;
    return TCP_RESULT_ERROR;
}
