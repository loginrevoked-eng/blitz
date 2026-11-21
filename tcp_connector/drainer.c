#include "models.h"

typedef struct { TcpConnectorLoop *connector; TcpConnectorTask *task; } ReadyCallbackContext;

static int FindReadyCallback(const char *key, const Value *value, void *context)
{
    (void)key;
    TcpConnectorTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    ReadyCallbackContext *ready = context;
    if (ready->task != NULL || task == NULL || task->on_completed == NULL ||
        task->callback_reported ||
        (task->status != TCP_CONNECTOR_TASK_COMPLETED &&
            task->status != TCP_CONNECTOR_TASK_CANCELED))
        return 0;

    TcpConnectorPendingIo *io = FindPendingIo(ready->connector, task->id);
    if (io != NULL && io->in_flight)
        return 0;
    task->callback_reported = TRUE;
    task->callback_running = TRUE;
    ready->task = task;
    return 0;
}

DWORD WINAPI TcpConnectorDrainerThread(LPVOID argument)
{
    (void)argument;
    TcpConnectorLoop *loop = TcpConnectorGetLoop();
    if (loop == NULL)
        return ERROR_INVALID_STATE;
    for (;;) {
        AcquireSRWLockExclusive(loop->interf.lock);
        ReadyCallbackContext ready = { loop, NULL };
        HashMap_RemoveIf(loop->lookup.tasks, FindReadyCallback, &ready);
        if (ready.task != NULL) {
            TcpConnectorCompletionCallback callback = ready.task->on_completed;
            ReleaseSRWLockExclusive(loop->interf.lock);
            callback(ready.task);
            AcquireSRWLockExclusive(loop->interf.lock);
            ready.task->callback_running = FALSE;
            WakeAllConditionVariable(&loop->interf.drainer_wake);
            ReleaseSRWLockExclusive(loop->interf.lock);
            continue;
        }
        if (!atomic_load(&loop->interf.drainer_running)) {
            ReleaseSRWLockExclusive(loop->interf.lock);
            return 0;
        }
        BOOL has_pending_io = atomic_load(&loop->interf.running) != 0;
        if (!has_pending_io) {
            SleepConditionVariableSRW(&loop->interf.drainer_wake,
                loop->interf.lock, INFINITE, 0);
            ReleaseSRWLockExclusive(loop->interf.lock);
            continue;
        }
        ReleaseSRWLockExclusive(loop->interf.lock);

        DWORD error = ProcessCompletion(loop, WaitMilliseconds(loop));
        if (error != ERROR_SUCCESS)
            SetConnectorError(loop, TCP_CONNECTOR_ERROR_IO);
    }
}
