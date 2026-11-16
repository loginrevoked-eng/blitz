#include "pool.h"
#include "ctors.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    SOCKET handle;
    char *id;
    uint32_t pending_ops;
    BOOL connected;
} IocpSocket;

typedef struct {
    OVERLAPPED overlapped;
    char *task_id;
    IocpSocket *socket;
    WSABUF buffer;
    uint32_t attempts;
    ULONGLONG deadline;
    BOOL in_flight;
    BOOL has_deadline;
    BOOL cancel_requested;
    BOOL timed_out;
} IocpPendingIo;

static void FinishOperationTask(IocpTask *task);

static Value PointerValue(void *pointer)
{
    Value value = { 0 };
    value.kind = VALUE_POINTER;
    value.ptr = pointer;
    return value;
}

static void SetPoolError(IocpPool *pool, IocpPoolError error)
{
    if (pool != NULL)
        atomic_store(&pool->error, (unsigned int)error);
}

static IocpTask *FindTask(IocpPool *pool, const char *task_id)
{
    Value *value = HashMap_Get(pool->tasks, task_id);
    return value != NULL && value->kind == VALUE_POINTER ? value->ptr : NULL;
}

static IocpPendingIo *FindOperation(IocpPool *pool, const char *task_id)
{
    Value *value = HashMap_Get(pool->overlapped, task_id);
    return value != NULL && value->kind == VALUE_POINTER ? value->ptr : NULL;
}

static IocpSocket *FindSocket(IocpPool *pool, const char *socket_id)
{
    Value *value = HashMap_Get(pool->sockets, socket_id);
    return value != NULL && value->kind == VALUE_POINTER ? value->ptr : NULL;
}

static void CloseSocket(IocpSocket *socket)
{
    if (socket != NULL && socket->handle != INVALID_SOCKET) {
        closesocket(socket->handle);
        socket->handle = INVALID_SOCKET;
        socket->connected = FALSE;
    }
}

static void RemoveOperation(IocpPool *pool, IocpPendingIo *operation)
{
    if (operation == NULL)
        return;
    HashMap_Remove(pool->overlapped, operation->task_id);
    free(operation);
}

static DWORD TaskErrorToPoolError(DWORD error)
{
    if (error == ERROR_SUCCESS)
        return IOCP_POOL_SUCCESS;
    if (error == ERROR_NOT_ENOUGH_MEMORY)
        return IOCP_POOL_ERROR_OUT_OF_MEMORY;
    if (error == WSAETIMEDOUT)
        return IOCP_POOL_ERROR_TIMEOUT;
    if (error == WSA_OPERATION_ABORTED)
        return IOCP_POOL_ERROR_CANCELED;
    return IOCP_POOL_ERROR_IO;
}

static LPFN_CONNECTEX GetConnectEx(void)
{
    SOCKET socket_handle = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket_handle == INVALID_SOCKET)
        return NULL;

    GUID id = WSAID_CONNECTEX;
    LPFN_CONNECTEX connect_ex = NULL;
    DWORD bytes_returned = 0;
    int result = WSAIoctl(socket_handle, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &id, sizeof(id), &connect_ex, sizeof(connect_ex), &bytes_returned,
        NULL, NULL);
    closesocket(socket_handle);
    return result == SOCKET_ERROR ? NULL : connect_ex;
}

static void FinishSynchronousFailure(IocpPool *pool, IocpTask *task,
    IocpPendingIo *operation, DWORD error, BOOL retry_connect)
{
    task->WSA_error = error;
    task->error = (IocpPoolError)TaskErrorToPoolError(error);
    if (retry_connect && operation->attempts <= task->options.conn_opt.retries) {
        task->status = IOCP_TASK_QUEUED;
        return;
    }

    task->status = IOCP_TASK_COMPLETED;
    RemoveOperation(pool, operation);
    FinishOperationTask(task);
}

static IocpPendingIo *GetOrCreateOperation(IocpPool *pool, IocpTask *task)
{
    IocpPendingIo *operation = FindOperation(pool, task->id);
    if (operation != NULL)
        return operation;

    operation = calloc(1, sizeof(*operation));
    if (operation == NULL)
        return NULL;
    operation->task_id = task->id;
    if (!HashMap_Set(pool->overlapped, task->id, PointerValue(operation))) {
        free(operation);
        return NULL;
    }
    return operation;
}

