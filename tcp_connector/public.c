#include "models.h"
#include <stdlib.h>
#include <string.h>

DWORD TcpConnectorCreateTask(TcpConnectorTaskOption options, const char *task_id,
    TcpConnectorCompletionCallback on_completed, void *userdata)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return ERROR_INVALID_STATE;

    if (task_id == NULL || task_id[0] == '\0') {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_INVALID_ARGUMENT);
        return ERROR_INVALID_PARAMETER;
    }

    if (options.kind != TCP_CONNECTOR_TASK_CONNECT && options.kind != TCP_CONNECTOR_TASK_SEND &&
        options.kind != TCP_CONNECTOR_TASK_RECV) {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_UNSUPPORTED_TASK);
        return ERROR_NOT_SUPPORTED;
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
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    task->on_completed = on_completed;
    task->userdata = userdata;

    AcquireSRWLockExclusive(connector->interf.lock);
    if (connector->interf.closing) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        TcpConnectorTaskDestroy(task);
        return ERROR_OPERATION_ABORTED;
    }
    if (FindTask(connector, task_id) != NULL) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        TcpConnectorTaskDestroy(task);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_DUPLICATE_TASK_ID);
        return ERROR_ALREADY_EXISTS;
    }

    TcpConnectorStartQueued(connector);
    while (atomic_load(&connector->interf.running) >=
            connector->config.max_pending && !connector->interf.closing) {
        ++connector->interf.blocked_creators;
        BOOL woke = SleepConditionVariableSRW(
            &connector->interf.slot_available, connector->interf.lock,
            INFINITE, 0);
        --connector->interf.blocked_creators;
        if (connector->interf.blocked_creators == 0)
            WakeAllConditionVariable(&connector->interf.submitters_done);
        if (!woke) {
            DWORD wait_error = GetLastError();
            ReleaseSRWLockExclusive(connector->interf.lock);
            TcpConnectorTaskDestroy(task);
            return wait_error;
        }
    }
    if (connector->interf.closing) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        TcpConnectorTaskDestroy(task);
        return ERROR_OPERATION_ABORTED;
    }
    if (FindTask(connector, task_id) != NULL) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        TcpConnectorTaskDestroy(task);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_DUPLICATE_TASK_ID);
        return ERROR_ALREADY_EXISTS;
    }
    if (!HashMap_Set(connector->lookup.tasks, task->id, PointerValue(task))) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        TcpConnectorTaskDestroy(task);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_OUT_OF_MEMORY);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SetConnectorError(connector, TCP_CONNECTOR_SUCCESS);
    TcpConnectorStartQueued(connector);
    WakeConditionVariable(&connector->interf.drainer_wake);
    ReleaseSRWLockExclusive(connector->interf.lock);
    return ERROR_SUCCESS;
}

DWORD TcpConnectorGetTask(const char *task_id, TcpConnectorTask *task)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return ERROR_INVALID_STATE;

    if (task_id == NULL || task == NULL)
        return ERROR_INVALID_PARAMETER;

    AcquireSRWLockExclusive(connector->interf.lock);
    TcpConnectorTask *stored = FindTask(connector, task_id);
    if (stored != NULL)
        *task = *stored;
    ReleaseSRWLockExclusive(connector->interf.lock);
    if (stored == NULL) {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }
    return ERROR_SUCCESS;
}

DWORD TcpConnectorCancelTask(const char *task_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return ERROR_INVALID_STATE;

    if (task_id == NULL)
        return ERROR_INVALID_PARAMETER;

    AcquireSRWLockExclusive(connector->interf.lock);
    TcpConnectorTask *task = FindTask(connector, task_id);
    if (task == NULL) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }
    if (task->status == TCP_CONNECTOR_TASK_COMPLETED ||
        task->status == TCP_CONNECTOR_TASK_CANCELED) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        return ERROR_SUCCESS;
    }

    TcpConnectorPendingIo *io_request = FindPendingIo(connector, task_id);
    task->status = TCP_CONNECTOR_TASK_CANCELED;
    task->error = TCP_CONNECTOR_ERROR_CANCELED;
    task->WSA_error = WSA_OPERATION_ABORTED;
    WakeConditionVariable(&connector->interf.drainer_wake);
    if (io_request == NULL || !io_request->in_flight) {
        RemovePendingIo(connector, io_request);
        ReleaseSRWLockExclusive(connector->interf.lock);
        return ERROR_SUCCESS;
    }

    io_request->cancel_requested = TRUE;
    if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT) {
        CloseSocket(io_request->socket);
    } else if (io_request->socket != NULL &&
        CancelIoEx((HANDLE)io_request->socket->handle,
            &io_request->overlapped) == 0 && GetLastError() != ERROR_NOT_FOUND) {
        DWORD error = GetLastError();
        ReleaseSRWLockExclusive(connector->interf.lock);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_IO);
        return error;
    }
    ReleaseSRWLockExclusive(connector->interf.lock);
    return ERROR_SUCCESS;
}

