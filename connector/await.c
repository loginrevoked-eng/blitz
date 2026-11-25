#include "internal.h"
#include "hashset.h"

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
        if (!TcpConnectorWaitTaskFinishedSignal(loop))
            return GetLastError();
    }
    return TCP_CONNECTOR_SUCCESS;
}

char *TcpConnectorTasksWaitAny(TcpConnector *connector, char *const task_ids[],
    int task_id_count, HashSet *task_id_cache)
{
    if (connector == NULL) {
        return NULL;
    }
    if (task_id_count < 0 || (task_id_count > 0 && task_ids == NULL) ||
        (task_id_cache == NULL && task_id_count == 0)) {
        TcpConnectorSetError(connector, TCP_CONNECTOR_INVALID_ARGUMENT);
        return NULL;
    }

    HashSet *local_task_ids = NULL;
    HashSet *watched_task_ids = task_id_cache;
    if (watched_task_ids == NULL) {
        local_task_ids = HashSet_New((size_t)task_id_count);
        if (local_task_ids == NULL) {
            TcpConnectorSetError(connector, TCP_CONNECTOR_OUT_OF_MEMORY);
            return NULL;
        }
        for (int i = 0; i < task_id_count; ++i) {
            if (task_ids[i] == NULL) {
                HashSet_Free(local_task_ids);
                TcpConnectorSetError(connector, TCP_CONNECTOR_INVALID_ARGUMENT);
                return NULL;
            }
            if (HashSet_ContainsString(local_task_ids, task_ids[i]))
                continue;
            if (!HashSet_PutString(local_task_ids, task_ids[i])) {
                HashSet_Free(local_task_ids);
                TcpConnectorSetError(connector, TCP_CONNECTOR_OUT_OF_MEMORY);
                return NULL;
            }
        }
        watched_task_ids = local_task_ids;
    } else if (task_id_count > 0) {
        BOOL has_cached_ids = FALSE;
        for (int i = 0; i < task_id_count; ++i) {
            if (task_ids[i] == NULL) {
                TcpConnectorSetError(connector, TCP_CONNECTOR_INVALID_ARGUMENT);
                return NULL;
            }
            if (HashSet_ContainsString(watched_task_ids, task_ids[i]))
                has_cached_ids = TRUE;
        }
        if (!has_cached_ids) {
            for (int i = 0; i < task_id_count; ++i) {
                if (HashSet_ContainsString(watched_task_ids, task_ids[i]))
                    continue;
                if (!HashSet_PutString(watched_task_ids, task_ids[i])) {
                    TcpConnectorSetError(connector, TCP_CONNECTOR_OUT_OF_MEMORY);
                    return NULL;
                }
            }
        }
    }

    TcpConnectorLockStateLock(connector);
    for (;;) {
        HashMapIter iter = { 0 };
        while (HashMap_IterNext(connector->lookup.tasks, &iter) != NULL) {
            const char *task_id = HashMap_IterKey(&iter);
            if (!HashSet_ContainsString(watched_task_ids, task_id))
                continue;
            TcpConnectorTask *task = TcpConnectorFindTask(connector, task_id);
            if (task != NULL && TcpConnectorTaskHasSettled(connector, task)) {
                char *completed_task_id = task->id;
                HashSet_RemoveString(watched_task_ids, task_id);
                TcpConnectorUnlockStateLock(connector);
                HashSet_Free(local_task_ids);
                return completed_task_id;
            }
        }
        if (!TcpConnectorWaitTaskFinishedSignal(connector)) {
            DWORD error = GetLastError();
            TcpConnectorUnlockStateLock(connector);
            HashSet_Free(local_task_ids);
            TcpConnectorSetError(connector, error);
            return NULL;
        }
    }
}

DWORD TcpConnectorAwaitTask(const char *task_id)
{
    TcpConnectorLoop *loop = TcpConnectorGetLoop();
    if (!loop)
        return TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED;

    if (!task_id)
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    TcpConnectorLockStateLock(loop);
    TcpConnectorTask *task = TcpConnectorFindTask(loop, task_id);
    if (!task)
    {
        TcpConnectorUnlockStateLock(loop);
        TcpConnectorSetError(loop, TCP_CONNECTOR_ERROR_TASK_NOT_FOUND);
        return TCP_CONNECTOR_ERROR_TASK_NOT_FOUND;
    }
    DWORD wait_error = TcpConnectorAwaitTaskState(loop, task);
    if (wait_error != TCP_CONNECTOR_SUCCESS)
    {
        TcpConnectorUnlockStateLock(loop);
        return wait_error;
    }
    DWORD error = task->WSA_error;
    TcpConnectorUnlockStateLock(loop);
    return error;
}
