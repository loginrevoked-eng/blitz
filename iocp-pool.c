#include "iocp-pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOOPBACK_IPV4 0x7F000001ULL
#define DEFAULT_TIMEOUT_MS 1500

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

int IocpPoolInit(IocpPool *pool, uint32_t capacity, uint32_t max_pending)
{
    if (pool == NULL || capacity == 0 || max_pending == 0)
        return 0;

    memset(pool, 0, sizeof(*pool));
    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0)
        return 0;
    pool->winsock_started = TRUE;

    pool->tasks.items = calloc(capacity, sizeof(*pool->tasks.items));
    if (pool->tasks.items == NULL)
        goto fail;
    pool->tasks.cap = capacity;
    pool->max_pending = max_pending;

    pool->completion_port = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (pool->completion_port == NULL)
        goto fail;

    pool->connect_ex = GetConnectEx();
    if (pool->connect_ex == NULL)
        goto fail;

    return 1;

fail:
    if (pool->completion_port != NULL)
        CloseHandle(pool->completion_port);
    free(pool->tasks.items);
    memset(pool, 0, sizeof(*pool));
    WSACleanup();
    return 0;
}

int IocpPoolAdd(IocpPool *pool, IocpProbeOption options)
{
    if (pool == NULL || pool->tasks.items == NULL ||
        pool->tasks.len == pool->tasks.cap || options.port == 0 ||
        options.probe.addr != LOOPBACK_IPV4 || pool->status.pending != 0)
        return 0;

    IocpProbeTask *task = &pool->tasks.items[pool->tasks.len++];
    memset(task, 0, sizeof(*task));
    task->options = options;
    task->socket = INVALID_SOCKET;
    task->status = IOCP_PROBE_TASK_QUEUED;
    return 1;
}

static void RetryOrComplete(IocpProbeTask *task, DWORD error)
{
    task->error = error;
    if (error != 0 && error != WSAECONNREFUSED &&
        task->attempts_started <= task->options.probe.retries) {
        task->timed_out = FALSE;
        task->status = IOCP_PROBE_TASK_QUEUED;
    } else {
        task->status = IOCP_PROBE_TASK_COMPLETED;
    }
}

static void FinishTask(IocpPool *pool, IocpProbeTask *task, DWORD error)
{
    if (task->timed_out)
        error = WSAETIMEDOUT;
    if (task->socket != INVALID_SOCKET) {
        closesocket(task->socket);
        task->socket = INVALID_SOCKET;
    }
    --pool->status.pending;
    RetryOrComplete(task, error);
}

static void StartTask(IocpPool *pool, uint32_t index)
{
    IocpProbeTask *task = &pool->tasks.items[index];
    struct sockaddr_in local = { 0 };
    struct sockaddr_in remote = { 0 };

    ++task->attempts_started;
    task->timed_out = FALSE;
    task->socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (task->socket == INVALID_SOCKET) {
        RetryOrComplete(task, (DWORD)WSAGetLastError());
        return;
    }

    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    remote.sin_port = htons(task->options.port);

    if (bind(task->socket, (struct sockaddr *)&local, sizeof(local))
            == SOCKET_ERROR) {
        DWORD error = (DWORD)WSAGetLastError();
        closesocket(task->socket);
        task->socket = INVALID_SOCKET;
        RetryOrComplete(task, error);
        return;
    }

    if (CreateIoCompletionPort((HANDLE)task->socket, pool->completion_port,
            (ULONG_PTR)(index + 1), 0) == NULL) {
        DWORD error = GetLastError();
        closesocket(task->socket);
        task->socket = INVALID_SOCKET;
        RetryOrComplete(task, error);
        return;
    }

    memset(&task->overlapped, 0, sizeof(task->overlapped));
    task->status = IOCP_PROBE_TASK_PENDING;
    task->deadline_ms = GetTickCount64() +
        (task->options.probe.timeout_ms != 0
            ? task->options.probe.timeout_ms : DEFAULT_TIMEOUT_MS);
    ++pool->status.pending;

    BOOL started = pool->connect_ex(task->socket,
        (struct sockaddr *)&remote, sizeof(remote), NULL, 0, NULL,
        &task->overlapped);
    if (!started && WSAGetLastError() != WSA_IO_PENDING)
        FinishTask(pool, task, (DWORD)WSAGetLastError());
}

