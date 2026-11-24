#include "models.h"

static BOOL TcpConnectorTaskHasSettled(TcpConnectorLoop *loop, TcpConnectorTask *task)
{
    if (task->status == TCP_CONNECTOR_TASK_COMPLETED)
        return TRUE;
    if (task->status != TCP_CONNECTOR_TASK_CANCELED)
        return FALSE;
    TcpConnectorPendingIo *io = TcpConnectorFindPendingIo(loop, task->id);
    return io == NULL || !io->in_flight;
}

static DWORD TcpConnectorAwaitTaskState(TcpConnectorLoop *loop, TcpConnectorTask *task)
{
    while (!TcpConnectorTaskHasSettled(loop, task))
    {
        if (!SleepConditionVariableSRW(&loop->control.task_finished, loop->control.lock, INFINITE, 0))
            return GetLastError();
    }
    return TCP_CONNECTOR_SUCCESS;
}

DWORD TcpConnectorAwaitTask(const char *task_id)
{
    TcpConnectorLoop *loop = TcpConnectorGetLoop();
    if (!loop)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (!task_id)
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    AcquireSRWLockExclusive(loop->control.lock);
    TcpConnectorTask *task = TcpConnectorFindTask(loop, task_id);
    if (!task)
    {
        ReleaseSRWLockExclusive(loop->control.lock);
        SetConnectorError(loop, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
        return TCP_CONNECTOR_ERROR_TASK_NOT_FOUND;
    }
    DWORD wait_error = TcpConnectorAwaitTaskState(loop, task);
    if (wait_error != TCP_CONNECTOR_SUCCESS)
    {
        ReleaseSRWLockExclusive(loop->control.lock);
        return wait_error;
    }
    DWORD error = task->WSA_error;
    ReleaseSRWLockExclusive(loop->control.lock);
    return error;
}
