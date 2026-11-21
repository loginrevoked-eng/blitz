#include "models.h"
#include <stdlib.h>

typedef struct {
    TcpConnectorLoop *connector;
} CancelAllContext;

static int CancelAllEntry(const char *key, const Value *value, void *context)
{
    CancelAllContext *cancel = context;
    TcpConnectorTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    if (task == NULL || task->status == TCP_CONNECTOR_TASK_COMPLETED ||
        task->status == TCP_CONNECTOR_TASK_CANCELED)
        return 0;

    TcpConnectorPendingIo *io_request = FindPendingIo(cancel->connector, key);
    task->status = TCP_CONNECTOR_TASK_CANCELED;
    task->error = TCP_CONNECTOR_ERROR_CANCELED;
    task->WSA_error = WSA_OPERATION_ABORTED;
    if (io_request == NULL || !io_request->in_flight) {
        RemovePendingIo(cancel->connector, io_request);
    } else {
        io_request->cancel_requested = TRUE;
        if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT) {
            CloseSocket(io_request->socket);
        } else if (io_request->socket != NULL) {
            CancelIoEx((HANDLE)io_request->socket->handle,
                &io_request->overlapped);
        }
    }
    return 0;
}

static int FreeTaskEntry(const char *key, const Value *value, void *context)
{
    (void)key;
    (void)context;
    TcpConnectorTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    TcpConnectorTaskDestroy(task);
    return 1;
}

static int FreePendingIoEntry(const char *key, const Value *value,
    void *context)
{
    (void)key;
    (void)context;
    free(value->kind == VALUE_POINTER ? value->ptr : NULL);
    return 1;
}

static int FreeSocketEntry(const char *key, const Value *value, void *context)
{
    (void)key;
    (void)context;
    TcpConnectorSocket *socket = value->kind == VALUE_POINTER ? value->ptr : NULL;
    if (socket != NULL) {
        CloseSocket(socket);
        free(socket);
    }
    return 1;
}


DWORD TcpConnectorClearEventloop(void)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return ERROR_INVALID_STATE;
    if (connector->interf.drainer_thread != NULL &&
        GetCurrentThreadId() == GetThreadId(connector->interf.drainer_thread))
        return ERROR_BUSY;

    AcquireSRWLockExclusive(connector->interf.lock);
    connector->interf.closing = TRUE;
    WakeAllConditionVariable(&connector->interf.slot_available);
    while (connector->interf.blocked_creators != 0)
        SleepConditionVariableSRW(&connector->interf.submitters_done,
            connector->interf.lock, INFINITE, 0);
    CancelAllContext context = { connector };
    HashMap_RemoveIf(connector->lookup.tasks, CancelAllEntry, &context);
    ReleaseSRWLockExclusive(connector->interf.lock);
    TcpConnectorDetachLoop(connector);

    while (atomic_load(&connector->interf.running) != 0)
        ProcessCompletion(connector, INFINITE);

    if (connector->interf.drainer_thread != NULL) {
        AcquireSRWLockExclusive(connector->interf.lock);
        atomic_store(&connector->interf.drainer_running, FALSE);
        WakeAllConditionVariable(&connector->interf.drainer_wake);
        PostQueuedCompletionStatus(connector->internal.completion_port, 0,
            TCP_CONNECTOR_DRAINER_WAKE_KEY, NULL);
        ReleaseSRWLockExclusive(connector->interf.lock);
        WaitForSingleObject(connector->interf.drainer_thread, INFINITE);
        CloseHandle(connector->interf.drainer_thread);
    }

    HashMap_RemoveIf(connector->lookup.sockets, FreeSocketEntry, NULL);
    HashMap_RemoveIf(connector->lookup.tasks, FreeTaskEntry, NULL);
    HashMap_RemoveIf(connector->lookup.pending_io, FreePendingIoEntry, NULL);
    HashMap_Free(connector->lookup.sockets);
    HashMap_Free(connector->lookup.tasks);
    HashMap_Free(connector->lookup.pending_io);
    if (connector->internal.completion_port != NULL)
        CloseHandle(connector->internal.completion_port);
    free(connector->interf.lock);
    WSACleanup();
    free(connector);
    return ERROR_SUCCESS;
}
