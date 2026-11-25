#include "internal.h"
#include <stdlib.h>

DWORD TcpConnectorCloseSocketById(char *socket_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (socket_id == NULL || socket_id[0] == '\0')
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    TcpConnectorLockStateLock(connector);
    TcpConnectorSocket *socket = TcpConnectorFindSocket(connector, socket_id);
    if (socket == NULL) {
        TcpConnectorUnlockStateLock(connector);
        return TCP_CONNECTOR_NOT_FOUND;
    }
    if (socket->pending_tasks != 0) {
        TcpConnectorUnlockStateLock(connector);
        return TCP_CONNECTOR_BUSY;
    }

    if (socket->handle != INVALID_SOCKET &&
        closesocket(socket->handle) == SOCKET_ERROR) {
        DWORD error = (DWORD)WSAGetLastError();
        TcpConnectorUnlockStateLock(connector);
        return error;
    }
    socket->handle = INVALID_SOCKET;
    socket->connected = FALSE;
    HashMap_Remove(connector->lookup.sockets, socket_id);
    free(socket->id);
    free(socket);
    TcpConnectorUnlockStateLock(connector);
    return TCP_CONNECTOR_SUCCESS;
}
