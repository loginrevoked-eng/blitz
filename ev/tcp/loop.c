#include "models.h"
#include <stdlib.h>
#include <string.h>

static SRWLOCK loop_instance_lock = SRWLOCK_INIT;
static TcpConnectorLoop *loop_instance;

TcpConnectorLoop *TcpConnectorGetLoop(void)
{
    AcquireSRWLockShared(&loop_instance_lock);
    TcpConnectorLoop *loop = loop_instance;
    ReleaseSRWLockShared(&loop_instance_lock);
    return loop;
}

BOOL TcpConnectorSetLoop(TcpConnectorLoop *loop)
{
    AcquireSRWLockExclusive(&loop_instance_lock);
    BOOL available = loop_instance == NULL;
    if (available)
        loop_instance = loop;
    ReleaseSRWLockExclusive(&loop_instance_lock);
    return available;
}

void TcpConnectorDetachLoop(TcpConnectorLoop *loop)
{
    AcquireSRWLockExclusive(&loop_instance_lock);
    if (loop_instance == loop)
        loop_instance = NULL;
    ReleaseSRWLockExclusive(&loop_instance_lock);
}




static void SetTaskFinished(TcpConnectorLoop *connector, TcpConnectorTask *task,
    DWORD error, TcpConnectorStatus status);

static void FinishSynchronousFailure(TcpConnectorLoop *connector, TcpConnectorTask *task,
    TcpConnectorPendingIo *io_request, DWORD error, BOOL retry_connect)
{
    task->WSA_error = error;
    if (retry_connect && io_request->attempts <= task->options.conn_opt.retries) {
        TcpConnectorStartTask(connector, task);
        return;
    }

    SetTaskFinished(connector, task, error, TCP_CONNECTOR_TASK_COMPLETED);
    RemovePendingIo(connector, io_request);
}

static TcpConnectorPendingIo *GetOrCreatePendingIo(TcpConnectorLoop *connector, TcpConnectorTask *task)
{
    TcpConnectorPendingIo *io_request = TcpConnectorFindPendingIo(connector, task->id);
    if (io_request != NULL)
        return io_request;

    io_request = calloc(1, sizeof(*io_request));
    if (io_request == NULL)
        return NULL;
    io_request->task_id = task->id;
    if (!HashMap_Set(connector->lookup.pending_io, task->id, PointerValue(io_request))) {
        free(io_request);
        return NULL;
    }
    return io_request;
}

static int StartConnectTask(TcpConnectorLoop *connector, TcpConnectorTask *task,
    TcpConnectorPendingIo *io_request)
{
    ++io_request->attempts;
    io_request->socket = NULL;
    io_request->cancel_requested = FALSE;
    io_request->timed_out = FALSE;
    io_request->has_deadline = TRUE;
    memset(&io_request->overlapped, 0, sizeof(io_request->overlapped));

    TcpConnectorSocket *socket = calloc(1, sizeof(*socket));
    if (socket == NULL) {
        FinishSynchronousFailure(connector, task, io_request,
            TCP_CONNECTOR_OUT_OF_MEMORY, TRUE);
        return 0;
    }
    size_t id_length = strlen(task->id) + 1;
    socket->id = malloc(id_length);
    if (socket->id == NULL) {
        free(socket);
        FinishSynchronousFailure(connector, task, io_request,
            TCP_CONNECTOR_OUT_OF_MEMORY, TRUE);
        return 0;
    }
    memcpy(socket->id, task->id, id_length);
    socket->handle = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket->handle == INVALID_SOCKET) {
        DWORD error = (DWORD)WSAGetLastError();
        CloseSocket(socket);
        free(socket);
        FinishSynchronousFailure(connector, task, io_request, error, TRUE);
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
        FinishSynchronousFailure(connector, task, io_request, error, TRUE);
        return 0;
    }

    if (CreateIoCompletionPort((HANDLE)socket->handle, connector->internal.completion_port,
            (ULONG_PTR)socket, 0) == NULL) {
        DWORD error = GetLastError();
        CloseSocket(socket);
        free(socket);
        FinishSynchronousFailure(connector, task, io_request, error, TRUE);
        return 0;
    }

    if (!HashMap_Set(connector->lookup.sockets, task->id, PointerValue(socket))) {
        CloseSocket(socket);
        free(socket);
        FinishSynchronousFailure(connector, task, io_request,
            TCP_CONNECTOR_OUT_OF_MEMORY, TRUE);
        return 0;
    }

    struct sockaddr_in remote = { 0 };
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(task->options.conn_opt.addr);
    remote.sin_port = htons(task->options.conn_opt.port);

    io_request->socket = socket;
    io_request->deadline = GetTickCount64() +
        task->options.conn_opt.timeout_ms;
    io_request->in_flight = TRUE;
    socket->pending_tasks++;
    task->status = TCP_CONNECTOR_TASK_PENDING;
    task->WSA_error = TCP_CONNECTOR_IO_PENDING;
    atomic_fetch_add(&connector->control.running, 1);

    BOOL started = connector->internal.connect_ex(socket->handle,
        (struct sockaddr *)&remote, sizeof(remote), NULL, 0, NULL,
        &io_request->overlapped);
    if (!started && WSAGetLastError() != WSA_IO_PENDING) {
        DWORD error = (DWORD)WSAGetLastError();
        HashMap_Remove(connector->lookup.sockets, task->id);
        CloseSocket(socket);
        if (socket->pending_tasks > 0)
            --socket->pending_tasks;
        free(socket);
        io_request->socket = NULL;
        io_request->in_flight = FALSE;
        atomic_fetch_sub(&connector->control.running, 1);
        WakeConditionVariable(&connector->control.slot_available);
        FinishSynchronousFailure(connector, task, io_request, error, TRUE);
        return 0;
    }
    return 1;
}

