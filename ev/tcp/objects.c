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

