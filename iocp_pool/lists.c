#include "lists.h"

#include <stdlib.h>

static int IocpTaskListReserve(IocpTaskList *list, uint32_t capacity)
{
    if (list == NULL)
        return 0;
    if (capacity <= list->cap)
        return 1;

    uint32_t next = list->cap == 0 ? 4 : list->cap;
    while (next < capacity) {
        if (next > UINT32_MAX / 2) {
            next = capacity;
            break;
        }
        next *= 2;
    }
    IocpTask **tasks = realloc(list->tasks, (size_t)next * sizeof(*tasks));
    if (tasks == NULL)
        return 0;
    list->tasks = tasks;
    list->cap = next;
    return 1;
}

void IocpTaskListInit(IocpTaskList *list)
{
    if (list != NULL) {
        list->tasks = NULL;
        list->len = 0;
        list->cap = 0;
    }
}

void IocpTaskListDestroy(IocpTaskList *list)
{
    if (list == NULL)
        return;
    free(list->tasks);
    IocpTaskListInit(list);
}

uint32_t IocpTaskListLength(const IocpTaskList *list)
{
    return list == NULL ? 0 : list->len;
}

IocpTask *IocpTaskListGet(const IocpTaskList *list, uint32_t index)
{
    if (list == NULL || index >= list->len)
        return NULL;
    return list->tasks[index];
}

int IocpTaskListSet(IocpTaskList *list, uint32_t index, IocpTask *task)
{
    if (list == NULL || task == NULL || index >= list->len)
        return 0;
    list->tasks[index] = task;
    return 1;
}

int IocpTaskListAppend(IocpTaskList *list, IocpTask *task)
{
    if (list == NULL || task == NULL || list->len == UINT32_MAX ||
        !IocpTaskListReserve(list, list->len + 1))
        return 0;
    list->tasks[list->len++] = task;
    return 1;
}
