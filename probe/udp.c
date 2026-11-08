#include <winsock2.h>

#include "udp.h"

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

UdpResult ProbeUdp(const HostUpCheckProbeOptions *options, uint16_t port)
{
    SOCKET socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_handle == INVALID_SOCKET)
        return UDP_RESULT_ERROR;

    BOOL reset_notifications = FALSE;
    DWORD bytes_returned = 0;
    if (WSAIoctl(socket_handle, SIO_UDP_CONNRESET, &reset_notifications,
            sizeof(reset_notifications), NULL, 0, &bytes_returned, NULL, NULL) == SOCKET_ERROR) {
        closesocket(socket_handle);
        return UDP_RESULT_ERROR;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl((uint32_t)options->addr);

    char payload = 0;
    if (sendto(socket_handle, &payload, sizeof(payload), 0,
            (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        closesocket(socket_handle);
        return UDP_RESULT_ERROR;
    }

    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(socket_handle, &readable);
    TIMEVAL timeout = {
        (long)(options->timeout_ms / 1000),
        (long)((options->timeout_ms % 1000) * 1000)
    };
    int ready = select(0, &readable, NULL, NULL, &timeout);
    if (ready <= 0) {
        closesocket(socket_handle);
        return ready == 0 ? UDP_RESULT_TIMEOUT : UDP_RESULT_ERROR;
    }

    char response;
    int received = recv(socket_handle, &response, sizeof(response), 0);
    int error = WSAGetLastError();
    closesocket(socket_handle);
    if (received >= 0)
        return UDP_RESULT_RESPONSE_RECEIVED;
    return error == WSAECONNRESET ? UDP_RESULT_PORT_UNREACHABLE : UDP_RESULT_ERROR;
}
