#include "models.h"
#include <stdlib.h>

static BOOL TcpConnectorRunQueuedCallback(TcpConnectorLoop *loop)
{
    AcquireSRWLockExclusive(loop->control.lock);
    TaskCallback *callback = TcpConnectorTakeCallback(loop);
    ReleaseSRWLockExclusive(loop->control.lock);
    if (callback == NULL)
        return FALSE;

    TcpConnectorTask *task = callback->arg;
    AcquireSRWLockExclusive(loop->control.lock);
    if(callback->arg->callback_running)
    {
        ReleaseSRWLockExclusive(loop->control.lock);
        free(callback);
        return TRUE;
    }
    task->callback_running = TRUE;
    ReleaseSRWLockExclusive(loop->control.lock);

    BOOL cb_success = callback->cb(task);
    free(callback);

    AcquireSRWLockExclusive(loop->control.lock);
    task->callback_running = FALSE;
    WakeAllConditionVariable(&loop->control.task_finished);
    ReleaseSRWLockExclusive(loop->control.lock);
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
