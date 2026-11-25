#include "internal.h"
#include <stdlib.h>
#include <string.h>

TcpConnectorTask *TcpConnectorTaskNew(const char *task_id, TcpConnectorTaskOption options)
{
    if (task_id == NULL || task_id[0] == '\0')
        return NULL;
    TcpConnectorTask *task = calloc(1, sizeof(*task));
    if (task == NULL)
        return NULL;
    size_t id_length = strlen(task_id) + 1;
    task->id = malloc(id_length);
    if (task->id == NULL) {
        free(task);
        return NULL;
    }
    memcpy(task->id, task_id, id_length);
    task->options = options;
    if (options.kind == TCP_CONNECTOR_TASK_SEND || options.kind == TCP_CONNECTOR_TASK_RECV) {
        task->options.sr_opt.kind = options.kind;
        if (options.sr_opt.socket_id)
        {
            task->options.sr_opt.socket_id = strdup(options.sr_opt.socket_id);
        }
    }
    task->status = TCP_CONNECTOR_TASK_PENDING;
    return task;
}

#include "internal.h"
#include <stdlib.h>

DWORD TcpConnectorGetTask(const char *task_id, TcpConnectorTask *task)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (task_id == NULL || task == NULL)
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    TcpConnectorLockStateLock(connector);
    TcpConnectorTask *stored = TcpConnectorFindTask(connector, task_id);
    if (stored != NULL)
        *task = *stored;
    TcpConnectorUnlockStateLock(connector);
    if (stored == NULL) {
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
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

    TcpConnectorLockStateLock(connector);
    TcpConnectorTask *task = TcpConnectorFindTask(connector, task_id);
    if (task == NULL) {
        TcpConnectorUnlockStateLock(connector);
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
        return TCP_CONNECTOR_NOT_FOUND;
    }
    if (task->status == TCP_CONNECTOR_TASK_COMPLETED ||
        task->status == TCP_CONNECTOR_TASK_CANCELED) {
        TcpConnectorUnlockStateLock(connector);
        return TCP_CONNECTOR_SUCCESS;
    }

    TcpConnectorPendingIo *io_request = TcpConnectorFindPendingIo(connector, task_id);
    task->status = TCP_CONNECTOR_TASK_CANCELED;
    task->WSA_error = WSA_OPERATION_ABORTED;
    if (io_request == NULL || !io_request->in_flight)
        TcpConnectorSignalAllTaskFinished(connector);
    PostQueuedCompletionStatus(connector->internal.completion_port, 0,
        TCP_CONNECTOR_IO_COMPLETION_WORKER_WAKE_KEY, NULL);
    if (io_request == NULL || !io_request->in_flight) {
        TcpConnectorRemovePendingIo(connector, io_request);
        TcpConnectorTaskReadySubmitCallback(connector, task);
        TcpConnectorUnlockStateLock(connector);
        return TCP_CONNECTOR_SUCCESS;
    }

    io_request->cancel_requested = TRUE;
    if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT) {
        TcpConnectorCloseSocket(io_request->socket);
    } else if (io_request->socket != NULL &&
        CancelIoEx((HANDLE)io_request->socket->handle,
            &io_request->overlapped) == 0 && GetLastError() != TCP_CONNECTOR_NOT_FOUND) {
        DWORD error = GetLastError();
        TcpConnectorUnlockStateLock(connector);
        TcpConnectorSetError(connector, error);
        return error;
    }
    TcpConnectorUnlockStateLock(connector);
    return TCP_CONNECTOR_SUCCESS;
}
