#ifndef TCP_CONNECTOR_MODELS_H
#define TCP_CONNECTOR_MODELS_H

#include "public.h"
#include <stdatomic.h>
#include "../../hashmap/hashmap.h"
#include "../../value/value.h"

#define TCP_CONNECTOR_IO_COMPLETION_WORKER_WAKE_KEY ((ULONG_PTR)~(ULONG_PTR)0)

typedef struct TcpConnectorLoop TcpConnectorLoop;
typedef struct TaskCallback {
    TcpConnectorCompletionCallback cb;
    TcpConnectorTask *arg;
    struct TaskCallback *next;
} TaskCallback;

typedef struct { uint32_t max_pending; uint32_t flags; } TcpConnectorConfig;
typedef struct {
    SRWLOCK *lock;
    CONDITION_VARIABLE slot_available;
    CONDITION_VARIABLE submitters_done;
    CONDITION_VARIABLE task_finished;
    CONDITION_VARIABLE callback_space;
    CONDITION_VARIABLE callback_available;
    HANDLE io_completion_worker;
    HANDLE *callback_threads;
    uint32_t callback_thread_count;
    uint32_t callback_queue_capacity;
    uint32_t callback_queue_length;
    BOOL callback_stopping;
    atomic_bool callback_runner_stop;
    TaskCallback *callback_head;
    TaskCallback *callback_tail;
    TcpConnectorTask *completion_head;
    TcpConnectorTask *completion_tail;
    uint32_t blocked_creators;
    BOOL closing;
    atomic_uint error;
    atomic_uint running;
    atomic_bool io_completion_worker_stop;
} TcpConnectorControlBlock;
typedef struct { LPFN_CONNECTEX connect_ex; HANDLE completion_port; } TcpConnectorInternal;
typedef struct { HashMap *tasks; HashMap *pending_io; HashMap *sockets; } TcpConnectorLookup;

struct TcpConnectorLoop {
    TcpConnectorLookup lookup;
    TcpConnectorConfig config;
    TcpConnectorControlBlock control;
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
void SetConnectorError(TcpConnectorLoop *connector, DWORD error);
TcpConnectorTask *TcpConnectorFindTask(TcpConnectorLoop *connector, const char *task_id);
TcpConnectorPendingIo *TcpConnectorFindPendingIo(TcpConnectorLoop *connector, const char *task_id);
TcpConnectorSocket *FindSocket(TcpConnectorLoop *connector, const char *socket_id);
void CloseSocket(TcpConnectorSocket *socket);
void RemovePendingIo(TcpConnectorLoop *connector, TcpConnectorPendingIo *io);
void TcpConnectorTaskReadySubmitCallback(TcpConnectorLoop *connector, TcpConnectorTask *task);
void TcpConnectorReportCompletion(TcpConnectorLoop *connector, TcpConnectorTask *task);
TaskCallback *TcpConnectorTakeCallback(TcpConnectorLoop *connector);
int TcpConnectorStartTask(TcpConnectorLoop *connector, TcpConnectorTask *task);
ByteArray *TcpConnectorByteArrayNew(uint32_t capacity);
void TcpConnectorByteArrayDestroy(ByteArray *array);
TcpConnectorTask *TcpConnectorTaskNew(const char *task_id, TcpConnectorTaskOption options);
void TcpConnectorTaskDestroy(TcpConnectorTask *task);
DWORD TcpConnectorHandleCompletion(TcpConnectorLoop *connector, DWORD wait_ms);
DWORD WINAPI TcpConnectorIoCompletionWorker(LPVOID argument);
DWORD WINAPI TcpConnectorTaskCallbackRunner(LPVOID argument);
TcpConnectorLoop *TcpConnectorGetLoop(void);
BOOL TcpConnectorSetLoop(TcpConnectorLoop *loop);
void TcpConnectorDetachLoop(TcpConnectorLoop *loop);


#endif
