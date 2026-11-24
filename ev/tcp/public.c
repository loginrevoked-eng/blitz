#include "models.h"
#include <stdlib.h>

DWORD TcpConnectorGetTask(const char *task_id, TcpConnectorTask *task)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (task_id == NULL || task == NULL)
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    AcquireSRWLockExclusive(connector->control.lock);
    TcpConnectorTask *stored = TcpConnectorFindTask(connector, task_id);
    if (stored != NULL)
        *task = *stored;
    ReleaseSRWLockExclusive(connector->control.lock);
    if (stored == NULL) {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
        return TCP_CONNECTOR_NOT_FOUND;
    }
    return TCP_CONNECTOR_SUCCESS;
}

DWORD TcpConnectorCancelTask(const char *task_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (task_id == NULL)
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    AcquireSRWLockExclusive(connector->control.lock);
    TcpConnectorTask *task = TcpConnectorFindTask(connector, task_id);
    if (task == NULL) {
        ReleaseSRWLockExclusive(connector->control.lock);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
        return TCP_CONNECTOR_NOT_FOUND;
    }
    if (task->status == TCP_CONNECTOR_TASK_COMPLETED ||
        task->status == TCP_CONNECTOR_TASK_CANCELED) {
        ReleaseSRWLockExclusive(connector->control.lock);
        return TCP_CONNECTOR_SUCCESS;
    }

    TcpConnectorPendingIo *io_request = TcpConnectorFindPendingIo(connector, task_id);
    task->status = TCP_CONNECTOR_TASK_CANCELED;
    task->WSA_error = WSA_OPERATION_ABORTED;
    if (io_request == NULL || !io_request->in_flight)
        TcpConnectorReportCompletion(connector, task);
    PostQueuedCompletionStatus(connector->internal.completion_port, 0,
        TCP_CONNECTOR_IO_COMPLETION_WORKER_WAKE_KEY, NULL);
    if (io_request == NULL || !io_request->in_flight) {
        RemovePendingIo(connector, io_request);
        TcpConnectorTaskReadySubmitCallback(connector, task);
        ReleaseSRWLockExclusive(connector->control.lock);
        return TCP_CONNECTOR_SUCCESS;
    }

    io_request->cancel_requested = TRUE;
    if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT) {
        CloseSocket(io_request->socket);
    } else if (io_request->socket != NULL &&
        CancelIoEx((HANDLE)io_request->socket->handle,
            &io_request->overlapped) == 0 && GetLastError() != TCP_CONNECTOR_NOT_FOUND) {
        DWORD error = GetLastError();
        ReleaseSRWLockExclusive(connector->control.lock);
        SetConnectorError(connector, error);
        return error;
    }
    ReleaseSRWLockExclusive(connector->control.lock);
    return TCP_CONNECTOR_SUCCESS;
}

DWORD TcpConnectorCloseSocketById(char *socket_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (socket_id == NULL || socket_id[0] == '\0')
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    AcquireSRWLockExclusive(connector->control.lock);
    TcpConnectorSocket *socket = FindSocket(connector, socket_id);
    if (socket == NULL) {
        ReleaseSRWLockExclusive(connector->control.lock);
        return TCP_CONNECTOR_NOT_FOUND;
    }
    if (socket->pending_tasks != 0) {
        ReleaseSRWLockExclusive(connector->control.lock);
        return TCP_CONNECTOR_BUSY;
    }

    if (socket->handle != INVALID_SOCKET &&
        closesocket(socket->handle) == SOCKET_ERROR) {
        DWORD error = (DWORD)WSAGetLastError();
        ReleaseSRWLockExclusive(connector->control.lock);
        return error;
    }
    socket->handle = INVALID_SOCKET;
    socket->connected = FALSE;
    HashMap_Remove(connector->lookup.sockets, socket_id);
    free(socket->id);
    free(socket);
    ReleaseSRWLockExclusive(connector->control.lock);
    return TCP_CONNECTOR_SUCCESS;
}


