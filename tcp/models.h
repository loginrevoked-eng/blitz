#ifndef TCP_CONNECTOR_MODELS_H
#define TCP_CONNECTOR_MODELS_H

#include "public.h"
#include <stdatomic.h>
#include "../hashmap/hashmap.h"
#include "../value/value.h"

#define TCP_CONNECTOR_IO_COMPLETION_WORKER_WAKE_KEY ((ULONG_PTR)~(ULONG_PTR)0)

typedef TcpConnector TcpConnectorLoop;
typedef struct TaskCallback {
    TcpConnectorCompletionCallback cb;
    TcpConnectorTask *arg;
} TaskCallback;
typedef struct {
    TaskCallback *cbs;
    uint32_t len;
    uint32_t cap;
} TaskCallbackQueue;

typedef struct { uint32_t max_pending; uint32_t flags; } TcpConnectorConfig;
typedef struct {
    /* Protects task, socket, pending-I/O, and lifecycle state. */
    SRWLOCK state_lock;
    /* Protects callback_queue and callback_stopping. */
    SRWLOCK callback_lock;
    CONDITION_VARIABLE slot_available;
    CONDITION_VARIABLE submitters_done;
    CONDITION_VARIABLE task_finished;
    CONDITION_VARIABLE callback_available;
    HANDLE io_completion_worker;
    HANDLE callback_thread;
    BOOL callback_stopping;
    atomic_bool callback_runner_stop;
    TaskCallbackQueue callback_queue;
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



#endif
