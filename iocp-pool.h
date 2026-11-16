#ifndef IOCP_POOL_H
#define IOCP_POOL_H

#include <winsock2.h>
#include <mswsock.h>
#include <stdint.h>

#include "core/models.h"

typedef enum {
    IOCP_PROBE_TASK_QUEUED,
    IOCP_PROBE_TASK_PENDING,
    IOCP_PROBE_TASK_COMPLETED
} IocpProbeTaskStatus;

typedef struct {
    HostUpCheckProbeOptions probe;
    uint16_t port;
} IocpProbeOption;

typedef struct {
    IocpProbeOption options;
    SOCKET socket;
    OVERLAPPED overlapped;
    IocpProbeTaskStatus status;
    DWORD error;
    ULONGLONG deadline_ms;
    uint32_t attempts_started;
    BOOL timed_out;
} IocpProbeTask;

typedef struct {
    uint32_t pending;
} IocpPoolStatus;

typedef struct {
    /* Keep task addresses stable until their completion is collected. */
    IocpProbeTask *items;
    uint32_t len;
    uint32_t cap;
} IocpProbeTaskList;

typedef struct {
    IocpProbeTaskList tasks;
    HANDLE completion_port;
    IocpPoolStatus status;
    LPFN_CONNECTEX connect_ex;
    uint32_t max_pending;
    BOOL winsock_started;
} IocpPool;

int IocpPoolInit(IocpPool *pool, uint32_t capacity, uint32_t max_pending);
int IocpPoolAdd(IocpPool *pool, IocpProbeOption options);
int IocpPoolRun(IocpPool *pool);
void IocpPoolDestroy(IocpPool *pool);

#endif
