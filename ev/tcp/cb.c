#include "models.h"
#include <stdlib.h>

DWORD WINAPI TcpConnectorCallbackThread(LPVOID argument)
{
    TcpConnectorLoop *connector = argument;
    for (;;) {
        DWORD wait = WaitForSingleObject(connector->control.callback_semaphore,
            INFINITE);
        if (wait != WAIT_OBJECT_0)
            return GetLastError();

        AcquireSRWLockExclusive(connector->control.lock);
        TaskCallback *callback = TcpConnectorTakeCallback(connector);
        BOOL stopping = connector->control.callback_stopping;
        ReleaseSRWLockExclusive(connector->control.lock);

        if (callback == NULL) {
            if (stopping)
                return 0;
            continue;
        }

        TcpConnectorTask *task = callback->arg;
        callback->cb(task);
        free(callback);

        AcquireSRWLockExclusive(connector->control.lock);
        task->callback_running = FALSE;
        WakeAllConditionVariable(&connector->control.task_finished);
        ReleaseSRWLockExclusive(connector->control.lock);
    }
}
