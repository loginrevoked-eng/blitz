#ifndef IOCP_POOL_H
#define IOCP_POOL_H

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <stdatomic.h>
#include <synchapi.h>
#include <stdint.h>
#include "../hashmap/hashmap.h"
#include "lists.h"

typedef enum {
    IOCP_TASK_QUEUED = 1,
    IOCP_TASK_PENDING = 2,
    IOCP_TASK_CANCELED = 3,
    IOCP_TASK_COMPLETED = 4
} IocpTaskStatus;



typedef struct {
    uint8_t *buffer;
    uint32_t len;
    uint32_t cap;
}ByteArray;


typedef enum{
    IOCP_TASK_SEND,
    IOCP_TASK_RECV,
    IOCP_TASK_CONNECT
} IocpTaskOptionKind;


typedef struct {
    uint32_t addr;
    uint32_t timeout_ms;
    uint32_t retries;
    uint16_t port;
    ByteArray *response;
} IocpConnectOption;


typedef struct {
    IocpTaskOptionKind kind;
    ByteArray *payload;
    char *socket_id;
}IocpSROption;

typedef struct {
    IocpTaskOptionKind kind;
    union {
        IocpSROption sr_opt;
        IocpConnectOption conn_opt;
    };
}IocpTaskOption;

typedef enum {
    IOCP_POOL_SUCCESS = 0,
    IOCP_POOL_ERROR_INVALID_ARGUMENT,
    IOCP_POOL_ERROR_DUPLICATE_TASK_ID,
    IOCP_POOL_ERROR_TASK_NOT_FOUND,
    IOCP_POOL_ERROR_UNSUPPORTED_TASK,
    IOCP_POOL_ERROR_LIMIT_REACHED,
    IOCP_POOL_ERROR_OUT_OF_MEMORY,
    IOCP_POOL_ERROR_WINSOCK,
    IOCP_POOL_ERROR_IOCP,
    IOCP_POOL_ERROR_IO,
    IOCP_POOL_ERROR_TIMEOUT,
    IOCP_POOL_ERROR_CANCELED
}IocpPoolError;


typedef struct IocpTask IocpTask;
typedef struct IocpPool IocpPool;
typedef struct IocpOperation IocpOperation;

typedef void (*CompletionCallback)(IocpTask *task, IocpPool *pool);

struct IocpTask {
    char *id;
    IocpTaskOption options;
    IocpTaskStatus status;
    IocpPoolError error;
    DWORD WSA_error;
    CompletionCallback on_completed;
    IocpOperation *operation;
    uint32_t operation_index;
};

typedef enum {
    IOCP_OPERATION_QUEUED = 1,
    IOCP_OPERATION_RUNNING,
    IOCP_OPERATION_COMPLETED,
    IOCP_OPERATION_FAILED,
    IOCP_OPERATION_CANCELED
} IocpOperationStatus;

struct IocpOperation {
    char *id;
    IocpTaskList tasks;
    uint32_t next_task;
    BOOL submitted;
    IocpOperationStatus status;
    IocpPoolError error;
    DWORD WSA_error;
};

struct IocpPool {
    HashMap *tasks;
    HashMap *overlapped;
    HashMap *sockets;
    HashMap *operations;
    uint32_t limit;
    HANDLE completion_port;
    LPFN_CONNECTEX connect_ex;
    SRWLOCK *lock;
    atomic_uint error;
    atomic_uint running;
};




IocpPool *IocpPoolNew(uint32_t limit);
void IocpPoolDestroy(IocpPool *pool);

DWORD IocpPoolCreateTask(IocpPool *pool, IocpTaskOption options,
    const char *task_id);
DWORD IocpPoolGetTask(IocpPool *pool, const char *task_id, IocpTask *task);
DWORD IocpPoolCancelTask(IocpPool *pool, const char *task_id);
DWORD IocpPoolWaitTask(IocpPool *pool, const char *task_id);
DWORD IocpPoolWaitTasks(IocpPool *pool);

DWORD IocpPoolCreateOperation(IocpPool *pool, const char *operation_id);
DWORD IocpPoolOperationAddTask(IocpPool *pool, const char *operation_id,
    IocpTaskOption options, const char *task_id);
DWORD IocpPoolSubmitOperation(IocpPool *pool, const char *operation_id);
DWORD IocpPoolGetOperation(IocpPool *pool, const char *operation_id,
    IocpOperation *operation);
DWORD IocpPoolCancelOperation(IocpPool *pool, const char *operation_id);
DWORD IocpPoolWaitOperation(IocpPool *pool, const char *operation_id);
DWORD IocpPoolWaitOperations(IocpPool *pool);

#endif
