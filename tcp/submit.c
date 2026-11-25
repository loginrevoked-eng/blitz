#include "internal.h"
#include <stdlib.h>
#include <string.h>

void TcpConnectorSetTaskFinished(TcpConnectorLoop *connector, TcpConnectorTask *task,
    DWORD error, TcpConnectorStatus status);

static void FinishSynchronousFailure(TcpConnectorLoop *connector, TcpConnectorTask *task,
    TcpConnectorPendingIo *io_request, DWORD error, BOOL retry_connect)
{
    task->WSA_error = error;
    if (retry_connect && io_request->attempts <= task->options.conn_opt.retries) {
        TcpConnectorStartTask(connector, task);
        return;
    }

    TcpConnectorSetTaskFinished(connector, task, error, TCP_CONNECTOR_TASK_COMPLETED);
    TcpConnectorRemovePendingIo(connector, io_request);
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
    if (!HashMap_Set(connector->lookup.pending_io, task->id, TcpConnectorPointerValue(io_request))) {
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
        TcpConnectorCloseSocket(socket);
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
        TcpConnectorCloseSocket(socket);
        free(socket);
        FinishSynchronousFailure(connector, task, io_request, error, TRUE);
        return 0;
    }

    if (CreateIoCompletionPort((HANDLE)socket->handle, connector->internal.completion_port,
            (ULONG_PTR)socket, 0) == NULL) {
        DWORD error = GetLastError();
        TcpConnectorCloseSocket(socket);
        free(socket);
        FinishSynchronousFailure(connector, task, io_request, error, TRUE);
        return 0;
    }

    if (!HashMap_Set(connector->lookup.sockets, task->id, TcpConnectorPointerValue(socket))) {
        TcpConnectorCloseSocket(socket);
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
        TcpConnectorCloseSocket(socket);
        if (socket->pending_tasks > 0)
            --socket->pending_tasks;
        free(socket);
        io_request->socket = NULL;
        io_request->in_flight = FALSE;
        atomic_fetch_sub(&connector->control.running, 1);
        TcpConnectorSignalSlotAvailable(connector);
        FinishSynchronousFailure(connector, task, io_request, error, TRUE);
        return 0;
    }
    return 1;
}

static int StartSocketTask(TcpConnectorLoop *connector, TcpConnectorTask *task,
    TcpConnectorPendingIo *io_request)
{
    TcpConnectorSROption *options = &task->options.sr_opt;
    TcpConnectorSocket *socket = TcpConnectorFindSocket(connector, options->socket_id);
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
        TcpConnectorSignalSlotAvailable(connector);
        FinishSynchronousFailure(connector, task, io_request, error, FALSE);
        return 0;
    }
    return 1;
}

int TcpConnectorStartTask(TcpConnectorLoop *connector, TcpConnectorTask *task)
{
    TcpConnectorPendingIo *io_request = GetOrCreatePendingIo(connector, task);
    if (io_request == NULL) {
        TcpConnectorSetTaskFinished(connector, task, TCP_CONNECTOR_OUT_OF_MEMORY,
            TCP_CONNECTOR_TASK_COMPLETED);
            return 0;
    }

    if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT)
        return StartConnectTask(connector, task, io_request);
    return StartSocketTask(connector, task, io_request);
}