static void StartQueued(IocpPool *pool)
{
    while (pool->status.pending < pool->max_pending) {
        uint32_t i;
        for (i = 0; i < pool->tasks.len; ++i) {
            if (pool->tasks.items[i].status == IOCP_PROBE_TASK_QUEUED)
                break;
        }
        if (i == pool->tasks.len)
            return;
        StartTask(pool, i);
    }
}

static int HasQueued(const IocpPool *pool)
{
    for (uint32_t i = 0; i < pool->tasks.len; ++i) {
        if (pool->tasks.items[i].status == IOCP_PROBE_TASK_QUEUED)
            return 1;
    }
    return 0;
}

static DWORD WaitUntilNextDeadline(const IocpPool *pool)
{
    ULONGLONG deadline = ~(ULONGLONG)0;
    for (uint32_t i = 0; i < pool->tasks.len; ++i) {
        const IocpProbeTask *task = &pool->tasks.items[i];
        if (task->status == IOCP_PROBE_TASK_PENDING && !task->timed_out &&
            task->deadline_ms < deadline)
            deadline = task->deadline_ms;
    }
    if (deadline == ~(ULONGLONG)0)
        return INFINITE;

    ULONGLONG now = GetTickCount64();
    if (now >= deadline)
        return 0;
    ULONGLONG remaining = deadline - now;
    return remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
}

static void CancelExpired(IocpPool *pool)
{
    ULONGLONG now = GetTickCount64();
    for (uint32_t i = 0; i < pool->tasks.len; ++i) {
        IocpProbeTask *task = &pool->tasks.items[i];
        if (task->status != IOCP_PROBE_TASK_PENDING || task->timed_out ||
            now < task->deadline_ms)
            continue;

        task->timed_out = TRUE;
        closesocket(task->socket);
        task->socket = INVALID_SOCKET;
    }
}

int IocpPoolRun(IocpPool *pool)
{
    if (pool == NULL || pool->completion_port == NULL ||
        pool->connect_ex == NULL)
        return 0;

    StartQueued(pool);
    while (pool->status.pending != 0 || HasQueued(pool)) {
        DWORD transferred = 0;
        ULONG_PTR key = 0;
        OVERLAPPED *overlapped = NULL;
        BOOL success = GetQueuedCompletionStatus(pool->completion_port,
            &transferred, &key, &overlapped, WaitUntilNextDeadline(pool));

        if (overlapped == NULL) {
            if (!success && GetLastError() == WAIT_TIMEOUT) {
                CancelExpired(pool);
                continue;
            }
            return 0;
        }

        uint32_t index = (uint32_t)key - 1;
        if (index >= pool->tasks.len)
            continue;

        IocpProbeTask *task = &pool->tasks.items[index];
        if (task->status != IOCP_PROBE_TASK_PENDING ||
            overlapped != &task->overlapped)
            continue;

        FinishTask(pool, task, success ? 0 : GetLastError());
        StartQueued(pool);
    }
    return 1;
}

void IocpPoolDestroy(IocpPool *pool)
{
    if (pool == NULL)
        return;

    for (uint32_t i = 0; i < pool->tasks.len; ++i) {
        IocpProbeTask *task = &pool->tasks.items[i];
        if (task->status == IOCP_PROBE_TASK_PENDING &&
            task->socket != INVALID_SOCKET) {
            task->timed_out = TRUE;
            closesocket(task->socket);
            task->socket = INVALID_SOCKET;
        }
    }

    while (pool->status.pending != 0) {
        DWORD transferred = 0;
        ULONG_PTR key = 0;
        OVERLAPPED *overlapped = NULL;
        GetQueuedCompletionStatus(pool->completion_port, &transferred, &key,
            &overlapped, INFINITE);
        if (overlapped == NULL)
            continue;

        uint32_t index = (uint32_t)key - 1;
        if (index < pool->tasks.len &&
            pool->tasks.items[index].status == IOCP_PROBE_TASK_PENDING) {
            IocpProbeTask *task = &pool->tasks.items[index];
            task->error = WSA_OPERATION_ABORTED;
            task->status = IOCP_PROBE_TASK_COMPLETED;
            --pool->status.pending;
        }
    }

    if (pool->completion_port != NULL)
        CloseHandle(pool->completion_port);
    free(pool->tasks.items);
    BOOL winsock_started = pool->winsock_started;
    memset(pool, 0, sizeof(*pool));
    if (winsock_started)
        WSACleanup();
}