static int StartSocketTask(TcpConnectorLoop *connector, TcpConnectorTask *task,
    TcpConnectorPendingIo *io_request)
{
    TcpConnectorSROption *options = &task->options.sr_opt;
    TcpConnectorSocket *socket = FindSocket(connector, options->socket_id);
    if (socket == NULL || !socket->connected ||
        socket->handle == INVALID_SOCKET) {
        FinishSynchronousFailure(connector, task, io_request, WSAENOTCONN, FALSE);
        return 0;
    }

    ByteArray *payload = options->payload;
    if (payload == NULL || payload->buffer == NULL) {
        FinishSynchronousFailure(connector, task, io_request, WSAEINVAL, FALSE);
        return 0;
    }

    uint32_t length = task->options.kind == TCP_CONNECTOR_TASK_SEND
        ? payload->len : payload->cap;
    if (length == 0) {
        FinishSynchronousFailure(connector, task, io_request, WSAEINVAL, FALSE);
        return 0;
    }

    io_request->socket = socket;
    io_request->cancel_requested = FALSE;
    io_request->timed_out = FALSE;
    io_request->has_deadline = task->options.kind == TCP_CONNECTOR_TASK_RECV &&
        options->timeout_ms != 0;
    if (io_request->has_deadline)
        io_request->deadline = GetTickCount64() + options->timeout_ms;
    io_request->in_flight = TRUE;
    io_request->buffer.buf = (CHAR *)payload->buffer;
    io_request->buffer.len = length;
    memset(&io_request->overlapped, 0, sizeof(io_request->overlapped));
    if (task->options.kind == TCP_CONNECTOR_TASK_RECV)
        payload->len = 0;

    task->status = TCP_CONNECTOR_TASK_PENDING;
    task->WSA_error = TCP_CONNECTOR_IO_PENDING;
    socket->pending_tasks++;
    atomic_fetch_add(&connector->control.running, 1);

    DWORD flags = 0;
    int result;
    if (task->options.kind == TCP_CONNECTOR_TASK_SEND) {
        result = WSASend(socket->handle, &io_request->buffer, 1, NULL,
            0, &io_request->overlapped, NULL);
    } else {
        result = WSARecv(socket->handle, &io_request->buffer, 1, NULL,
            &flags, &io_request->overlapped, NULL);
    }

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        DWORD error = (DWORD)WSAGetLastError();
        io_request->in_flight = FALSE;
        io_request->socket = NULL;
        socket->pending_tasks--;
        atomic_fetch_sub(&connector->control.running, 1);
        WakeConditionVariable(&connector->control.slot_available);
        FinishSynchronousFailure(connector, task, io_request, error, FALSE);
        return 0;
    }
    return 1;
}

int TcpConnectorStartTask(TcpConnectorLoop *connector, TcpConnectorTask *task)
{
    TcpConnectorPendingIo *io_request = GetOrCreatePendingIo(connector, task);
    if (io_request == NULL) {
        SetTaskFinished(connector, task, TCP_CONNECTOR_OUT_OF_MEMORY,
            TCP_CONNECTOR_TASK_COMPLETED);
            return 0;
    }

    if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT)
        return StartConnectTask(connector, task, io_request);
    return StartSocketTask(connector, task, io_request);
}


static void SetTaskFinished(TcpConnectorLoop *connector, TcpConnectorTask *task,
    DWORD error, TcpConnectorStatus status)
{
    task->status = status;
    task->WSA_error = error;
    TcpConnectorReportCompletion(connector, task);
    TcpConnectorTaskReadySubmitCallback(connector, task);
}


