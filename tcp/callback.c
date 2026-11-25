#include "internal.h"
#include <stdlib.h>

static BOOL TcpConnectorRunQueuedCallback(TcpConnectorLoop *loop)
{
    TaskCallback callback;
    BOOL has_callback = TcpConnectorTakeCallback(loop, &callback);
    if (!has_callback)
        return FALSE;

    TcpConnectorTask *task = callback.arg;
    TcpConnectorLockStateLock(loop);
    task->callback_queued = FALSE;
    task->callback_reported = TRUE;
    if (task->callback_running)
    {
        TcpConnectorUnlockStateLock(loop);
        return TRUE;
    }
    task->callback_running = TRUE;
    TcpConnectorUnlockStateLock(loop);

    BOOL cb_success = callback.cb(task);

    TcpConnectorLockStateLock(loop);
    task->callback_running = FALSE;
    TcpConnectorSignalAllTaskFinished(loop);
    TcpConnectorUnlockStateLock(loop);
    return cb_success;
}

DWORD WINAPI TcpConnectorTaskCallbackRunner(LPVOID argument)
{
    (void)argument;
    TcpConnectorLoop *loop = TcpConnectorGetLoop();
    if (loop)
    {
        while (!atomic_load(&loop->control.callback_runner_stop)) {
            TcpConnectorRunQueuedCallback(loop);
        }
        return TCP_CONNECTOR_SUCCESS;
    }
    return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;
}