static int StartConnectTask(IocpPool *pool, IocpTask *task,
    IocpPendingIo *operation)
{
    ++operation->attempts;
    operation->socket = NULL;
    operation->cancel_requested = FALSE;
    operation->timed_out = FALSE;
    operation->has_deadline = TRUE;
    memset(&operation->overlapped, 0, sizeof(operation->overlapped));

    IocpSocket *socket = calloc(1, sizeof(*socket));
    if (socket == NULL) {
        FinishSynchronousFailure(pool, task, operation,
            ERROR_NOT_ENOUGH_MEMORY, TRUE);
        return 0;
    }
    socket->id = task->id;
    socket->handle = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket->handle == INVALID_SOCKET) {
        DWORD error = (DWORD)WSAGetLastError();
        free(socket);
        FinishSynchronousFailure(pool, task, operation, error, TRUE);
        return 0;
    }

    struct sockaddr_in local = { 0 };
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(socket->handle, (struct sockaddr *)&local, sizeof(local))
            == SOCKET_ERROR) {
        DWORD error = (DWORD)WSAGetLastError();
        CloseSocket(socket);
        free(socket);
        FinishSynchronousFailure(pool, task, operation, error, TRUE);
        return 0;
    }

    if (CreateIoCompletionPort((HANDLE)socket->handle, pool->completion_port,
            (ULONG_PTR)socket, 0) == NULL) {
        DWORD error = GetLastError();
        CloseSocket(socket);
        free(socket);
        FinishSynchronousFailure(pool, task, operation, error, TRUE);
        return 0;
    }

    if (!HashMap_Set(pool->sockets, task->id, PointerValue(socket))) {
        CloseSocket(socket);
        free(socket);
        FinishSynchronousFailure(pool, task, operation,
            ERROR_NOT_ENOUGH_MEMORY, TRUE);
        return 0;
    }

    struct sockaddr_in remote = { 0 };
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(task->options.conn_opt.addr);
    remote.sin_port = htons(task->options.conn_opt.port);

    operation->socket = socket;
    operation->deadline = GetTickCount64() +
        task->options.conn_opt.timeout_ms;
    operation->in_flight = TRUE;
    socket->pending_ops++;
    task->status = IOCP_TASK_PENDING;
    task->WSA_error = ERROR_IO_PENDING;
    atomic_fetch_add(&pool->running, 1);

    BOOL started = pool->connect_ex(socket->handle,
        (struct sockaddr *)&remote, sizeof(remote), NULL, 0, NULL,
        &operation->overlapped);
    if (!started && WSAGetLastError() != WSA_IO_PENDING) {
        DWORD error = (DWORD)WSAGetLastError();
        HashMap_Remove(pool->sockets, task->id);
        CloseSocket(socket);
        if (socket->pending_ops > 0)
            --socket->pending_ops;
        free(socket);
        operation->socket = NULL;
        operation->in_flight = FALSE;
        atomic_fetch_sub(&pool->running, 1);
        FinishSynchronousFailure(pool, task, operation, error, TRUE);
        return 0;
    }
    return 1;
}

static int StartSocketTask(IocpPool *pool, IocpTask *task,
    IocpPendingIo *operation)
{
    IocpSROption *options = &task->options.sr_opt;
    IocpSocket *socket = FindSocket(pool, options->socket_id);
    if (socket == NULL || !socket->connected ||
        socket->handle == INVALID_SOCKET) {
        FinishSynchronousFailure(pool, task, operation, WSAENOTCONN, FALSE);
        return 0;
    }

    ByteArray *payload = options->payload;
    if (payload == NULL || payload->buffer == NULL) {
        FinishSynchronousFailure(pool, task, operation, WSAEINVAL, FALSE);
        return 0;
    }

    uint32_t length = task->options.kind == IOCP_TASK_SEND
        ? payload->len : payload->cap;
    if (length == 0) {
        FinishSynchronousFailure(pool, task, operation, WSAEINVAL, FALSE);
        return 0;
    }

    operation->socket = socket;
    operation->cancel_requested = FALSE;
    operation->timed_out = FALSE;
    operation->has_deadline = FALSE;
    operation->in_flight = TRUE;
    operation->buffer.buf = (CHAR *)payload->buffer;
    operation->buffer.len = length;
    memset(&operation->overlapped, 0, sizeof(operation->overlapped));
    if (task->options.kind == IOCP_TASK_RECV)
        payload->len = 0;

    task->status = IOCP_TASK_PENDING;
    task->WSA_error = ERROR_IO_PENDING;
    socket->pending_ops++;
    atomic_fetch_add(&pool->running, 1);

    DWORD flags = 0;
    int result;
    if (task->options.kind == IOCP_TASK_SEND) {
        result = WSASend(socket->handle, &operation->buffer, 1, NULL,
            0, &operation->overlapped, NULL);
    } else {
        result = WSARecv(socket->handle, &operation->buffer, 1, NULL,
            &flags, &operation->overlapped, NULL);
    }

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        DWORD error = (DWORD)WSAGetLastError();
        operation->in_flight = FALSE;
        operation->socket = NULL;
        socket->pending_ops--;
        atomic_fetch_sub(&pool->running, 1);
        FinishSynchronousFailure(pool, task, operation, error, FALSE);
        return 0;
    }
    return 1;
}

static int StartTask(IocpPool *pool, IocpTask *task)
{
    IocpPendingIo *operation = GetOrCreateOperation(pool, task);
    if (operation == NULL) {
        task->status = IOCP_TASK_COMPLETED;
        task->error = IOCP_POOL_ERROR_OUT_OF_MEMORY;
        task->WSA_error = ERROR_NOT_ENOUGH_MEMORY;
        FinishOperationTask(task);
        return 0;
    }

    if (task->options.kind == IOCP_TASK_CONNECT)
        return StartConnectTask(pool, task, operation);
    return StartSocketTask(pool, task, operation);
}

typedef struct {
    IocpPool *pool;
    int started;
} StartQueuedContext;

static int StartQueuedEntry(const char *key, const Value *value,
    void *context)
{
    (void)key;
    StartQueuedContext *start = context;
    IocpTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    int operation_turn = task == NULL || task->operation == NULL ||
        (task->operation->submitted &&
            task->operation->status != IOCP_OPERATION_FAILED &&
            task->operation->status != IOCP_OPERATION_CANCELED &&
            task->operation->status != IOCP_OPERATION_COMPLETED &&
            IocpTaskListGet(&task->operation->tasks,
                task->operation->next_task) == task);
    if (task != NULL && task->status == IOCP_TASK_QUEUED && operation_turn &&
        atomic_load(&start->pool->running) < start->pool->limit) {
        if (task->operation != NULL)
            task->operation->status = IOCP_OPERATION_RUNNING;
        start->started = 1;
        StartTask(start->pool, task);
    }
    return 0;
}

