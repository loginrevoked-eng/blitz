#include "models.h"
#include <stdlib.h>

Value PointerValue(void *pointer)
{
    Value value = { 0 };
    value.kind = VALUE_POINTER;
    value.ptr = pointer;
    return value;
}

void SetConnectorError(TcpConnectorLoop *connector, TcpConnectorError error)
{
    if (connector != NULL)
        atomic_store(&connector->interf.error, (unsigned int)error);
}

TcpConnectorTask *FindTask(TcpConnectorLoop *connector, const char *task_id)
{
    Value *value = HashMap_Get(connector->lookup.tasks, task_id);
    return value != NULL && value->kind == VALUE_POINTER ? value->ptr : NULL;
}

TcpConnectorPendingIo *FindPendingIo(TcpConnectorLoop *connector, const char *task_id)
{
    Value *value = HashMap_Get(connector->lookup.pending_io, task_id);
    return value != NULL && value->kind == VALUE_POINTER ? value->ptr : NULL;
}

TcpConnectorSocket *FindSocket(TcpConnectorLoop *connector, const char *socket_id)
{
    Value *value = HashMap_Get(connector->lookup.sockets, socket_id);
    return value != NULL && value->kind == VALUE_POINTER ? value->ptr : NULL;
}

void CloseSocket(TcpConnectorSocket *socket)
{
    if (socket != NULL && socket->handle != INVALID_SOCKET) {
        closesocket(socket->handle);
        socket->handle = INVALID_SOCKET;
        socket->connected = FALSE;
    }
}

void RemovePendingIo(TcpConnectorLoop *connector, TcpConnectorPendingIo *io_request)
{
    if (io_request == NULL)
        return;
    HashMap_Remove(connector->lookup.pending_io, io_request->task_id);
    free(io_request);
}

DWORD TaskErrorToConnectorError(DWORD error)
{
    if (error == ERROR_SUCCESS)
        return TCP_CONNECTOR_SUCCESS;
    if (error == ERROR_NOT_ENOUGH_MEMORY)
        return TCP_CONNECTOR_ERROR_OUT_OF_MEMORY;
    if (error == WSAETIMEDOUT)
        return TCP_CONNECTOR_ERROR_TIMEOUT;
    if (error == WSA_OPERATION_ABORTED)
        return TCP_CONNECTOR_ERROR_CANCELED;
    return TCP_CONNECTOR_ERROR_IO;
}

