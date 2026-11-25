#include "internal.h"
#include <stdlib.h>
#include <string.h>

DWORD TcpConnectorCreateTask(TcpConnectorTaskOption options, const char *task_id,
    TcpConnectorCompletionCallback on_completed, void *userdata)
{
    TcpConnectorLoop *connector = TcpConnectorGetLoop();
    if (connector == NULL)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (task_id == NULL || task_id[0] == '\0') {
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_INVALID_ARGUMENT);
        return TCP_CONNECTOR_INVALID_ARGUMENT;
    }
    if (options.kind != TCP_CONNECTOR_TASK_CONNECT && options.kind != TCP_CONNECTOR_TASK_SEND &&
        options.kind != TCP_CONNECTOR_TASK_RECV) {
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_UNSUPPORTED_TASK);
        return TCP_CONNECTOR_NOT_SUPPORTED;
    }
    if (options.kind == TCP_CONNECTOR_TASK_CONNECT && options.conn_opt.port == 0) {
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_INVALID_ARGUMENT);
        return WSAEINVAL;
    }
    if ((options.kind == TCP_CONNECTOR_TASK_SEND || options.kind == TCP_CONNECTOR_TASK_RECV) &&
        (options.sr_opt.socket_id == NULL || options.sr_opt.payload == NULL ||
            (options.kind == TCP_CONNECTOR_TASK_SEND && options.sr_opt.payload->len == 0) ||
            (options.kind == TCP_CONNECTOR_TASK_RECV && options.sr_opt.payload->cap == 0))) {
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_INVALID_ARGUMENT);
        return WSAEINVAL;
    }

    TcpConnectorTask *task = TcpConnectorTaskNew(task_id, options);
    if (task == NULL) {
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_OUT_OF_MEMORY);
        return TCP_CONNECTOR_OUT_OF_MEMORY;
    }
    task->on_completed = on_completed;
    task->userdata = userdata;

    TcpConnectorLockStateLock(connector);
    if (connector->control.closing) {
        TcpConnectorUnlockStateLock(connector);
        TcpConnectorTaskDestroy(task);
        return TCP_CONNECTOR_OPERATION_ABORTED;
    }
    if (TcpConnectorFindTask(connector, task_id) != NULL) {
        TcpConnectorUnlockStateLock(connector);
        TcpConnectorTaskDestroy(task);
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_DUPLICATE_TASK_ID);
        return TCP_CONNECTOR_ALREADY_EXISTS;
    }

    while (atomic_load(&connector->control.running) >= connector->config.max_pending &&
        !connector->control.closing) {
        if (connector->control.io_completion_worker != NULL &&
            GetCurrentThreadId() == GetThreadId(connector->control.io_completion_worker)) {
            TcpConnectorUnlockStateLock(connector);
            DWORD completion_error = TcpConnectorHandleCompletion(connector, INFINITE);
            TcpConnectorLockStateLock(connector);
            if (completion_error != TCP_CONNECTOR_SUCCESS) {
                TcpConnectorUnlockStateLock(connector);
                TcpConnectorTaskDestroy(task);
                TcpConnectorSetError(connector, completion_error);
                return completion_error;
            }
            continue;
        }

        ++connector->control.blocked_creators;
        BOOL woke = TcpConnectorWaitSlotAvailableSignal(connector);
        --connector->control.blocked_creators;
        if (connector->control.blocked_creators == 0)
            TcpConnectorSignalAllSubmittersDone(connector);
        if (!woke) {
            DWORD wait_error = GetLastError();
            TcpConnectorUnlockStateLock(connector);
            TcpConnectorTaskDestroy(task);
            return wait_error;
        }
    }
    if (connector->control.closing) {
        TcpConnectorUnlockStateLock(connector);
        TcpConnectorTaskDestroy(task);
        return TCP_CONNECTOR_OPERATION_ABORTED;
    }
    if (TcpConnectorFindTask(connector, task_id) != NULL) {
        TcpConnectorUnlockStateLock(connector);
        TcpConnectorTaskDestroy(task);
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_DUPLICATE_TASK_ID);
        return TCP_CONNECTOR_ALREADY_EXISTS;
    }
    if (!HashMap_Set(connector->lookup.tasks, task->id, TcpConnectorPointerValue(task))) {
        TcpConnectorUnlockStateLock(connector);
        TcpConnectorTaskDestroy(task);
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_OUT_OF_MEMORY);
        return TCP_CONNECTOR_OUT_OF_MEMORY;
    }

    TcpConnectorSetError(connector, TCP_CONNECTOR_SUCCESS);
    TcpConnectorStartTask(connector, task);
    TcpConnectorUnlockStateLock(connector);
    return TCP_CONNECTOR_SUCCESS;
}