static void StartQueued(IocpPool *pool)
{
    while (atomic_load(&pool->running) < pool->limit) {
        StartQueuedContext context = { pool, 0 };
        HashMap_RemoveIf(pool->tasks, StartQueuedEntry, &context);
        if (!context.started)
            break;
    }
}

static void SetTaskFinished(IocpTask *task, DWORD error,
    IocpTaskStatus status)
{
    task->status = status;
    task->WSA_error = error;
    task->error = (IocpPoolError)TaskErrorToPoolError(error);
}

static void FinishOperationTask(IocpTask *task)
{
    IocpOperation *operation = task == NULL ? NULL : task->operation;
    if (operation == NULL || operation->status == IOCP_OPERATION_CANCELED ||
        operation->status == IOCP_OPERATION_FAILED)
        return;

    if (task->status != IOCP_TASK_COMPLETED || task->WSA_error != ERROR_SUCCESS) {
        operation->status = IOCP_OPERATION_FAILED;
        operation->error = task->error == IOCP_POOL_SUCCESS
            ? IOCP_POOL_ERROR_IO : task->error;
        operation->WSA_error = task->WSA_error;
        for (uint32_t i = operation->next_task; i < operation->tasks.len; ++i) {
            IocpTask *remaining = IocpTaskListGet(&operation->tasks, i);
            if (remaining != NULL && remaining != task &&
                remaining->status == IOCP_TASK_QUEUED) {
                remaining->status = IOCP_TASK_CANCELED;
                remaining->error = IOCP_POOL_ERROR_CANCELED;
                remaining->WSA_error = WSA_OPERATION_ABORTED;
            }
        }
        return;
    }

    if (operation->next_task < operation->tasks.len &&
        IocpTaskListGet(&operation->tasks, operation->next_task) == task)
        ++operation->next_task;
    if (operation->next_task == operation->tasks.len) {
        operation->status = IOCP_OPERATION_COMPLETED;
        operation->error = IOCP_POOL_SUCCESS;
        operation->WSA_error = ERROR_SUCCESS;
    }
}

typedef struct {
    ULONGLONG deadline;
    int found;
} DeadlineContext;

static int FindDeadlineEntry(const char *key, const Value *value,
    void *context)
{
    (void)key;
    IocpPendingIo *operation = value->kind == VALUE_POINTER
        ? value->ptr : NULL;
    DeadlineContext *deadline = context;
    if (operation != NULL && operation->in_flight &&
        operation->has_deadline) {
        /* Only connect operations have a deadline. */
        if (!deadline->found || operation->deadline < deadline->deadline) {
            deadline->deadline = operation->deadline;
            deadline->found = 1;
        }
    }
    return 0;
}

static DWORD WaitMilliseconds(IocpPool *pool)
{
    DeadlineContext deadline = { 0 };
    AcquireSRWLockExclusive(pool->lock);
    HashMap_RemoveIf(pool->overlapped, FindDeadlineEntry, &deadline);
    ReleaseSRWLockExclusive(pool->lock);
    if (!deadline.found)
        return INFINITE;

    ULONGLONG now = GetTickCount64();
    if (now >= deadline.deadline)
        return 0;
    ULONGLONG remaining = deadline.deadline - now;
    return remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
}

typedef struct {
    IocpPool *pool;
    ULONGLONG now;
} CancelExpiredContext;

static int CancelExpiredEntry(const char *task_id, const Value *value,
    void *context)
{
    IocpPendingIo *operation = value->kind == VALUE_POINTER
        ? value->ptr : NULL;
    CancelExpiredContext *expired = context;
    if (operation == NULL || !operation->in_flight || !operation->has_deadline ||
        operation->cancel_requested || operation->timed_out ||
        expired->now < operation->deadline)
        return 0;

    IocpTask *task = FindTask(expired->pool, task_id);
    if (task == NULL || task->options.kind != IOCP_TASK_CONNECT)
        return 0;

    operation->timed_out = TRUE;
    SetTaskFinished(task, WSAETIMEDOUT, IOCP_TASK_CANCELED);
    CloseSocket(operation->socket);
    return 0;
}

static void CancelExpired(IocpPool *pool)
{
    CancelExpiredContext context = { pool, GetTickCount64() };
    HashMap_RemoveIf(pool->overlapped, CancelExpiredEntry, &context);
}

