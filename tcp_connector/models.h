#ifndef TCP_CONNECTOR_MODELS_H
#define TCP_CONNECTOR_MODELS_H

#include "public.h"
#include <stdatomic.h>
#include "../hashmap/hashmap.h"
#include "../value/value.h"

#define TCP_CONNECTOR_DRAINER_WAKE_KEY ((ULONG_PTR)~(ULONG_PTR)0)

typedef struct TcpConnectorLoop TcpConnectorLoop;

typedef struct { uint32_t max_pending; uint32_t flags; } TcpConnectorConfig;
typedef struct {
    SRWLOCK *lock;
    CONDITION_VARIABLE drainer_wake;
    CONDITION_VARIABLE slot_available;
    CONDITION_VARIABLE submitters_done;
    HANDLE drainer_thread;
    uint32_t blocked_creators;
    BOOL closing;
    atomic_uint error;
    atomic_uint running;
    atomic_bool drainer_running;
} TcpConnectorInterface;
typedef struct { LPFN_CONNECTEX connect_ex; HANDLE completion_port; } TcpConnectorInternal;
typedef struct { HashMap *tasks; HashMap *pending_io; HashMap *sockets; } TcpConnectorLookup;

struct TcpConnectorLoop {
    TcpConnectorLookup lookup;
    TcpConnectorConfig config;
    TcpConnectorInterface interf;
    TcpConnectorInternal internal;
};


typedef struct {
    SOCKET handle;
    char *id;
    uint32_t pending_tasks;
    BOOL connected;
} TcpConnectorSocket;

typedef struct {
    OVERLAPPED overlapped;
    char *task_id;
    TcpConnectorSocket *socket;
    WSABUF buffer;
    uint32_t attempts;
    ULONGLONG deadline;
    BOOL in_flight;
    BOOL has_deadline;
    BOOL cancel_requested;
    BOOL timed_out;
} TcpConnectorPendingIo;


Value PointerValue(void *pointer);
DWORD TaskErrorToConnectorError(DWORD error);
void SetConnectorError(TcpConnectorLoop *connector, TcpConnectorError error);
TcpConnectorTask *FindTask(TcpConnectorLoop *connector, const char *task_id);
TcpConnectorPendingIo *FindPendingIo(TcpConnectorLoop *connector, const char *task_id);
TcpConnectorSocket *FindSocket(TcpConnectorLoop *connector, const char *socket_id);
void CloseSocket(TcpConnectorSocket *socket);
void RemovePendingIo(TcpConnectorLoop *connector, TcpConnectorPendingIo *io);
void TcpConnectorStartQueued(TcpConnectorLoop *connector);
int TcpConnectorStartTask(TcpConnectorLoop *connector, TcpConnectorTask *task);
ByteArray *TcpConnectorByteArrayNew(uint32_t capacity);
void TcpConnectorByteArrayDestroy(ByteArray *array);
TcpConnectorTask *TcpConnectorTaskNew(const char *task_id, TcpConnectorTaskOption options);
void TcpConnectorTaskDestroy(TcpConnectorTask *task);
DWORD WaitMilliseconds(TcpConnectorLoop *connector);
DWORD ProcessCompletion(TcpConnectorLoop *connector, DWORD wait_ms);
DWORD WINAPI TcpConnectorDrainerThread(LPVOID argument);
TcpConnectorLoop *TcpConnectorGetLoop(void);
BOOL TcpConnectorSetLoop(TcpConnectorLoop *loop);
void TcpConnectorDetachLoop(TcpConnectorLoop *loop);


#endif
