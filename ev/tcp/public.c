#include "models.h"
#include <stdlib.h>
#include <string.h>

DWORD TcpConnectorCreateTask(TcpConnectorTaskOption options, const char *task_id,
    TcpConnectorCompletionCallback on_completed, void *userdata)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (task_id == NULL || task_id[0] == '\0') {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_INVALID_ARGUMENT);
        return TCP_CONNECTOR_INVALID_ARGUMENT;
    }

    if (options.kind != TCP_CONNECTOR_TASK_CONNECT && options.kind != TCP_CONNECTOR_TASK_SEND &&
        options.kind != TCP_CONNECTOR_TASK_RECV) {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_UNSUPPORTED_TASK);
        return TCP_CONNECTOR_NOT_SUPPORTED;
    }
    if (options.kind == TCP_CONNECTOR_TASK_CONNECT && options.conn_opt.port == 0) {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_INVALID_ARGUMENT);
        return WSAEINVAL;
    }
    if ((options.kind == TCP_CONNECTOR_TASK_SEND || options.kind == TCP_CONNECTOR_TASK_RECV) &&
        (options.sr_opt.socket_id == NULL || options.sr_opt.payload == NULL ||
            (options.kind == TCP_CONNECTOR_TASK_SEND && options.sr_opt.payload->len == 0) ||
            (options.kind == TCP_CONNECTOR_TASK_RECV && options.sr_opt.payload->cap == 0))) {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_INVALID_ARGUMENT);
        return WSAEINVAL;
    }

    TcpConnectorTask *task = TcpConnectorTaskNew(task_id, options);
    if (task == NULL) {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_OUT_OF_MEMORY);
        return TCP_CONNECTOR_OUT_OF_MEMORY;
    }
    task->on_completed = on_completed;
    task->userdata = userdata;

    AcquireSRWLockExclusive(connector->control.lock);
    if (connector->control.closing) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        return TCP_CONNECTOR_OPERATION_ABORTED;
    }
    if (FindTask(connector, task_id) != NULL) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_DUPLICATE_TASK_ID);
        return TCP_CONNECTOR_ALREADY_EXISTS;
    }

    while (atomic_load(&connector->control.running) >=
            connector->config.max_pending && !connector->control.closing) {
        if (connector->control.io_completion_worker != NULL &&
            GetCurrentThreadId() == GetThreadId(connector->control.io_completion_worker)) {
            ReleaseSRWLockExclusive(connector->control.lock);
            DWORD completion_error = TcpConnectorHandleCompletion(connector, INFINITE);
            AcquireSRWLockExclusive(connector->control.lock);
            if (completion_error != TCP_CONNECTOR_SUCCESS) {
                ReleaseSRWLockExclusive(connector->control.lock);
                TcpConnectorTaskDestroy(task);
                SetConnectorError(connector, completion_error);
                return completion_error;
            }
            continue;
        }

        ++connector->control.blocked_creators;
        BOOL woke = SleepConditionVariableSRW(
            &connector->control.slot_available, connector->control.lock,
            INFINITE, 0);
        --connector->control.blocked_creators;
        if (connector->control.blocked_creators == 0)
            WakeAllConditionVariable(&connector->control.submitters_done);
        if (!woke) {
            DWORD wait_error = GetLastError();
            ReleaseSRWLockExclusive(connector->control.lock);
            TcpConnectorTaskDestroy(task);
            return wait_error;
        }
    }
    if (connector->control.closing) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        return TCP_CONNECTOR_OPERATION_ABORTED;
    }
    if (FindTask(connector, task_id) != NULL) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_DUPLICATE_TASK_ID);
        return TCP_CONNECTOR_ALREADY_EXISTS;
    }
    if (!HashMap_Set(connector->lookup.tasks, task->id, PointerValue(task))) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_OUT_OF_MEMORY);
        return TCP_CONNECTOR_OUT_OF_MEMORY;
    }

    SetConnectorError(connector, TCP_CONNECTOR_SUCCESS);
    TcpConnectorStartTask(connector, task);
    ReleaseSRWLockExclusive(connector->control.lock);
    return TCP_CONNECTOR_SUCCESS;
}

DWORD TcpConnectorGetTask(const char *task_id, TcpConnectorTask *task)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (task_id == NULL || task == NULL)
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    AcquireSRWLockExclusive(connector->control.lock);
    TcpConnectorTask *stored = FindTask(connector, task_id);
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
    TcpConnectorTask *task = FindTask(connector, task_id);
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

    TcpConnectorPendingIo *io_request = FindPendingIo(connector, task_id);
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

DWORD TcpConnectorWaitTask(const char *task_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (task_id == NULL)
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    AcquireSRWLockExclusive(connector->control.lock);
    TcpConnectorTask *task = FindTask(connector, task_id);
    if (task == NULL) {
        ReleaseSRWLockExclusive(connector->control.lock);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
        return TCP_CONNECTOR_NOT_FOUND;
    }
    while (task->status != TCP_CONNECTOR_TASK_COMPLETED &&
        task->status != TCP_CONNECTOR_TASK_CANCELED) {
        if (!SleepConditionVariableSRW(&connector->control.task_finished,
                connector->control.lock, INFINITE, 0)) {
            DWORD error = GetLastError();
            ReleaseSRWLockExclusive(connector->control.lock);
            return error;
        }
    }
    while (task->status == TCP_CONNECTOR_TASK_CANCELED) {
        TcpConnectorPendingIo *io = FindPendingIo(connector, task_id);
        if (io == NULL || !io->in_flight) break;
        if (!SleepConditionVariableSRW(&connector->control.task_finished,
                connector->control.lock, INFINITE, 0)) {
            DWORD error = GetLastError();
            ReleaseSRWLockExclusive(connector->control.lock);
            return error;
        }
    }
    DWORD error = task->WSA_error;
    ReleaseSRWLockExclusive(connector->control.lock);
    return error;
}

DWORD TcpConnectorWaitTasks(const char **task_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (task_id == NULL)
        return TCP_CONNECTOR_ERROR_INVALID_ARGUMENT;
    *task_id = NULL;

    AcquireSRWLockExclusive(connector->control.lock);
    while (connector->control.completion_head == NULL) {
        if (atomic_load(&connector->control.running) == 0 &&
            connector->control.callback_head == NULL) {
            ReleaseSRWLockExclusive(connector->control.lock);
            return TCP_CONNECTOR_ERROR_NO_PENDING;
        }
        if (!SleepConditionVariableSRW(&connector->control.task_finished,
                connector->control.lock, INFINITE, 0)) {
            ReleaseSRWLockExclusive(connector->control.lock);
            return GetLastError();
        }
    }
    TcpConnectorTask *completed = connector->control.completion_head;
    connector->control.completion_head = completed->completion_next;
    if (connector->control.completion_head == NULL)
        connector->control.completion_tail = NULL;
    completed->completion_next = NULL;
    completed->completion_queued = FALSE;
    completed->completion_reported = TRUE;
    *task_id = completed->id;
    ReleaseSRWLockExclusive(connector->control.lock);
    return TCP_CONNECTOR_SUCCESS;
}


