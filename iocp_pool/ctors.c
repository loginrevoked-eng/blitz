#include "ctors.h"

#include <stdlib.h>
#include <string.h>

ByteArray *IocpByteArrayNew(uint32_t capacity)
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

void IocpByteArrayDestroy(ByteArray *array)
{
    if (array == NULL)
        return;
    free(array->buffer);
    free(array);
}

IocpTask *IocpTaskNew(const char *task_id, IocpTaskOption options)
{
    if (task_id == NULL || task_id[0] == '\0')
        return NULL;
    IocpTask *task = calloc(1, sizeof(*task));
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
    if (options.kind == IOCP_TASK_SEND || options.kind == IOCP_TASK_RECV) {
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
    task->status = IOCP_TASK_QUEUED;
    task->error = IOCP_POOL_SUCCESS;
    return task;
}

void IocpTaskDestroy(IocpTask *task)
{
    if (task == NULL)
        return;
    if (task->options.kind == IOCP_TASK_SEND ||
        task->options.kind == IOCP_TASK_RECV)
        free(task->options.sr_opt.socket_id);
    free(task->id);
    free(task);
}

IocpOperation *IocpOperationNew(const char *operation_id)
{
    if (operation_id == NULL || operation_id[0] == '\0')
        return NULL;
    IocpOperation *operation = calloc(1, sizeof(*operation));
    if (operation == NULL)
        return NULL;
    size_t length = strlen(operation_id) + 1;
    operation->id = malloc(length);
    if (operation->id == NULL) {
        free(operation);
        return NULL;
    }
    memcpy(operation->id, operation_id, length);
    IocpTaskListInit(&operation->tasks);
    operation->status = IOCP_OPERATION_QUEUED;
    operation->error = IOCP_POOL_SUCCESS;
    return operation;
}

void IocpOperationDestroy(IocpOperation *operation)
{
    if (operation == NULL)
        return;
    free(operation->id);
    IocpTaskListDestroy(&operation->tasks);
    free(operation);
}
