#include "internal.h"
#include <stdlib.h>
#include <string.h>

void TcpConnectorSetTaskFinished(TcpConnectorLoop *connector, TcpConnectorTask *task,
    DWORD error, TcpConnectorStatus status)
{
    task->status = status;
    task->WSA_error = error;
    TcpConnectorSignalAllTaskFinished(connector);
    TcpConnectorTaskReadySubmitCallback(connector, task);
}


static void CancelExpired(TcpConnectorLoop *connector)
{
    ULONGLONG now = GetTickCount64();
    HashMapIter iter = { 0 };
    for (Value *value = HashMap_IterNext(connector->lookup.pending_io, &iter);
         value != NULL; value = HashMap_IterNext(connector->lookup.pending_io, &iter)) {
        TcpConnectorPendingIo *io = value->kind == VALUE_POINTER ? value->ptr : NULL;
        if (io == NULL || !io->in_flight || !io->has_deadline ||
            io->cancel_requested || io->timed_out || now < io->deadline)
            continue;
        TcpConnectorTask *task = TcpConnectorFindTask(connector, HashMap_IterKey(&iter));
        if (task == NULL || (task->options.kind != TCP_CONNECTOR_TASK_CONNECT &&
            !(task->options.kind == TCP_CONNECTOR_TASK_RECV &&
                task->options.sr_opt.timeout_ms != 0)))
            continue;
        io->timed_out = TRUE;
        TcpConnectorSetTaskFinished(connector, task, WSAETIMEDOUT, TCP_CONNECTOR_TASK_CANCELED);
        TcpConnectorCloseSocket(io->socket);
    }
}

DWORD TcpConnectorHandleCompletion(TcpConnectorLoop *connector, DWORD wait_ms)
{
    DWORD transferred = 0;
    ULONG_PTR key = 0;
    OVERLAPPED *overlapped = NULL;
    BOOL success = GetQueuedCompletionStatus(connector->internal.completion_port,
        &transferred, &key, &overlapped, wait_ms);
    DWORD completion_error = success ? TCP_CONNECTOR_SUCCESS : GetLastError();

    if (overlapped == NULL) {
        if (success && key == TCP_CONNECTOR_IO_COMPLETION_WORKER_WAKE_KEY)
            return TCP_CONNECTOR_SUCCESS;
        if (!success && completion_error == WAIT_TIMEOUT) {
            TcpConnectorLockStateLock(connector);
            CancelExpired(connector);
            TcpConnectorUnlockStateLock(connector);
            return TCP_CONNECTOR_SUCCESS;
        }
        return success ? TCP_CONNECTOR_INVALID_DATA : completion_error;
    }

    TcpConnectorPendingIo *io_request = (TcpConnectorPendingIo *)overlapped;
    TcpConnectorTask *task = NULL;

    TcpConnectorLockStateLock(connector);
    task = TcpConnectorFindTask(connector, io_request->task_id);
    if (task == NULL || TcpConnectorFindPendingIo(connector, io_request->task_id) != io_request ||
        !io_request->in_flight || (TcpConnectorSocket *)key != io_request->socket) {
        TcpConnectorUnlockStateLock(connector);
        TcpConnectorSetError(connector, TCP_CONNECTOR_INVALID_DATA);
        return TCP_CONNECTOR_INVALID_DATA;
    }

    TcpConnectorSocket *socket = io_request->socket;
    io_request->in_flight = FALSE;
    if (socket != NULL && socket->pending_tasks > 0)
        --socket->pending_tasks;
    atomic_fetch_sub(&connector->control.running, 1);
    TcpConnectorSignalSlotAvailable(connector);
    TcpConnectorSignalAllTaskFinished(connector);

    if (io_request->timed_out)
        completion_error = WSAETIMEDOUT;

    if (io_request->cancel_requested) {
        TcpConnectorSetTaskFinished(connector, task, WSA_OPERATION_ABORTED, TCP_CONNECTOR_TASK_CANCELED);
        if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT && socket != NULL) {
            HashMap_Remove(connector->lookup.sockets, task->id);
            TcpConnectorCloseSocket(socket);
            free(socket);
            io_request->socket = NULL;
        }
        TcpConnectorRemovePendingIo(connector, io_request);
    } else if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT) {
        if (completion_error == TCP_CONNECTOR_SUCCESS) {
            if (setsockopt(socket->handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,
                    NULL, 0) == SOCKET_ERROR)
                completion_error = (DWORD)WSAGetLastError();
        }

        if (completion_error == TCP_CONNECTOR_SUCCESS) {
            socket->connected = TRUE;
            TcpConnectorSetTaskFinished(connector, task, TCP_CONNECTOR_SUCCESS, TCP_CONNECTOR_TASK_COMPLETED);
            TcpConnectorRemovePendingIo(connector, io_request);
        } else {
            HashMap_Remove(connector->lookup.sockets, task->id);
            TcpConnectorCloseSocket(socket);
            free(socket);
            io_request->socket = NULL;
            if (!io_request->timed_out &&
                io_request->attempts <= task->options.conn_opt.retries) {
                task->WSA_error = completion_error;
                TcpConnectorStartTask(connector, task);
            } else if (io_request->timed_out &&
                io_request->attempts <= task->options.conn_opt.retries) {
                task->WSA_error = WSAETIMEDOUT;
                TcpConnectorStartTask(connector, task);
            } else {
                TcpConnectorSetTaskFinished(connector, task, completion_error, TCP_CONNECTOR_TASK_COMPLETED);
                TcpConnectorRemovePendingIo(connector, io_request);
            }
        }
    } else {
        if (task->options.kind == TCP_CONNECTOR_TASK_RECV && completion_error == TCP_CONNECTOR_SUCCESS)
            task->options.sr_opt.payload->len = transferred;
        TcpConnectorSetTaskFinished(connector, task, completion_error, TCP_CONNECTOR_TASK_COMPLETED);
        TcpConnectorRemovePendingIo(connector, io_request);
    }

    TcpConnectorUnlockStateLock(connector);
    return TCP_CONNECTOR_SUCCESS;
}