static DWORD ProcessCompletion(IocpPool *pool, DWORD wait_ms)
{
    DWORD transferred = 0;
    ULONG_PTR key = 0;
    OVERLAPPED *overlapped = NULL;
    BOOL success = GetQueuedCompletionStatus(pool->completion_port,
        &transferred, &key, &overlapped, wait_ms);
    DWORD completion_error = success ? ERROR_SUCCESS : GetLastError();

    if (overlapped == NULL) {
        if (!success && completion_error == WAIT_TIMEOUT) {
            AcquireSRWLockExclusive(pool->lock);
            CancelExpired(pool);
            ReleaseSRWLockExclusive(pool->lock);
            return ERROR_SUCCESS;
        }
        return success ? ERROR_INVALID_DATA : completion_error;
    }

    IocpPendingIo *operation = (IocpPendingIo *)overlapped;
    IocpTask *task = NULL;
    CompletionCallback callback = NULL;
    BOOL terminal = FALSE;

    AcquireSRWLockExclusive(pool->lock);
    task = FindTask(pool, operation->task_id);
    if (task == NULL || FindOperation(pool, operation->task_id) != operation ||
        !operation->in_flight || (IocpSocket *)key != operation->socket) {
        ReleaseSRWLockExclusive(pool->lock);
        SetPoolError(pool, IOCP_POOL_ERROR_IO);
        return ERROR_INVALID_DATA;
    }

    IocpSocket *socket = operation->socket;
    operation->in_flight = FALSE;
    if (socket != NULL && socket->pending_ops > 0)
        --socket->pending_ops;
    atomic_fetch_sub(&pool->running, 1);

    if (operation->timed_out)
        completion_error = WSAETIMEDOUT;

    if (operation->cancel_requested) {
        SetTaskFinished(task, WSA_OPERATION_ABORTED, IOCP_TASK_CANCELED);
        if (task->options.kind == IOCP_TASK_CONNECT && socket != NULL) {
            HashMap_Remove(pool->sockets, task->id);
            CloseSocket(socket);
            free(socket);
            operation->socket = NULL;
        }
        RemoveOperation(pool, operation);
        terminal = TRUE;
    } else if (task->options.kind == IOCP_TASK_CONNECT) {
        if (completion_error == ERROR_SUCCESS) {
            if (setsockopt(socket->handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,
                    NULL, 0) == SOCKET_ERROR)
                completion_error = (DWORD)WSAGetLastError();
        }

        if (completion_error == ERROR_SUCCESS) {
            socket->connected = TRUE;
            SetTaskFinished(task, ERROR_SUCCESS, IOCP_TASK_COMPLETED);
            RemoveOperation(pool, operation);
            terminal = TRUE;
        } else {
            HashMap_Remove(pool->sockets, task->id);
            CloseSocket(socket);
            free(socket);
            operation->socket = NULL;
            if (!operation->timed_out &&
                operation->attempts <= task->options.conn_opt.retries) {
                task->status = IOCP_TASK_QUEUED;
                task->error = (IocpPoolError)TaskErrorToPoolError(completion_error);
                task->WSA_error = completion_error;
            } else if (operation->timed_out &&
                operation->attempts <= task->options.conn_opt.retries) {
                task->status = IOCP_TASK_QUEUED;
                task->error = IOCP_POOL_ERROR_TIMEOUT;
                task->WSA_error = WSAETIMEDOUT;
            } else {
                SetTaskFinished(task, completion_error, IOCP_TASK_COMPLETED);
                RemoveOperation(pool, operation);
                terminal = TRUE;
            }
        }
    } else {
        if (task->options.kind == IOCP_TASK_RECV && completion_error == ERROR_SUCCESS)
            task->options.sr_opt.payload->len = transferred;
        SetTaskFinished(task, completion_error, IOCP_TASK_COMPLETED);
        RemoveOperation(pool, operation);
        terminal = TRUE;
    }

    callback = task->on_completed;
    if (terminal)
        FinishOperationTask(task);
    StartQueued(pool);
    ReleaseSRWLockExclusive(pool->lock);

    if (terminal && callback != NULL)
        callback(task, pool);
    return ERROR_SUCCESS;
}

IocpPool *IocpPoolNew(uint32_t limit)
{
    if (limit == 0)
        return NULL;

    IocpPool *pool = calloc(1, sizeof(*pool));
    if (pool == NULL)
        return NULL;

    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0)
        goto fail;

    size_t capacity = (size_t)limit + 16;
    pool->tasks = HashMap_New(capacity);
    pool->overlapped = HashMap_New(capacity);
    pool->sockets = HashMap_New(capacity);
    pool->operations = HashMap_New(capacity);
    pool->lock = malloc(sizeof(*pool->lock));
    if (pool->tasks == NULL || pool->overlapped == NULL ||
        pool->sockets == NULL || pool->operations == NULL || pool->lock == NULL)
        goto fail_winsock;

    InitializeSRWLock(pool->lock);
    pool->completion_port = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (pool->completion_port == NULL)
        goto fail_winsock;

    pool->connect_ex = GetConnectEx();
    if (pool->connect_ex == NULL)
        goto fail_winsock;

    pool->limit = limit;
    atomic_init(&pool->error, IOCP_POOL_SUCCESS);
    atomic_init(&pool->running, 0);
    return pool;

fail_winsock:
    if (pool->completion_port != NULL)
        CloseHandle(pool->completion_port);
    HashMap_Free(pool->tasks);
    HashMap_Free(pool->overlapped);
    HashMap_Free(pool->sockets);
    HashMap_Free(pool->operations);
    free(pool->lock);
    WSACleanup();
fail:
    free(pool);
    return NULL;
}

