#ifndef TCP_CONNECTOR_PUBLIC_H
#define TCP_CONNECTOR_PUBLIC_H

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <stdint.h>

typedef enum {
    TCP_CONNECTOR_TASK_QUEUED = 1,
    TCP_CONNECTOR_TASK_PENDING,
    TCP_CONNECTOR_TASK_CANCELED,
    TCP_CONNECTOR_TASK_COMPLETED
} TcpConnectorTaskStatus;

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

typedef enum {
    TCP_CONNECTOR_SUCCESS = 0,
    TCP_CONNECTOR_ERROR_INVALID_ARGUMENT,
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
} TcpConnectorError;

typedef struct TcpConnectorTask TcpConnectorTask;
typedef BOOL (*TcpConnectorCompletionCallback)(TcpConnectorTask *task);

struct TcpConnectorTask {
    char *id;
    TcpConnectorTaskOption options;
    TcpConnectorTaskStatus status;
    TcpConnectorError error;
    DWORD WSA_error;
    TcpConnectorCompletionCallback on_completed;
    BOOL completion_reported;
    BOOL callback_reported;
    BOOL callback_running;
    void *userdata;
};

#define TCP_CONNECTOR_WITH_DRAINER 0x1u


DWORD TcpConnectorInitEventloop(uint32_t max_pending, uint32_t flags);
DWORD TcpConnectorClearEventloop(void);
DWORD TcpConnectorCreateTask(TcpConnectorTaskOption options, const char *task_id,
    TcpConnectorCompletionCallback callback, void *userdata);
DWORD TcpConnectorGetTask(const char *task_id, TcpConnectorTask *task);
DWORD TcpConnectorCancelTask(const char *task_id);
DWORD TcpConnectorWaitTask(const char *task_id);
TcpConnectorError TcpConnectorWaitTasks(const char **task_id);
DWORD TcpConnectorDiscardTask(const char *task_id);
DWORD TcpConnectorCloseSocketById(char *socket_id);

#endif