DWORD TcpConnectorCloseSocketById(char *socket_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return ERROR_INVALID_STATE;

    if (socket_id == NULL || socket_id[0] == '\0')
        return ERROR_INVALID_PARAMETER;

    AcquireSRWLockExclusive(connector->interf.lock);
    TcpConnectorSocket *socket = FindSocket(connector, socket_id);
    if (socket == NULL) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        return ERROR_NOT_FOUND;
    }
    if (socket->pending_tasks != 0) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        return ERROR_BUSY;
    }

    if (socket->handle != INVALID_SOCKET &&
        closesocket(socket->handle) == SOCKET_ERROR) {
        DWORD error = (DWORD)WSAGetLastError();
        ReleaseSRWLockExclusive(connector->interf.lock);
        return error;
    }
    socket->handle = INVALID_SOCKET;
    socket->connected = FALSE;
    HashMap_Remove(connector->lookup.sockets, socket_id);
    free(socket);
    ReleaseSRWLockExclusive(connector->interf.lock);
    return ERROR_SUCCESS;
}

DWORD TcpConnectorDiscardTask(const char *task_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return ERROR_INVALID_STATE;

    if (task_id == NULL || task_id[0] == '\0')
        return ERROR_INVALID_PARAMETER;

    AcquireSRWLockExclusive(connector->interf.lock);
    TcpConnectorTask *task = FindTask(connector, task_id);
    if (task == NULL) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }

    if (task->callback_running ||
        (task->status != TCP_CONNECTOR_TASK_COMPLETED &&
            task->status != TCP_CONNECTOR_TASK_CANCELED)) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        return ERROR_BUSY;
    }

    TcpConnectorPendingIo *io = FindPendingIo(connector, task_id);
    if (io != NULL && io->in_flight) {
        ReleaseSRWLockExclusive(connector->interf.lock);
        return ERROR_BUSY;
    }

    RemovePendingIo(connector, io);
    HashMap_Remove(connector->lookup.tasks, task_id);
    ReleaseSRWLockExclusive(connector->interf.lock);

    TcpConnectorTaskDestroy(task);
    return ERROR_SUCCESS;
}

DWORD TcpConnectorWaitTask(const char *task_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return ERROR_INVALID_STATE;

    if (task_id == NULL)
        return ERROR_INVALID_PARAMETER;

    for (;;) {
        AcquireSRWLockExclusive(connector->interf.lock);
        TcpConnectorTask *task = FindTask(connector, task_id);
        if (task == NULL) {
            ReleaseSRWLockExclusive(connector->interf.lock);
            SetConnectorError(connector, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
            return ERROR_NOT_FOUND;
        }
        TcpConnectorStartQueued(connector);
        if (task->status == TCP_CONNECTOR_TASK_COMPLETED ||
            (task->status == TCP_CONNECTOR_TASK_CANCELED &&
                (FindPendingIo(connector, task_id) == NULL ||
                    !FindPendingIo(connector, task_id)->in_flight))) {
            DWORD error = task->WSA_error;
            task->completion_reported = TRUE;
            ReleaseSRWLockExclusive(connector->interf.lock);
            return error;
        }
        ReleaseSRWLockExclusive(connector->interf.lock);

        DWORD error = ProcessCompletion(connector, WaitMilliseconds(connector));
        if (error != ERROR_SUCCESS) {
            SetConnectorError(connector, TCP_CONNECTOR_ERROR_IO);
            return error;
        }
    }
}

typedef struct {
    int queued;
} QueuedContext;

static int FindQueuedEntry(const char *key, const Value *value, void *context)
{
    (void)key;
    TcpConnectorTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    if (task != NULL && task->status == TCP_CONNECTOR_TASK_QUEUED)
        ((QueuedContext *)context)->queued = 1;
    return 0;
}

typedef struct {
    TcpConnectorLoop *connector;
    TcpConnectorTask *completed;
} CompletedTaskContext;

static int FindUnreportedTask(const char *key, const Value *value,
    void *context)
{
    (void)key;
    TcpConnectorTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    CompletedTaskContext *completed = context;
    if (completed->completed != NULL || task == NULL || task->completion_reported ||
        (task->status != TCP_CONNECTOR_TASK_COMPLETED &&
            task->status != TCP_CONNECTOR_TASK_CANCELED))
        return 0;

    TcpConnectorPendingIo *io = FindPendingIo(completed->connector, task->id);
    if (io != NULL && io->in_flight)
        return 0;

    task->completion_reported = TRUE;
    completed->completed = task;
    return 0;
}

TcpConnectorError TcpConnectorWaitTasks(const char **task_id)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_ERROR_INVALID_ARGUMENT;

    if (task_id == NULL)
        return TCP_CONNECTOR_ERROR_INVALID_ARGUMENT;
    *task_id = NULL;

    for (;;) {
        AcquireSRWLockExclusive(connector->interf.lock);
        TcpConnectorStartQueued(connector);
        CompletedTaskContext completed = { connector, NULL };
        HashMap_RemoveIf(connector->lookup.tasks, FindUnreportedTask, &completed);
        if (completed.completed != NULL)
            *task_id = completed.completed->id;
        QueuedContext queued = { 0 };
        HashMap_RemoveIf(connector->lookup.tasks, FindQueuedEntry, &queued);
        uint32_t running = atomic_load(&connector->interf.running);
        ReleaseSRWLockExclusive(connector->interf.lock);

        if (completed.completed != NULL)
            return TCP_CONNECTOR_SUCCESS;
        if (running == 0 && !queued.queued)
            return TCP_CONNECTOR_ERROR_NO_PENDING;
        DWORD error = ProcessCompletion(connector, WaitMilliseconds(connector));
        if (error != ERROR_SUCCESS) {
            SetConnectorError(connector, TCP_CONNECTOR_ERROR_IO);
            return TCP_CONNECTOR_ERROR_IO;
        }
    }
}


