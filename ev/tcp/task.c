#include "models.h"
void TcpConnectorTaskReadySubmitCallback(TcpConnectorLoop *connector, TcpConnectorTask *task)
{
    if (connector == NULL || task == NULL || task->on_completed == NULL ||
        task->callback_queued || task->callback_reported)
        return;
    TcpConnectorPendingIo *io = TcpConnectorFindPendingIo(connector, task->id);
    if (io != NULL && io->in_flight)
        return;
    while (connector->control.callback_queue_length >=
            connector->control.callback_queue_capacity &&
        !connector->control.callback_stopping) {
        if (!SleepConditionVariableSRW(&connector->control.callback_space,
                connector->control.lock, INFINITE, 0)) {
            SetConnectorError(connector, GetLastError());
            return;
        }
    }
    if (connector->control.callback_stopping)
        return;
    TaskCallback *callback = malloc(sizeof(*callback));
    if (callback == NULL) {
        SetConnectorError(connector, TCP_CONNECTOR_ERROR_OUT_OF_MEMORY);
        return;
    }
    *callback = (TaskCallback){ .cb = task->on_completed, .arg = task, .next = NULL };
    task->callback_queued = TRUE;
    if (connector->control.callback_tail != NULL)
        connector->control.callback_tail->next = callback;
    else
        connector->control.callback_head = callback;
    connector->control.callback_tail = callback;
    ++connector->control.callback_queue_length;
    WakeConditionVariable(&connector->control.callback_available);
}

void TcpConnectorReportCompletion(TcpConnectorLoop *connector, TcpConnectorTask *task)
{
    if (connector == NULL || task == NULL || task->completion_queued ||
        task->completion_reported)
        return;
    TcpConnectorPendingIo *io = TcpConnectorFindPendingIo(connector, task->id);
    if (io != NULL && io->in_flight)
        return;
    task->completion_queued = TRUE;
    task->completion_next = NULL;
    if (connector->control.completion_tail != NULL)
        connector->control.completion_tail->completion_next = task;
    else
        connector->control.completion_head = task;
    connector->control.completion_tail = task;
    WakeAllConditionVariable(&connector->control.task_finished);
}

TaskCallback *TcpConnectorTakeCallback(TcpConnectorLoop *connector)
{
    while (connector->control.callback_head == NULL &&
        !connector->control.callback_stopping &&
        !atomic_load(&connector->control.callback_runner_stop)) {
        if (!SleepConditionVariableSRW(&connector->control.callback_available,
                connector->control.lock, INFINITE, 0)) {
            SetConnectorError(connector, GetLastError());
            return NULL;
        }
    }

    TaskCallback *callback = connector->control.callback_head;
    if (callback == NULL)
        return NULL;
    connector->control.callback_head = callback->next;
    if (connector->control.callback_head == NULL)
        connector->control.callback_tail = NULL;
    --connector->control.callback_queue_length;
    callback->next = NULL;
    TcpConnectorTask *task = callback->arg;
    task->callback_queued = FALSE;
    task->callback_reported = TRUE;
    WakeConditionVariable(&connector->control.callback_space);
    return callback;
}

