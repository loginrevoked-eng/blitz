#include "internal.h"
void TcpConnectorTaskReadySubmitCallback(TcpConnectorLoop *connector, TcpConnectorTask *task)
{
    if (connector == NULL || task == NULL || task->on_completed == NULL ||
        task->callback_queued || task->callback_reported)
        return;
    TcpConnectorPendingIo *io = TcpConnectorFindPendingIo(connector, task->id);
    if (io != NULL && io->in_flight)
        return;

    TcpConnectorLockCallbackLock(connector);
    if (connector->control.callback_stopping)
    {
        TcpConnectorUnlockCallbackLock(connector);
        return;
    }
    TaskCallback callback = { .cb = task->on_completed, .arg = task };
    if (!TcpConnectorTaskCallbackQueuePush(&connector->control.callback_queue, callback)) {
        TcpConnectorUnlockCallbackLock(connector);
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_OUT_OF_MEMORY);
        return;
    }
    task->callback_queued = TRUE;
    TcpConnectorSignalCallbackAvailable(connector);
    TcpConnectorUnlockCallbackLock(connector);
}

BOOL TcpConnectorTakeCallback(TcpConnectorLoop *connector, TaskCallback *callback)
{
    TcpConnectorLockCallbackLock(connector);
    while (connector->control.callback_queue.len == 0 &&
        !connector->control.callback_stopping &&
        !atomic_load(&connector->control.callback_runner_stop)) {
        if (!TcpConnectorWaitCallbackAvailableSignal(connector)) {
            TcpConnectorSetError(connector, GetLastError());
            TcpConnectorUnlockCallbackLock(connector);
            return FALSE;
        }
    }

    if (!TcpConnectorTaskCallbackQueuePop(&connector->control.callback_queue, callback)) {
        TcpConnectorUnlockCallbackLock(connector);
        return FALSE;
    }
    TcpConnectorUnlockCallbackLock(connector);
    return TRUE;
}