static void CancelExpired(TcpConnectorLoop *connector)
{
    ULONGLONG now = GetTickCount64();
    HashMapIter iter = { 0 };
    for (Value *value = HashMap_IterNext(connector->lookup.pending_io, &iter);
         value != NULL; value = HashMap_IterNext(connector->lookup.pending_io, &iter)) {
        TcpConnectorPendingIo *io = value->kind == VALUE_POINTER ? value->ptr : NULL;
        if (io == NULL || !io->in_flight || !io->has_deadline ||
            io->cancel_requested || io->timed_out || now < io->deadline)
            continue;
        TcpConnectorTask *task = TcpConnectorFindTask(connector, HashMap_IterKey(&iter));
        if (task == NULL || (task->options.kind != TCP_CONNECTOR_TASK_CONNECT &&
            !(task->options.kind == TCP_CONNECTOR_TASK_RECV &&
                task->options.sr_opt.timeout_ms != 0)))
            continue;
        io->timed_out = TRUE;
        SetTaskFinished(connector, task, WSAETIMEDOUT, TCP_CONNECTOR_TASK_CANCELED);
        CloseSocket(io->socket);
    }
}

DWORD TcpConnectorHandleCompletion(TcpConnectorLoop *connector, DWORD wait_ms)
{
    DWORD transferred = 0;
    ULONG_PTR key = 0;
    OVERLAPPED *overlapped = NULL;
    BOOL success = GetQueuedCompletionStatus(connector->internal.completion_port,
        &transferred, &key, &overlapped, wait_ms);
    DWORD completion_error = success ? TCP_CONNECTOR_SUCCESS : GetLastError();

    if (overlapped == NULL) {
        if (success && key == TCP_CONNECTOR_IO_COMPLETION_WORKER_WAKE_KEY)
            return TCP_CONNECTOR_SUCCESS;
        if (!success && completion_error == WAIT_TIMEOUT) {
            AcquireSRWLockExclusive(connector->control.lock);
            CancelExpired(connector);
            ReleaseSRWLockExclusive(connector->control.lock);
            return TCP_CONNECTOR_SUCCESS;
        }
        return success ? TCP_CONNECTOR_INVALID_DATA : completion_error;
    }

    TcpConnectorPendingIo *io_request = (TcpConnectorPendingIo *)overlapped;
    TcpConnectorTask *task = NULL;

    AcquireSRWLockExclusive(connector->control.lock);
    task = TcpConnectorFindTask(connector, io_request->task_id);
    if (task == NULL || TcpConnectorFindPendingIo(connector, io_request->task_id) != io_request ||
        !io_request->in_flight || (TcpConnectorSocket *)key != io_request->socket) {
        ReleaseSRWLockExclusive(connector->control.lock);
        SetConnectorError(connector, TCP_CONNECTOR_INVALID_DATA);
        return TCP_CONNECTOR_INVALID_DATA;
    }

    TcpConnectorSocket *socket = io_request->socket;
    io_request->in_flight = FALSE;
    if (socket != NULL && socket->pending_tasks > 0)
        --socket->pending_tasks;
    atomic_fetch_sub(&connector->control.running, 1);
    WakeConditionVariable(&connector->control.slot_available);
    WakeAllConditionVariable(&connector->control.task_finished);

    if (io_request->timed_out)
        completion_error = WSAETIMEDOUT;

    if (io_request->cancel_requested) {
        SetTaskFinished(connector, task, WSA_OPERATION_ABORTED, TCP_CONNECTOR_TASK_CANCELED);
        if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT && socket != NULL) {
            HashMap_Remove(connector->lookup.sockets, task->id);
            CloseSocket(socket);
            free(socket);
            io_request->socket = NULL;
        }
        RemovePendingIo(connector, io_request);
    } else if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT) {
        if (completion_error == TCP_CONNECTOR_SUCCESS) {
            if (setsockopt(socket->handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,
                    NULL, 0) == SOCKET_ERROR)
                completion_error = (DWORD)WSAGetLastError();
        }

        if (completion_error == TCP_CONNECTOR_SUCCESS) {
            socket->connected = TRUE;
            SetTaskFinished(connector, task, TCP_CONNECTOR_SUCCESS, TCP_CONNECTOR_TASK_COMPLETED);
            RemovePendingIo(connector, io_request);
        } else {
            HashMap_Remove(connector->lookup.sockets, task->id);
            CloseSocket(socket);
            free(socket);
            io_request->socket = NULL;
            if (!io_request->timed_out &&
                io_request->attempts <= task->options.conn_opt.retries) {
                task->WSA_error = completion_error;
                TcpConnectorStartTask(connector, task);
            } else if (io_request->timed_out &&
                io_request->attempts <= task->options.conn_opt.retries) {
                task->WSA_error = WSAETIMEDOUT;
                TcpConnectorStartTask(connector, task);
            } else {
                SetTaskFinished(connector, task, completion_error, TCP_CONNECTOR_TASK_COMPLETED);
                RemovePendingIo(connector, io_request);
            }
        }
    } else {
        if (task->options.kind == TCP_CONNECTOR_TASK_RECV && completion_error == TCP_CONNECTOR_SUCCESS)
            task->options.sr_opt.payload->len = transferred;
        SetTaskFinished(connector, task, completion_error, TCP_CONNECTOR_TASK_COMPLETED);
        RemovePendingIo(connector, io_request);
    }

    ReleaseSRWLockExclusive(connector->control.lock);
    return TCP_CONNECTOR_SUCCESS;
}



