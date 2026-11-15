#ifndef IOCP_POOL_H
#define IOCP_POOL_H

#include <winsock2.h>
#include <stdint.h>

#include "core/models.h"

typedef struct IocpPool IocpPool;

typedef enum {
    IOCP_PROBE_TASK_QUEUED,
    IOCP_PROBE_TASK_PENDING,
    IOCP_PROBE_TASK_COMPLETED
} IocpProbeTaskStatus;

typedef struct {
    HostUpCheckProbeOptions probe;
    uint16_t port;
} IocpProbeOption;

/* A copy of a task's public state. Socket and OVERLAPPED details stay private. */
typedef struct {
    uint64_t id;
    IocpProbeOption options;
    IocpProbeTaskStatus status;
    DWORD error;
} IocpProbeTask;

/* The pool calls WSAStartup on create and WSACleanup on destroy. */
IocpPool *IocpPoolCreate(uint32_t max_pending, uint32_t default_timeout_ms,
    DWORD *error);
void IocpPoolDestroy(IocpPool *pool);

/* Task CRUD. Update and remove are allowed only while a task is not pending. */
int IocpPoolAdd(IocpPool *pool, const IocpProbeOption *options,
    uint64_t *task_id, DWORD *error);
int IocpPoolGet(const IocpPool *pool, uint64_t task_id, IocpProbeTask *task);
int IocpPoolUpdate(IocpPool *pool, uint64_t task_id,
    const IocpProbeOption *options, DWORD *error);
int IocpPoolRemove(IocpPool *pool, uint64_t task_id, DWORD *error);

/* Runs all queued tasks and returns when each has completed or timed out. */
int IocpPoolRun(IocpPool *pool, DWORD *error);

#endif
