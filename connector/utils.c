#include "internal.h"
#include <stdlib.h>

Value TcpConnectorPointerValue(void *pointer)
{
    Value value = { 0 };
    value.kind = VALUE_POINTER;
    value.ptr = pointer;
    return value;
}

void TcpConnectorSetError(TcpConnectorLoop *connector, DWORD error)
{
    if (connector != NULL)
        atomic_store(&connector->control.error, (unsigned int)error);
}

TcpConnectorTask *TcpConnectorFindTask(TcpConnectorLoop *connector, const char *task_id)
{
    Value *value = HashMap_Get(connector->lookup.tasks, task_id);
    return value != NULL && value->kind == VALUE_POINTER ? value->ptr : NULL;
}

TcpConnectorPendingIo *TcpConnectorFindPendingIo(TcpConnectorLoop *connector, const char *task_id)
{
    Value *value = HashMap_Get(connector->lookup.pending_io, task_id);
    return value != NULL && value->kind == VALUE_POINTER ? value->ptr : NULL;
}

TcpConnectorSocket *TcpConnectorFindSocket(TcpConnectorLoop *connector, const char *socket_id)
{
    Value *value = HashMap_Get(connector->lookup.sockets, socket_id);
    return value != NULL && value->kind == VALUE_POINTER ? value->ptr : NULL;
}

void TcpConnectorCloseSocket(TcpConnectorSocket *socket)
{
    if (socket != NULL && socket->handle != INVALID_SOCKET) {
        closesocket(socket->handle);
        socket->handle = INVALID_SOCKET;
        socket->connected = FALSE;
    }
    if (socket != NULL) {
        free(socket->id);
        socket->id = NULL;
    }
}

void TcpConnectorRemovePendingIo(TcpConnectorLoop *connector, TcpConnectorPendingIo *io_request)
{
    if (io_request == NULL)
        return;
    HashMap_Remove(connector->lookup.pending_io, io_request->task_id);
    free(io_request);
}


