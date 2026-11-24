#include "models.h"
#include <stdlib.h>

DWORD TcpConnectorClearEventloop(void)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;
    if (connector->control.io_completion_worker != NULL &&
        GetCurrentThreadId() == GetThreadId(connector->control.io_completion_worker))
        return TCP_CONNECTOR_BUSY;
    for (uint32_t i = 0; i < connector->control.callback_thread_count; ++i) {
        if (GetCurrentThreadId() == GetThreadId(connector->control.callback_threads[i]))
            return TCP_CONNECTOR_BUSY;
    }

    AcquireSRWLockExclusive(connector->control.lock);
    connector->control.closing = TRUE;
    WakeAllConditionVariable(&connector->control.task_finished);
    WakeAllConditionVariable(&connector->control.slot_available);
    while (connector->control.blocked_creators != 0)
        SleepConditionVariableSRW(&connector->control.submitters_done,
            connector->control.lock, INFINITE, 0);
    HashMapIter iter = { 0 };
    for (Value *value = HashMap_IterNext(connector->lookup.tasks, &iter);
         value != NULL; value = HashMap_IterNext(connector->lookup.tasks, &iter)) {
        TcpConnectorTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
        if (task == NULL || task->status == TCP_CONNECTOR_TASK_COMPLETED ||
            task->status == TCP_CONNECTOR_TASK_CANCELED)
            continue;
        TcpConnectorPendingIo *io = TcpConnectorFindPendingIo(connector, HashMap_IterKey(&iter));
        task->status = TCP_CONNECTOR_TASK_CANCELED;
        task->WSA_error = WSA_OPERATION_ABORTED;
        if (io == NULL || !io->in_flight) {
            RemovePendingIo(connector, io);
            TcpConnectorTaskReadySubmitCallback(connector, task);
        } else {
            io->cancel_requested = TRUE;
            if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT)
                CloseSocket(io->socket);
            else if (io->socket != NULL)
                CancelIoEx((HANDLE)io->socket->handle, &io->overlapped);
        }
    }
    ReleaseSRWLockExclusive(connector->control.lock);
    TcpConnectorDetachLoop(connector);

    if (connector->control.io_completion_worker != NULL) {
        AcquireSRWLockExclusive(connector->control.lock);
        while (atomic_load(&connector->control.running) != 0)
            SleepConditionVariableSRW(&connector->control.task_finished,
                connector->control.lock, INFINITE, 0);
        ReleaseSRWLockExclusive(connector->control.lock);
    } else {
        while (atomic_load(&connector->control.running) != 0)
            TcpConnectorHandleCompletion(connector, INFINITE);
    }

    if (connector->control.io_completion_worker != NULL) {
        AcquireSRWLockExclusive(connector->control.lock);
        atomic_store(&connector->control.io_completion_worker_stop, TRUE);
        PostQueuedCompletionStatus(connector->internal.completion_port, 0,
            TCP_CONNECTOR_IO_COMPLETION_WORKER_WAKE_KEY, NULL);
        ReleaseSRWLockExclusive(connector->control.lock);
        WaitForSingleObject(connector->control.io_completion_worker, INFINITE);
        CloseHandle(connector->control.io_completion_worker);
    }

    AcquireSRWLockExclusive(connector->control.lock);
    connector->control.callback_stopping = TRUE;
    WakeAllConditionVariable(&connector->control.callback_available);
    ReleaseSRWLockExclusive(connector->control.lock);
    for (uint32_t i = 0; i < connector->control.callback_thread_count; ++i) {
        WaitForSingleObject(connector->control.callback_threads[i], INFINITE);
        CloseHandle(connector->control.callback_threads[i]);
    }
    atomic_store(&connector->control.callback_runner_stop, TRUE);

    iter = (HashMapIter){ 0 };
    for (Value *value = HashMap_IterNext(connector->lookup.sockets, &iter);
         value != NULL; value = HashMap_IterNext(connector->lookup.sockets, &iter)) {
        TcpConnectorSocket *socket = value->kind == VALUE_POINTER ? value->ptr : NULL;
        if (socket != NULL) { CloseSocket(socket); free(socket); }
    }
    iter = (HashMapIter){ 0 };
    for (Value *value = HashMap_IterNext(connector->lookup.tasks, &iter);
         value != NULL; value = HashMap_IterNext(connector->lookup.tasks, &iter))
        TcpConnectorTaskDestroy(value->kind == VALUE_POINTER ? value->ptr : NULL);
    iter = (HashMapIter){ 0 };
    for (Value *value = HashMap_IterNext(connector->lookup.pending_io, &iter);
         value != NULL; value = HashMap_IterNext(connector->lookup.pending_io, &iter))
        free(value->kind == VALUE_POINTER ? value->ptr : NULL);
    HashMap_Free(connector->lookup.sockets);
    HashMap_Free(connector->lookup.tasks);
    HashMap_Free(connector->lookup.pending_io);
    if (connector->internal.completion_port != NULL)
        CloseHandle(connector->internal.completion_port);
    free(connector->control.callback_threads);
    free(connector->control.lock);
    WSACleanup();
    free(connector);
    return TCP_CONNECTOR_SUCCESS;
}
