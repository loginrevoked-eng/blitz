#include "io-async.h"

#include <windows.h>
#include <mswsock.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
    OVERLAPPED overlapped;
    SOCKET socket;
    size_t result_index;
    int pending;
    int timed_out;
} ConnectAttempt;

static IoAsyncTcpState StateForSocketError(int error)
{
    if (error == 0)
        return IO_ASYNC_TCP_OPEN;
    if (error == WSAECONNREFUSED)
        return IO_ASYNC_TCP_CLOSED;
    if (error == WSAETIMEDOUT || error == WSA_OPERATION_ABORTED)
        return IO_ASYNC_TCP_TIMEOUT;
    return IO_ASYNC_TCP_ERROR;
}

static void CloseAttemptSocket(ConnectAttempt *attempt)
{
    if (attempt->socket != INVALID_SOCKET) {
        closesocket(attempt->socket);
        attempt->socket = INVALID_SOCKET;
    }
}

static void CancelPending(ConnectAttempt *attempts, size_t count,
    size_t *pending_count, HANDLE completion_port, IoAsyncTcpResult *results)
{
    for (size_t i = 0; i < count; ++i) {
        if (!attempts[i].pending)
            continue;
        attempts[i].timed_out = 1;
        CloseAttemptSocket(&attempts[i]);
    }

    while (*pending_count != 0) {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED *overlapped = NULL;
        BOOL completed = GetQueuedCompletionStatus(completion_port, &bytes,
            &key, &overlapped, INFINITE);
        if (overlapped == NULL)
            continue;

        ConnectAttempt *attempt = (ConnectAttempt *)key;
        if (attempt == NULL || !attempt->pending)
            continue;

        IoAsyncTcpResult *result = &results[attempt->result_index];
        result->state = attempt->timed_out ? IO_ASYNC_TCP_TIMEOUT :
            StateForSocketError(completed ? 0 : (int)GetLastError());
        result->error = attempt->timed_out ? WSAETIMEDOUT :
            (completed ? 0 : (int)GetLastError());
        attempt->pending = 0;
        --*pending_count;
        CloseAttemptSocket(attempt);
    }
}

int IoIocpTcpProbeBatch(uint32_t address, const uint16_t *ports,
    size_t port_count, DWORD timeout_ms, IoAsyncTcpResult *results,
    int *batch_error)
{
    if (batch_error != NULL)
        *batch_error = 0;
    if ((port_count != 0 && (ports == NULL || results == NULL)) ||
        timeout_ms == 0) {
        if (batch_error != NULL)
            *batch_error = WSAEINVAL;
        return 0;
    }
    if (port_count == 0)
        return 1;

    ConnectAttempt *attempts = calloc(port_count, sizeof(*attempts));
    if (attempts == NULL) {
        if (batch_error != NULL)
            *batch_error = WSAENOBUFS;
        return 0;
    }
    for (size_t i = 0; i < port_count; ++i)
        attempts[i].socket = INVALID_SOCKET;

    HANDLE completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
        NULL, 0, 1);
    if (completion_port == NULL) {
        if (batch_error != NULL)
            *batch_error = (int)GetLastError();
        free(attempts);
        return 0;
    }

    SOCKET lookup_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (lookup_socket == INVALID_SOCKET) {
        if (batch_error != NULL)
            *batch_error = WSAGetLastError();
        CloseHandle(completion_port);
        free(attempts);
        return 0;
    }

    GUID connect_ex_id = WSAID_CONNECTEX;
    LPFN_CONNECTEX connect_ex = NULL;
    DWORD bytes_returned = 0;
    if (WSAIoctl(lookup_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
            &connect_ex_id, sizeof(connect_ex_id), &connect_ex,
            sizeof(connect_ex), &bytes_returned, NULL, NULL) == SOCKET_ERROR) {
        if (batch_error != NULL)
            *batch_error = WSAGetLastError();
        closesocket(lookup_socket);
        CloseHandle(completion_port);
        free(attempts);
        return 0;
    }
    closesocket(lookup_socket);

    struct sockaddr_in target = { 0 };
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = htonl(address);

    struct sockaddr_in local = { 0 };
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    uint64_t deadline = GetTickCount64() + timeout_ms;
    size_t pending_count = 0;
    for (size_t i = 0; i < port_count; ++i) {
        results[i].port = ports[i];
        results[i].state = IO_ASYNC_TCP_ERROR;
        results[i].error = 0;
        attempts[i].result_index = i;

        SOCKET socket_handle = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
            NULL, 0, WSA_FLAG_OVERLAPPED);
        if (socket_handle == INVALID_SOCKET) {
            results[i].error = WSAGetLastError();
            continue;
        }
        attempts[i].socket = socket_handle;

        if (bind(socket_handle, (struct sockaddr *)&local,
                sizeof(local)) == SOCKET_ERROR) {
            results[i].error = WSAGetLastError();
            CloseAttemptSocket(&attempts[i]);
            continue;
        }
        if (CreateIoCompletionPort((HANDLE)socket_handle, completion_port,
                (ULONG_PTR)&attempts[i], 0) == NULL) {
            results[i].error = (int)GetLastError();
            CloseAttemptSocket(&attempts[i]);
            continue;
        }

        target.sin_port = htons(ports[i]);
        memset(&attempts[i].overlapped, 0, sizeof(attempts[i].overlapped));
        BOOL started = connect_ex(socket_handle,
            (struct sockaddr *)&target, sizeof(target), NULL, 0, NULL,
            &attempts[i].overlapped);
        if (!started) {
            int error = WSAGetLastError();
            if (error != WSA_IO_PENDING) {
                results[i].state = StateForSocketError(error);
                results[i].error = error;
                CloseAttemptSocket(&attempts[i]);
                continue;
            }
        }
        attempts[i].pending = 1;
        ++pending_count;
    }

    while (pending_count != 0) {
        uint64_t now = GetTickCount64();
        if (now >= deadline) {
            CancelPending(attempts, port_count, &pending_count,
                completion_port, results);
            break;
        }

        uint64_t remaining = deadline - now;
        DWORD wait_ms = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
        DWORD transferred = 0;
        ULONG_PTR key = 0;
        OVERLAPPED *overlapped = NULL;
        BOOL completed = GetQueuedCompletionStatus(completion_port,
            &transferred, &key, &overlapped, wait_ms);
        if (overlapped == NULL) {
            DWORD error = GetLastError();
            if (error == WAIT_TIMEOUT)
                CancelPending(attempts, port_count, &pending_count,
                    completion_port, results);
            else {
                if (batch_error != NULL)
                    *batch_error = (int)error;
                CancelPending(attempts, port_count, &pending_count,
                    completion_port, results);
            }
            break;
        }

        ConnectAttempt *attempt = (ConnectAttempt *)key;
        if (attempt == NULL || !attempt->pending)
            continue;
        IoAsyncTcpResult *result = &results[attempt->result_index];
        DWORD error = completed ? 0 : GetLastError();
        result->state = StateForSocketError((int)error);
        result->error = (int)error;
        attempt->pending = 0;
        --pending_count;
        CloseAttemptSocket(attempt);
    }

    CloseHandle(completion_port);
    free(attempts);
    return 1;
}
