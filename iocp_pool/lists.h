#ifndef IOCP_POOL_LISTS_H
#define IOCP_POOL_LISTS_H

#include <stdint.h>

typedef struct IocpTask IocpTask;

typedef struct {
    IocpTask **tasks;
    uint32_t len;
    uint32_t cap;
} IocpTaskList;

void IocpTaskListInit(IocpTaskList *list);
void IocpTaskListDestroy(IocpTaskList *list);
uint32_t IocpTaskListLength(const IocpTaskList *list);
IocpTask *IocpTaskListGet(const IocpTaskList *list, uint32_t index);
int IocpTaskListSet(IocpTaskList *list, uint32_t index, IocpTask *task);
int IocpTaskListAppend(IocpTaskList *list, IocpTask *task);

#endif
