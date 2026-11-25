#include "internal.h"
#include <stdlib.h>
#include <string.h>

BOOL TcpConnectorTaskCallbackQueueInit(TaskCallbackQueue *queue, uint32_t initial_capacity)
{
    if (queue == NULL || initial_capacity == 0)
        return FALSE;
#if SIZE_MAX <= UINT32_MAX
    if (initial_capacity > SIZE_MAX / sizeof(*queue->cbs))
        return FALSE;
#endif

    queue->cbs = calloc(initial_capacity, sizeof(*queue->cbs));
    if (queue->cbs == NULL)
        return FALSE;
    queue->len = 0;
    queue->cap = initial_capacity;
    return TRUE;
}

BOOL TcpConnectorTaskCallbackQueuePush(TaskCallbackQueue *queue, TaskCallback callback)
{
    if (queue == NULL || queue->len == UINT32_MAX)
        return FALSE;

    if (queue->len == queue->cap) {
        uint32_t new_capacity = queue->cap == 0 ? 8 :
            queue->cap > UINT32_MAX / 2 ? UINT32_MAX : queue->cap * 2;
        if (new_capacity <= queue->cap)
            return FALSE;
#if SIZE_MAX <= UINT32_MAX
        if (new_capacity > SIZE_MAX / sizeof(*queue->cbs))
            return FALSE;
#endif

        TaskCallback *callbacks = realloc(queue->cbs,
            (size_t)new_capacity * sizeof(*queue->cbs));
        if (callbacks == NULL)
            return FALSE;
        queue->cbs = callbacks;
        queue->cap = new_capacity;
    }

    queue->cbs[queue->len++] = callback;
    return TRUE;
}

BOOL TcpConnectorTaskCallbackQueuePop(TaskCallbackQueue *queue, TaskCallback *callback)
{
    if (queue == NULL || callback == NULL || queue->len == 0)
        return FALSE;

    *callback = queue->cbs[0];
    --queue->len;
    if (queue->len != 0)
        memmove(queue->cbs, queue->cbs + 1,
            (size_t)queue->len * sizeof(*queue->cbs));
    queue->cbs[queue->len] = (TaskCallback){ 0 };
    return TRUE;
}

void TcpConnectorTaskCallbackQueueDestroy(TaskCallbackQueue *queue)
{
    if (queue == NULL)
        return;
    free(queue->cbs);
    *queue = (TaskCallbackQueue){ 0 };
}

ByteArray *TcpConnectorByteArrayNew(uint32_t capacity)
{
    ByteArray *array = calloc(1, sizeof(*array));
    if (array == NULL)
        return NULL;
    if (capacity != 0) {
        array->buffer = malloc(capacity);
        if (array->buffer == NULL) {
            free(array);
            return NULL;
        }
    }
    array->cap = capacity;
    return array;
}

void TcpConnectorByteArrayDestroy(ByteArray *array)
{
    if (array == NULL)
        return;
    free(array->buffer);
    free(array);
}

void TcpConnectorTaskDestroy(TcpConnectorTask *task)
{
    if (task == NULL)
        return;
    if (task->options.kind == TCP_CONNECTOR_TASK_SEND ||
        task->options.kind == TCP_CONNECTOR_TASK_RECV)
        free(task->options.sr_opt.socket_id);
    free(task->id);
    free(task);
}

TcpConnectorTask *TcpConnectorTaskDup(const TcpConnectorTask *task)
{
    if (task == NULL)
        return NULL;

    TcpConnectorTask *copy = malloc(sizeof(*copy));
    if (copy == NULL)
        return NULL;
    *copy = *task;
    copy->id = NULL;
    if (task->options.kind == TCP_CONNECTOR_TASK_SEND ||
        task->options.kind == TCP_CONNECTOR_TASK_RECV)
        copy->options.sr_opt.socket_id = NULL;

    if (task->id != NULL) {
        size_t id_length = strlen(task->id) + 1;
        copy->id = malloc(id_length);
        if (copy->id == NULL) {
            TcpConnectorTaskDestroy(copy);
            return NULL;
        }
        memcpy(copy->id, task->id, id_length);
    }

    if (task->options.kind == TCP_CONNECTOR_TASK_SEND ||
        task->options.kind == TCP_CONNECTOR_TASK_RECV) {
        if (task->options.sr_opt.socket_id != NULL) {
            size_t socket_id_length = strlen(task->options.sr_opt.socket_id) + 1;
            copy->options.sr_opt.socket_id = malloc(socket_id_length);
            if (copy->options.sr_opt.socket_id == NULL) {
                TcpConnectorTaskDestroy(copy);
                return NULL;
            }
            memcpy(copy->options.sr_opt.socket_id, task->options.sr_opt.socket_id,
                socket_id_length);
        }
    }
    return copy;
}

