#ifndef IOCP_POOL_CTORS_H
#define IOCP_POOL_CTORS_H

#include "pool.h"

ByteArray *IocpByteArrayNew(uint32_t capacity);
void IocpByteArrayDestroy(ByteArray *array);
IocpTask *IocpTaskNew(const char *task_id, IocpTaskOption options);
void IocpTaskDestroy(IocpTask *task);
IocpOperation *IocpOperationNew(const char *operation_id);
void IocpOperationDestroy(IocpOperation *operation);

#endif
