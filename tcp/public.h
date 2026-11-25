#ifndef TCP_CONNECTOR_PUBLIC_H
#define TCP_CONNECTOR_PUBLIC_H

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <stdint.h>
#include "../hashset/hashset.h"

typedef enum {
    TCP_CONNECTOR_SUCCESS = 0,
    TCP_CONNECTOR_EVENT_LOOP_NO_INITIALIZED = ERROR_INVALID_STATE,
    TCP_CONNECTOR_INVALID_ARGUMENT = ERROR_INVALID_PARAMETER,
    TCP_CONNECTOR_NOT_SUPPORTED = ERROR_NOT_SUPPORTED,
    TCP_CONNECTOR_OUT_OF_MEMORY = ERROR_NOT_ENOUGH_MEMORY,
    TCP_CONNECTOR_OPERATION_ABORTED = ERROR_OPERATION_ABORTED,
    TCP_CONNECTOR_ALREADY_EXISTS = ERROR_ALREADY_EXISTS,
    TCP_CONNECTOR_NOT_FOUND = ERROR_NOT_FOUND,
    TCP_CONNECTOR_BUSY = ERROR_BUSY,
    TCP_CONNECTOR_IO_PENDING = ERROR_IO_PENDING,
    TCP_CONNECTOR_INVALID_DATA = ERROR_INVALID_DATA,
    TCP_CONNECTOR_TASK_PENDING = 0x10000,
    TCP_CONNECTOR_TASK_CANCELED,
    TCP_CONNECTOR_TASK_COMPLETED,
    TCP_CONNECTOR_ERROR_INVALID_ARGUMENT = 0x20000,
    TCP_CONNECTOR_ERROR_DUPLICATE_TASK_ID,
    TCP_CONNECTOR_ERROR_TASK_NOT_FOUND,
    TCP_CONNECTOR_ERROR_UNSUPPORTED_TASK,
    TCP_CONNECTOR_ERROR_LIMIT_REACHED,
    TCP_CONNECTOR_ERROR_OUT_OF_MEMORY,
    TCP_CONNECTOR_ERROR_WINSOCK,
    TCP_CONNECTOR_ERROR_COMPLETION_PORT,
    TCP_CONNECTOR_ERROR_IO,
    TCP_CONNECTOR_ERROR_TIMEOUT,
    TCP_CONNECTOR_ERROR_CANCELED,
    TCP_CONNECTOR_ERROR_NO_PENDING
} TcpConnectorStatus;

typedef struct {
    uint8_t *buffer;
    uint32_t len;
    uint32_t cap;
} ByteArray;

typedef enum {
    TCP_CONNECTOR_TASK_SEND,
    TCP_CONNECTOR_TASK_RECV,
    TCP_CONNECTOR_TASK_CONNECT
} TcpConnectorTaskOptionKind;

typedef struct {
    uint32_t addr;
    uint32_t timeout_ms;
    uint32_t retries;
    uint16_t port;
    ByteArray *response;
} TcpConnectorConnectOption;

typedef struct {
    TcpConnectorTaskOptionKind kind;
    ByteArray *payload;
    char *socket_id;
    uint32_t timeout_ms;
} TcpConnectorSROption;

typedef struct {
    TcpConnectorTaskOptionKind kind;
    union {
        TcpConnectorSROption sr_opt;
        TcpConnectorConnectOption conn_opt;
    };
} TcpConnectorTaskOption;

typedef struct TcpConnectorTask TcpConnectorTask;
typedef struct TcpConnectorLoop TcpConnector;
typedef BOOL (*TcpConnectorCompletionCallback)(TcpConnectorTask *task);

struct TcpConnectorTask {
    char *id;
    TcpConnectorTaskOption options;
    TcpConnectorStatus status;
    DWORD WSA_error;
    TcpConnectorCompletionCallback on_completed;
    BOOL callback_reported;
    BOOL callback_running;
    BOOL callback_queued;
    void *userdata;
};

#define TCP_CONNECTOR_WITH_IO_COMPLETION_WORKER 0x1u

DWORD TcpConnectorInitEventloop(uint32_t max_pending, uint32_t flags,
    uint32_t callback_queue_initial_capacity);
TcpConnector *TcpConnectorGet(void);
char *TcpConnectorTasksWaitAny(TcpConnector *connector, char *const task_ids[],
    int task_id_count, HashSet *task_id_cache);
void TcpConnectorTaskInit(TcpConnectorTask *task);
/* Duplicates owned IDs; payload, callback, and userdata pointers remain borrowed. */
TcpConnectorTask *TcpConnectorTaskDup(const TcpConnectorTask *task);
/* Frees a heap-owned task copy returned by TcpConnectorTaskDup. */
void TcpConnectorTaskDestroy(TcpConnectorTask *task);
BOOL TcpConnectorTaskSetId(TcpConnectorTask *task, const char *task_id);
BOOL TcpConnectorTaskSetKind(TcpConnectorTask *task, TcpConnectorTaskOptionKind kind);
BOOL TcpConnectorTaskSetAddr(TcpConnectorTask *task, const char *ipv4_address);
BOOL TcpConnectorTaskSetPort(TcpConnectorTask *task, uint16_t port);
BOOL TcpConnectorTaskSetTimeout(TcpConnectorTask *task, uint32_t timeout_ms);
BOOL TcpConnectorTaskSetRetries(TcpConnectorTask *task, uint32_t retries);
BOOL TcpConnectorTaskSetSocketId(TcpConnectorTask *task, const char *socket_id);
BOOL TcpConnectorTaskSetPayload(TcpConnectorTask *task, ByteArray *payload);
void TcpConnectorTaskSetCallback(TcpConnectorTask *task,
    TcpConnectorCompletionCallback callback, void *userdata);
DWORD TcpConnectorClearEventloop(void);
DWORD TcpConnectorCreateTask(TcpConnectorTaskOption options, const char *task_id, TcpConnectorCompletionCallback callback, void *userdata);
DWORD TcpConnectorGetTask(const char *task_id, TcpConnectorTask *task);
DWORD TcpConnectorCancelTask(const char *task_id);
DWORD TcpConnectorAwaitTask(const char *task_id);
DWORD TcpConnectorCloseSocketById(char *socket_id);

#endif