DWORD IocpPoolCreateTask(IocpPool *pool, IocpTaskOption options,
    const char *task_id)
{
    if (pool == NULL || task_id == NULL || task_id[0] == '\0') {
        SetPoolError(pool, IOCP_POOL_ERROR_INVALID_ARGUMENT);
        return ERROR_INVALID_PARAMETER;
    }

    if (options.kind != IOCP_TASK_CONNECT && options.kind != IOCP_TASK_SEND &&
        options.kind != IOCP_TASK_RECV) {
        SetPoolError(pool, IOCP_POOL_ERROR_UNSUPPORTED_TASK);
        return ERROR_NOT_SUPPORTED;
    }
    if (options.kind == IOCP_TASK_CONNECT && options.conn_opt.port == 0) {
        SetPoolError(pool, IOCP_POOL_ERROR_INVALID_ARGUMENT);
        return WSAEINVAL;
    }
    if ((options.kind == IOCP_TASK_SEND || options.kind == IOCP_TASK_RECV) &&
        (options.sr_opt.socket_id == NULL || options.sr_opt.payload == NULL ||
            (options.kind == IOCP_TASK_SEND && options.sr_opt.payload->len == 0) ||
            (options.kind == IOCP_TASK_RECV && options.sr_opt.payload->cap == 0))) {
        SetPoolError(pool, IOCP_POOL_ERROR_INVALID_ARGUMENT);
        return WSAEINVAL;
    }

    IocpTask *task = IocpTaskNew(task_id, options);
    if (task == NULL) {
        SetPoolError(pool, IOCP_POOL_ERROR_OUT_OF_MEMORY);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    AcquireSRWLockExclusive(pool->lock);
    if (FindTask(pool, task_id) != NULL) {
        ReleaseSRWLockExclusive(pool->lock);
        IocpTaskDestroy(task);
        SetPoolError(pool, IOCP_POOL_ERROR_DUPLICATE_TASK_ID);
        return ERROR_ALREADY_EXISTS;
    }
    if (!HashMap_Set(pool->tasks, task->id, PointerValue(task))) {
        ReleaseSRWLockExclusive(pool->lock);
        IocpTaskDestroy(task);
        SetPoolError(pool, IOCP_POOL_ERROR_OUT_OF_MEMORY);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    SetPoolError(pool, IOCP_POOL_SUCCESS);
    StartQueued(pool);
    ReleaseSRWLockExclusive(pool->lock);
    return ERROR_SUCCESS;
}

DWORD IocpPoolCreateOperation(IocpPool *pool, const char *operation_id)
{
    if (pool == NULL || operation_id == NULL || operation_id[0] == '\0')
        return ERROR_INVALID_PARAMETER;
    IocpOperation *operation = IocpOperationNew(operation_id);
    if (operation == NULL) {
        SetPoolError(pool, IOCP_POOL_ERROR_OUT_OF_MEMORY);
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    AcquireSRWLockExclusive(pool->lock);
    if (HashMap_Get(pool->operations, operation_id) != NULL) {
        ReleaseSRWLockExclusive(pool->lock);
        IocpOperationDestroy(operation);
        SetPoolError(pool, IOCP_POOL_ERROR_DUPLICATE_TASK_ID);
        return ERROR_ALREADY_EXISTS;
    }
    if (!HashMap_Set(pool->operations, operation_id,
            PointerValue(operation))) {
        ReleaseSRWLockExclusive(pool->lock);
        IocpOperationDestroy(operation);
        SetPoolError(pool, IOCP_POOL_ERROR_OUT_OF_MEMORY);
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    ReleaseSRWLockExclusive(pool->lock);
    return ERROR_SUCCESS;
}

DWORD IocpPoolOperationAddTask(IocpPool *pool, const char *operation_id,
    IocpTaskOption options, const char *task_id)
{
    if (pool == NULL || operation_id == NULL || task_id == NULL ||
        task_id[0] == '\0')
        return ERROR_INVALID_PARAMETER;
    if (options.kind != IOCP_TASK_CONNECT && options.kind != IOCP_TASK_SEND &&
        options.kind != IOCP_TASK_RECV)
        return ERROR_NOT_SUPPORTED;
    if ((options.kind == IOCP_TASK_CONNECT &&
            options.conn_opt.port == 0) ||
        ((options.kind == IOCP_TASK_SEND || options.kind == IOCP_TASK_RECV) &&
            (options.sr_opt.socket_id == NULL || options.sr_opt.payload == NULL ||
                (options.kind == IOCP_TASK_SEND && options.sr_opt.payload->len == 0) ||
                (options.kind == IOCP_TASK_RECV && options.sr_opt.payload->cap == 0)))) {
        SetPoolError(pool, IOCP_POOL_ERROR_INVALID_ARGUMENT);
        return WSAEINVAL;
    }

    IocpTask *task = IocpTaskNew(task_id, options);
    if (task == NULL) {
        SetPoolError(pool, IOCP_POOL_ERROR_OUT_OF_MEMORY);
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    AcquireSRWLockExclusive(pool->lock);
    Value *stored = HashMap_Get(pool->operations, operation_id);
    IocpOperation *operation = stored != NULL && stored->kind == VALUE_POINTER
        ? stored->ptr : NULL;
    if (operation == NULL) {
        ReleaseSRWLockExclusive(pool->lock);
        IocpTaskDestroy(task);
        SetPoolError(pool, IOCP_POOL_ERROR_TASK_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }
    if (operation->status != IOCP_OPERATION_QUEUED || operation->submitted ||
        HashMap_Get(pool->tasks, task_id) != NULL) {
        ReleaseSRWLockExclusive(pool->lock);
        IocpTaskDestroy(task);
        SetPoolError(pool, IOCP_POOL_ERROR_DUPLICATE_TASK_ID);
        return ERROR_ALREADY_EXISTS;
    }

    task->operation = operation;
    task->operation_index = operation->tasks.len;
    if (!HashMap_Set(pool->tasks, task->id, PointerValue(task))) {
        task->operation = NULL;
        ReleaseSRWLockExclusive(pool->lock);
        IocpTaskDestroy(task);
        SetPoolError(pool, IOCP_POOL_ERROR_OUT_OF_MEMORY);
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    if (!IocpTaskListAppend(&operation->tasks, task)) {
        HashMap_Remove(pool->tasks, task->id);
        task->operation = NULL;
        ReleaseSRWLockExclusive(pool->lock);
        IocpTaskDestroy(task);
        SetPoolError(pool, IOCP_POOL_ERROR_OUT_OF_MEMORY);
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    ReleaseSRWLockExclusive(pool->lock);
    return ERROR_SUCCESS;
}

DWORD IocpPoolSubmitOperation(IocpPool *pool, const char *operation_id)
{
    if (pool == NULL || operation_id == NULL)
        return ERROR_INVALID_PARAMETER;
    AcquireSRWLockExclusive(pool->lock);
    Value *stored = HashMap_Get(pool->operations, operation_id);
    IocpOperation *operation = stored != NULL && stored->kind == VALUE_POINTER
        ? stored->ptr : NULL;
    if (operation == NULL) {
        ReleaseSRWLockExclusive(pool->lock);
        SetPoolError(pool, IOCP_POOL_ERROR_TASK_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }
    if (operation->submitted) {
        ReleaseSRWLockExclusive(pool->lock);
        return ERROR_ALREADY_EXISTS;
    }
    operation->submitted = TRUE;
    if (operation->tasks.len == 0)
        operation->status = IOCP_OPERATION_COMPLETED;
    else
        StartQueued(pool);
    ReleaseSRWLockExclusive(pool->lock);
    return ERROR_SUCCESS;
}

DWORD IocpPoolGetOperation(IocpPool *pool, const char *operation_id,
    IocpOperation *operation)
{
    if (pool == NULL || operation_id == NULL || operation == NULL)
        return ERROR_INVALID_PARAMETER;
    AcquireSRWLockExclusive(pool->lock);
    Value *stored = HashMap_Get(pool->operations, operation_id);
    IocpOperation *found = stored != NULL && stored->kind == VALUE_POINTER
        ? stored->ptr : NULL;
    if (found != NULL)
        *operation = *found;
    ReleaseSRWLockExclusive(pool->lock);
    if (found == NULL) {
        SetPoolError(pool, IOCP_POOL_ERROR_TASK_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }
    return ERROR_SUCCESS;
}

DWORD IocpPoolCancelOperation(IocpPool *pool, const char *operation_id)
{
    if (pool == NULL || operation_id == NULL)
        return ERROR_INVALID_PARAMETER;
    AcquireSRWLockExclusive(pool->lock);
    Value *stored = HashMap_Get(pool->operations, operation_id);
    IocpOperation *operation = stored != NULL && stored->kind == VALUE_POINTER
        ? stored->ptr : NULL;
    if (operation == NULL) {
        ReleaseSRWLockExclusive(pool->lock);
        SetPoolError(pool, IOCP_POOL_ERROR_TASK_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }
    operation->status = IOCP_OPERATION_CANCELED;
    operation->error = IOCP_POOL_ERROR_CANCELED;
    operation->WSA_error = WSA_OPERATION_ABORTED;
    for (uint32_t i = 0; i < operation->tasks.len; ++i) {
        IocpTask *task = IocpTaskListGet(&operation->tasks, i);
        if (task != NULL && task->status == IOCP_TASK_QUEUED) {
            task->status = IOCP_TASK_CANCELED;
            task->error = IOCP_POOL_ERROR_CANCELED;
            task->WSA_error = WSA_OPERATION_ABORTED;
        }
    }
    ReleaseSRWLockExclusive(pool->lock);
    for (uint32_t i = 0; i < operation->tasks.len; ++i) {
        IocpTask *task = IocpTaskListGet(&operation->tasks, i);
        if (task != NULL && task->status == IOCP_TASK_PENDING)
            IocpPoolCancelTask(pool, task->id);
    }
    return ERROR_SUCCESS;
}

DWORD IocpPoolWaitOperation(IocpPool *pool, const char *operation_id)
{
    if (pool == NULL || operation_id == NULL)
        return ERROR_INVALID_PARAMETER;
    for (;;) {
        AcquireSRWLockExclusive(pool->lock);
        Value *stored = HashMap_Get(pool->operations, operation_id);
        IocpOperation *operation = stored != NULL &&
            stored->kind == VALUE_POINTER ? stored->ptr : NULL;
        if (operation == NULL) {
            ReleaseSRWLockExclusive(pool->lock);
            SetPoolError(pool, IOCP_POOL_ERROR_TASK_NOT_FOUND);
            return ERROR_NOT_FOUND;
        }
        if (!operation->submitted) {
            ReleaseSRWLockExclusive(pool->lock);
            return ERROR_INVALID_STATE;
        }
        if (operation->tasks.len == 0)
            operation->status = IOCP_OPERATION_COMPLETED;
        StartQueued(pool);
        int pending = 0;
        for (uint32_t i = 0; i < operation->tasks.len; ++i) {
            IocpTask *task = IocpTaskListGet(&operation->tasks, i);
            IocpPendingIo *io = task == NULL ? NULL : FindOperation(pool, task->id);
            if (io != NULL && io->in_flight) {
                pending = 1;
                break;
            }
        }
        IocpOperationStatus status = operation->status;
        IocpPoolError task_error = operation->error;
        DWORD WSA_error = operation->WSA_error;
        ReleaseSRWLockExclusive(pool->lock);
        if ((status == IOCP_OPERATION_COMPLETED ||
                status == IOCP_OPERATION_FAILED ||
                status == IOCP_OPERATION_CANCELED) && !pending) {
            if (status == IOCP_OPERATION_FAILED || status == IOCP_OPERATION_CANCELED)
                return WSA_error != ERROR_SUCCESS ? WSA_error :
                    (task_error == IOCP_POOL_ERROR_TIMEOUT
                        ? WSAETIMEDOUT : WSA_OPERATION_ABORTED);
            return ERROR_SUCCESS;
        }
        DWORD error = ProcessCompletion(pool, WaitMilliseconds(pool));
        if (error != ERROR_SUCCESS) {
            SetPoolError(pool, IOCP_POOL_ERROR_IO);
            return error;
        }
    }
}

typedef struct {
    IocpPool *pool;
    int pending;
} PendingOperationsContext;

static int FindPendingOperation(const char *key, const Value *value,
    void *context)
{
    (void)key;
    IocpOperation *operation = value->kind == VALUE_POINTER
        ? value->ptr : NULL;
    PendingOperationsContext *pending = context;
    if (operation == NULL || !operation->submitted ||
        operation->status == IOCP_OPERATION_COMPLETED ||
        operation->status == IOCP_OPERATION_FAILED)
        return 0;

    if (operation->status == IOCP_OPERATION_CANCELED) {
        for (uint32_t i = 0; i < operation->tasks.len; ++i) {
            IocpTask *task = IocpTaskListGet(&operation->tasks, i);
            IocpPendingIo *io = task == NULL
                ? NULL : FindOperation(pending->pool, task->id);
            if (io != NULL && io->in_flight) {
                pending->pending = 1;
                break;
            }
        }
        return 0;
    }

    pending->pending = 1;
    return 0;
}

DWORD IocpPoolWaitOperations(IocpPool *pool)
{
    if (pool == NULL)
        return ERROR_INVALID_PARAMETER;

    for (;;) {
        AcquireSRWLockExclusive(pool->lock);
        StartQueued(pool);
        PendingOperationsContext context = { pool, 0 };
        HashMap_RemoveIf(pool->operations, FindPendingOperation, &context);
        ReleaseSRWLockExclusive(pool->lock);

        if (!context.pending)
            return ERROR_SUCCESS;

        DWORD error = ProcessCompletion(pool, WaitMilliseconds(pool));
        if (error != ERROR_SUCCESS) {
            SetPoolError(pool, IOCP_POOL_ERROR_IO);
            return error;
        }
    }
}

DWORD IocpPoolGetTask(IocpPool *pool, const char *task_id, IocpTask *task)
{
    if (pool == NULL || task_id == NULL || task == NULL)
        return ERROR_INVALID_PARAMETER;

    AcquireSRWLockExclusive(pool->lock);
    IocpTask *stored = FindTask(pool, task_id);
    if (stored != NULL)
        *task = *stored;
    ReleaseSRWLockExclusive(pool->lock);
    if (stored == NULL) {
        SetPoolError(pool, IOCP_POOL_ERROR_TASK_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }
    return ERROR_SUCCESS;
}

DWORD IocpPoolCancelTask(IocpPool *pool, const char *task_id)
{
    if (pool == NULL || task_id == NULL)
        return ERROR_INVALID_PARAMETER;

    AcquireSRWLockExclusive(pool->lock);
    IocpTask *task = FindTask(pool, task_id);
    if (task == NULL) {
        ReleaseSRWLockExclusive(pool->lock);
        SetPoolError(pool, IOCP_POOL_ERROR_TASK_NOT_FOUND);
        return ERROR_NOT_FOUND;
    }
    if (task->status == IOCP_TASK_COMPLETED ||
        task->status == IOCP_TASK_CANCELED) {
        ReleaseSRWLockExclusive(pool->lock);
        return ERROR_SUCCESS;
    }

    IocpPendingIo *operation = FindOperation(pool, task_id);
    task->status = IOCP_TASK_CANCELED;
    task->error = IOCP_POOL_ERROR_CANCELED;
    task->WSA_error = WSA_OPERATION_ABORTED;
    if (operation == NULL || !operation->in_flight) {
        RemoveOperation(pool, operation);
        FinishOperationTask(task);
        ReleaseSRWLockExclusive(pool->lock);
        return ERROR_SUCCESS;
    }

    operation->cancel_requested = TRUE;
    if (task->options.kind == IOCP_TASK_CONNECT) {
        CloseSocket(operation->socket);
    } else if (operation->socket != NULL &&
        CancelIoEx((HANDLE)operation->socket->handle,
            &operation->overlapped) == 0 && GetLastError() != ERROR_NOT_FOUND) {
        DWORD error = GetLastError();
        ReleaseSRWLockExclusive(pool->lock);
        SetPoolError(pool, IOCP_POOL_ERROR_IO);
        return error;
    }
    ReleaseSRWLockExclusive(pool->lock);
    return ERROR_SUCCESS;
}

static DWORD WaitMilliseconds(IocpPool *pool);

DWORD IocpPoolWaitTask(IocpPool *pool, const char *task_id)
{
    if (pool == NULL || task_id == NULL)
        return ERROR_INVALID_PARAMETER;

    for (;;) {
        AcquireSRWLockExclusive(pool->lock);
        IocpTask *task = FindTask(pool, task_id);
        if (task == NULL) {
            ReleaseSRWLockExclusive(pool->lock);
            SetPoolError(pool, IOCP_POOL_ERROR_TASK_NOT_FOUND);
            return ERROR_NOT_FOUND;
        }
        if (task->operation != NULL && !task->operation->submitted) {
            ReleaseSRWLockExclusive(pool->lock);
            return ERROR_INVALID_STATE;
        }
        StartQueued(pool);
        if (task->status == IOCP_TASK_COMPLETED ||
            (task->status == IOCP_TASK_CANCELED &&
                (FindOperation(pool, task_id) == NULL ||
                    !FindOperation(pool, task_id)->in_flight))) {
            DWORD error = task->WSA_error;
            ReleaseSRWLockExclusive(pool->lock);
            return error;
        }
        ReleaseSRWLockExclusive(pool->lock);

        DWORD error = ProcessCompletion(pool, WaitMilliseconds(pool));
        if (error != ERROR_SUCCESS) {
            SetPoolError(pool, IOCP_POOL_ERROR_IO);
            return error;
        }
    }
}

typedef struct {
    int queued;
} QueuedContext;

static int FindQueuedEntry(const char *key, const Value *value, void *context)
{
    (void)key;
    IocpTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    int runnable = task == NULL || task->operation == NULL ||
        (task->operation->submitted &&
            IocpTaskListGet(&task->operation->tasks,
                task->operation->next_task) == task);
    if (task != NULL && task->status == IOCP_TASK_QUEUED && runnable)
        ((QueuedContext *)context)->queued = 1;
    return 0;
}

DWORD IocpPoolWaitTasks(IocpPool *pool)
{
    if (pool == NULL)
        return ERROR_INVALID_PARAMETER;

    for (;;) {
        AcquireSRWLockExclusive(pool->lock);
        StartQueued(pool);
        QueuedContext queued = { 0 };
        HashMap_RemoveIf(pool->tasks, FindQueuedEntry, &queued);
        uint32_t running = atomic_load(&pool->running);
        ReleaseSRWLockExclusive(pool->lock);

        if (running == 0 && !queued.queued)
            return ERROR_SUCCESS;
        DWORD error = ProcessCompletion(pool, WaitMilliseconds(pool));
        if (error != ERROR_SUCCESS) {
            SetPoolError(pool, IOCP_POOL_ERROR_IO);
            return error;
        }
    }
}

typedef struct {
    IocpPool *pool;
} CancelAllContext;

static int CancelAllEntry(const char *key, const Value *value, void *context)
{
    CancelAllContext *cancel = context;
    IocpTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    if (task == NULL || task->status == IOCP_TASK_COMPLETED ||
        task->status == IOCP_TASK_CANCELED)
        return 0;

    IocpPendingIo *operation = FindOperation(cancel->pool, key);
    task->status = IOCP_TASK_CANCELED;
    task->error = IOCP_POOL_ERROR_CANCELED;
    task->WSA_error = WSA_OPERATION_ABORTED;
    if (operation == NULL || !operation->in_flight) {
        RemoveOperation(cancel->pool, operation);
    } else {
        operation->cancel_requested = TRUE;
        if (task->options.kind == IOCP_TASK_CONNECT) {
            CloseSocket(operation->socket);
        } else if (operation->socket != NULL) {
            CancelIoEx((HANDLE)operation->socket->handle,
                &operation->overlapped);
        }
    }
    return 0;
}

static int FreeTaskEntry(const char *key, const Value *value, void *context)
{
    (void)key;
    (void)context;
    IocpTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    IocpTaskDestroy(task);
    return 1;
}

static int FreeOperationEntry(const char *key, const Value *value,
    void *context)
{
    (void)key;
    (void)context;
    free(value->kind == VALUE_POINTER ? value->ptr : NULL);
    return 1;
}

static int FreeSocketEntry(const char *key, const Value *value, void *context)
{
    (void)key;
    (void)context;
    IocpSocket *socket = value->kind == VALUE_POINTER ? value->ptr : NULL;
    if (socket != NULL) {
        CloseSocket(socket);
        free(socket);
    }
    return 1;
}

static int FreePoolOperationEntry(const char *key, const Value *value,
    void *context)
{
    (void)key;
    (void)context;
    IocpOperationDestroy(value->kind == VALUE_POINTER ? value->ptr : NULL);
    return 1;
}

void IocpPoolDestroy(IocpPool *pool)
{
    if (pool == NULL)
        return;

    AcquireSRWLockExclusive(pool->lock);
    CancelAllContext context = { pool };
    HashMap_RemoveIf(pool->tasks, CancelAllEntry, &context);
    ReleaseSRWLockExclusive(pool->lock);

    while (atomic_load(&pool->running) != 0)
        ProcessCompletion(pool, INFINITE);

    HashMap_RemoveIf(pool->sockets, FreeSocketEntry, NULL);
    HashMap_RemoveIf(pool->tasks, FreeTaskEntry, NULL);
    HashMap_RemoveIf(pool->overlapped, FreeOperationEntry, NULL);
    HashMap_RemoveIf(pool->operations, FreePoolOperationEntry, NULL);
    HashMap_Free(pool->sockets);
    HashMap_Free(pool->tasks);
    HashMap_Free(pool->overlapped);
    HashMap_Free(pool->operations);
    if (pool->completion_port != NULL)
        CloseHandle(pool->completion_port);
    free(pool->lock);
    WSACleanup();
    free(pool);
}
