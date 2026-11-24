#include "models.h"
#include <stdlib.h>
#include <string.h>

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
            memcpy(task->options.sr_opt.socket_id,
                options.sr_opt.socket_id, socket_id_length);
        }
    }
    task->status = TCP_CONNECTOR_TASK_PENDING;
    return task;
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

