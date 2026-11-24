#include "models.h"
#include <stdlib.h>
#include <string.h>

TcpConnectorTask *TcpConnectorTaskNew(const char *task_id, TcpConnectorTaskOption options)
{
    if (task_id == NULL || task_id[0] == '\0')
        return NULL;
    TcpConnectorTask *task = calloc(1, sizeof(*task));
    if (task == NULL)
        return NULL;
    size_t id_length = strlen(task_id) + 1;
    task->id = malloc(id_length);
    if (task->id == NULL) {
        free(task);
        return NULL;
    }
    memcpy(task->id, task_id, id_length);
    task->options = options;
    if (options.kind == TCP_CONNECTOR_TASK_SEND || options.kind == TCP_CONNECTOR_TASK_RECV) {
        task->options.sr_opt.kind = options.kind;
        if (options.sr_opt.socket_id != NULL) {
            size_t socket_id_length = strlen(options.sr_opt.socket_id) + 1;
            task->options.sr_opt.socket_id = malloc(socket_id_length);
            if (task->options.sr_opt.socket_id == NULL) {
                free(task->id);
                free(task);
                return NULL;
            }
            memcpy(task->options.sr_opt.socket_id, options.sr_opt.socket_id,
                socket_id_length);
        }
    }
    task->status = TCP_CONNECTOR_TASK_PENDING;
    return task;
}

DWORD TcpConnectorCreateTask(TcpConnectorTaskOption options, const char *task_id, TcpConnectorCompletionCallback on_completed, void *userdata)
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

    AcquireSRWLockExclusive(connector->control.lock);
    if (connector->control.closing) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        return TCP_CONNECTOR_OPERATION_ABORTED;
    }
    if (TcpConnectorFindTask(connector, task_id) != NULL) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_DUPLICATE_TASK_ID);
        return TCP_CONNECTOR_ALREADY_EXISTS;
    }

    while (atomic_load(&connector->control.running) >= connector->config.max_pending &&
        !connector->control.closing) {
        if (connector->control.io_completion_worker != NULL &&
            GetCurrentThreadId() == GetThreadId(connector->control.io_completion_worker)) {
            ReleaseSRWLockExclusive(connector->control.lock);
            DWORD completion_error = TcpConnectorHandleCompletion(connector, INFINITE);
            AcquireSRWLockExclusive(connector->control.lock);
            if (completion_error != TCP_CONNECTOR_SUCCESS) {
                ReleaseSRWLockExclusive(connector->control.lock);
                TcpConnectorTaskDestroy(task);
                TcpConnectorSetError(connector, completion_error);
                return completion_error;
            }
            continue;
        }

        ++connector->control.blocked_creators;
        BOOL woke = SleepConditionVariableSRW(&connector->control.slot_available,
            connector->control.lock, INFINITE, 0);
        --connector->control.blocked_creators;
        if (connector->control.blocked_creators == 0)
            WakeAllConditionVariable(&connector->control.submitters_done);
        if (!woke) {
            DWORD wait_error = GetLastError();
            ReleaseSRWLockExclusive(connector->control.lock);
            TcpConnectorTaskDestroy(task);
            return wait_error;
        }
    }
    if (connector->control.closing) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        return TCP_CONNECTOR_OPERATION_ABORTED;
    }
    if (TcpConnectorFindTask(connector, task_id) != NULL) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_DUPLICATE_TASK_ID);
        return TCP_CONNECTOR_ALREADY_EXISTS;
    }
    if (!HashMap_Set(connector->lookup.tasks, task->id, TcpConnectorPointerValue(task))) {
        ReleaseSRWLockExclusive(connector->control.lock);
        TcpConnectorTaskDestroy(task);
        TcpConnectorSetError(connector, TCP_CONNECTOR_ERROR_OUT_OF_MEMORY);
        return TCP_CONNECTOR_OUT_OF_MEMORY;
    }

    TcpConnectorSetError(connector, TCP_CONNECTOR_SUCCESS);
    TcpConnectorStartTask(connector, task);
    ReleaseSRWLockExclusive(connector->control.lock);
    return TCP_CONNECTOR_SUCCESS;
}
