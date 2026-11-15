#include "iocp-pool.h"

#include <mswsock.h>
#include <stdlib.h>
#include <string.h>

typedef struct IocpTaskNode {
    IocpProbeTask task;
    OVERLAPPED overlapped;
    SOCKET socket;
    uint64_t deadline;
    int timed_out;
    struct IocpTaskNode *next;
} IocpTaskNode;

struct IocpPool {
    HANDLE completion_port;
    LPFN_CONNECTEX connect_ex;
    IocpTaskNode *tasks;
    uint64_t next_id;
    uint32_t max_pending;
    uint32_t pending;
    uint32_t default_timeout_ms;
    int running;
};

static void SetError(DWORD *error, DWORD value)
{
    if (error != NULL)
        *error = value;
}

static IocpTaskNode *FindTask(const IocpPool *pool, uint64_t task_id)
{
    for (IocpTaskNode *node = pool->tasks; node != NULL; node = node->next) {
        if (node->task.id == task_id)
            return node;
    }
    return NULL;
}

static void CloseTaskSocket(IocpTaskNode *node)
{
    if (node->socket != INVALID_SOCKET) {
        closesocket(node->socket);
        node->socket = INVALID_SOCKET;
    }
}

static int IsValidOptions(const IocpProbeOption *options)
{
    return options != NULL && options->port != 0 &&
        options->probe.addr <= UINT32_MAX;
}

static void CompleteTask(IocpPool *pool, IocpTaskNode *node, DWORD error)
{
    node->task.status = IOCP_PROBE_TASK_COMPLETED;
    node->task.error = node->timed_out ? WSAETIMEDOUT : error;
    node->timed_out = 0;
    node->deadline = 0;
    CloseTaskSocket(node);
    if (pool->pending != 0)
        --pool->pending;
}

static int StartTask(IocpPool *pool, IocpTaskNode *node, DWORD *error)
{
    struct sockaddr_in local = { 0 };
    struct sockaddr_in target = { 0 };
    DWORD timeout_ms = node->task.options.probe.timeout_ms;

    if (timeout_ms == 0)
        timeout_ms = pool->default_timeout_ms;

    node->socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (node->socket == INVALID_SOCKET) {
        SetError(error, (DWORD)WSAGetLastError());
        return 0;
    }

    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(node->socket, (struct sockaddr *)&local, sizeof(local))
            == SOCKET_ERROR) {
        SetError(error, (DWORD)WSAGetLastError());
        CloseTaskSocket(node);
        return 0;
    }

    if (CreateIoCompletionPort((HANDLE)node->socket, pool->completion_port,
            (ULONG_PTR)node, 0) == NULL) {
        SetError(error, GetLastError());
        CloseTaskSocket(node);
        return 0;
    }

    target.sin_family = AF_INET;
    target.sin_port = htons(node->task.options.port);
    target.sin_addr.s_addr = htonl((uint32_t)node->task.options.probe.addr);
    memset(&node->overlapped, 0, sizeof(node->overlapped));

    BOOL started = pool->connect_ex(node->socket,
        (struct sockaddr *)&target, sizeof(target), NULL, 0, NULL,
        &node->overlapped);
    if (!started) {
        DWORD connect_error = (DWORD)WSAGetLastError();
        if (connect_error != WSA_IO_PENDING) {
            SetError(error, connect_error);
            CloseTaskSocket(node);
            return 0;
        }
    }

    node->task.status = IOCP_PROBE_TASK_PENDING;
    node->task.error = 0;
    node->deadline = GetTickCount64() + timeout_ms;
    ++pool->pending;
    return 1;
}

static void StartQueuedTasks(IocpPool *pool)
{
    for (IocpTaskNode *node = pool->tasks;
            node != NULL && pool->pending < pool->max_pending;
            node = node->next) {
        if (node->task.status != IOCP_PROBE_TASK_QUEUED)
            continue;

        DWORD error = 0;
        if (!StartTask(pool, node, &error)) {
            node->task.status = IOCP_PROBE_TASK_COMPLETED;
            node->task.error = error;
        }
    }
}

static uint64_t NearestDeadline(const IocpPool *pool)
{
    uint64_t deadline = UINT64_MAX;
    for (IocpTaskNode *node = pool->tasks; node != NULL; node = node->next) {
        if (node->task.status == IOCP_PROBE_TASK_PENDING &&
            node->deadline < deadline)
            deadline = node->deadline;
    }
    return deadline;
}

static void CancelExpiredTasks(IocpPool *pool, uint64_t now)
{
    for (IocpTaskNode *node = pool->tasks; node != NULL; node = node->next) {
        if (node->task.status != IOCP_PROBE_TASK_PENDING ||
            node->deadline > now)
            continue;
        node->timed_out = 1;
        CloseTaskSocket(node);
    }
}

static void CancelAllPending(IocpPool *pool)
{
    for (IocpTaskNode *node = pool->tasks; node != NULL; node = node->next) {
        if (node->task.status == IOCP_PROBE_TASK_PENDING) {
            node->timed_out = 1;
            CloseTaskSocket(node);
        }
    }
}

IocpPool *IocpPoolCreate(uint32_t max_pending, uint32_t default_timeout_ms,
    DWORD *error)
{
    SetError(error, 0);
    if (max_pending == 0 || default_timeout_ms == 0) {
        SetError(error, WSAEINVAL);
        return NULL;
    }

    WSADATA winsock_data;
    int startup_error = WSAStartup(MAKEWORD(2, 2), &winsock_data);
    if (startup_error != 0) {
        SetError(error, (DWORD)startup_error);
        return NULL;
    }

    IocpPool *pool = calloc(1, sizeof(*pool));
    if (pool == NULL) {
        SetError(error, WSAENOBUFS);
        WSACleanup();
        return NULL;
    }
    pool->completion_port = NULL;
    pool->next_id = 1;
    pool->max_pending = max_pending;
    pool->default_timeout_ms = default_timeout_ms;

    pool->completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
        NULL, 0, 0);
    if (pool->completion_port == NULL) {
        SetError(error, GetLastError());
        free(pool);
        WSACleanup();
        return NULL;
    }

    SOCKET lookup_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (lookup_socket == INVALID_SOCKET) {
        SetError(error, (DWORD)WSAGetLastError());
        CloseHandle(pool->completion_port);
        free(pool);
        WSACleanup();
        return NULL;
    }

    GUID connect_ex_id = WSAID_CONNECTEX;
    DWORD bytes_returned = 0;
    if (WSAIoctl(lookup_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
            &connect_ex_id, sizeof(connect_ex_id), &pool->connect_ex,
            sizeof(pool->connect_ex), &bytes_returned, NULL, NULL)
            == SOCKET_ERROR) {
        SetError(error, (DWORD)WSAGetLastError());
        closesocket(lookup_socket);
        CloseHandle(pool->completion_port);
        free(pool);
        WSACleanup();
        return NULL;
    }
    closesocket(lookup_socket);
    return pool;
}

void IocpPoolDestroy(IocpPool *pool)
{
    if (pool == NULL)
        return;

    /* Run is synchronous, so no overlapped operations remain here. */
    IocpTaskNode *node = pool->tasks;
    while (node != NULL) {
        IocpTaskNode *next = node->next;
        CloseTaskSocket(node);
        free(node);
        node = next;
    }
    if (pool->completion_port != NULL)
        CloseHandle(pool->completion_port);
    free(pool);
    WSACleanup();
}

int IocpPoolAdd(IocpPool *pool, const IocpProbeOption *options,
    uint64_t *task_id, DWORD *error)
{
    SetError(error, 0);
    if (pool == NULL || !IsValidOptions(options) || task_id == NULL) {
        SetError(error, WSAEINVAL);
        return 0;
    }
    if (pool->running) {
        SetError(error, WSAEINPROGRESS);
        return 0;
    }

    IocpTaskNode *node = calloc(1, sizeof(*node));
    if (node == NULL) {
        SetError(error, WSAENOBUFS);
        return 0;
    }
    if (pool->next_id == 0) {
        free(node);
        SetError(error, WSAENOBUFS);
        return 0;
    }

    node->socket = INVALID_SOCKET;
    node->task.id = pool->next_id++;
    node->task.options = *options;
    node->task.status = IOCP_PROBE_TASK_QUEUED;
    node->next = pool->tasks;
    pool->tasks = node;
    *task_id = node->task.id;
    return 1;
}

int IocpPoolGet(const IocpPool *pool, uint64_t task_id, IocpProbeTask *task)
{
    if (pool == NULL || task == NULL)
        return 0;
    IocpTaskNode *node = FindTask(pool, task_id);
    if (node == NULL)
        return 0;
    *task = node->task;
    return 1;
}

int IocpPoolUpdate(IocpPool *pool, uint64_t task_id,
    const IocpProbeOption *options, DWORD *error)
{
    SetError(error, 0);
    if (pool == NULL || !IsValidOptions(options)) {
        SetError(error, WSAEINVAL);
        return 0;
    }
    if (pool->running) {
        SetError(error, WSAEINPROGRESS);
        return 0;
    }

    IocpTaskNode *node = FindTask(pool, task_id);
    if (node == NULL) {
        SetError(error, WSA_INVALID_HANDLE);
        return 0;
    }
    if (node->task.status == IOCP_PROBE_TASK_PENDING) {
        SetError(error, WSAEINPROGRESS);
        return 0;
    }
    node->task.options = *options;
    node->task.status = IOCP_PROBE_TASK_QUEUED;
    node->task.error = 0;
    return 1;
}

int IocpPoolRemove(IocpPool *pool, uint64_t task_id, DWORD *error)
{
    SetError(error, 0);
    if (pool == NULL) {
        SetError(error, WSAEINVAL);
        return 0;
    }
    if (pool->running) {
        SetError(error, WSAEINPROGRESS);
        return 0;
    }

    IocpTaskNode **link = &pool->tasks;
    while (*link != NULL && (*link)->task.id != task_id)
        link = &(*link)->next;
    if (*link == NULL) {
        SetError(error, WSA_INVALID_HANDLE);
        return 0;
    }
    if ((*link)->task.status == IOCP_PROBE_TASK_PENDING) {
        SetError(error, WSAEINPROGRESS);
        return 0;
    }

    IocpTaskNode *removed = *link;
    *link = removed->next;
    free(removed);
    return 1;
}

int IocpPoolRun(IocpPool *pool, DWORD *error)
{
    SetError(error, 0);
    if (pool == NULL) {
        SetError(error, WSAEINVAL);
        return 0;
    }
    if (pool->running) {
        SetError(error, WSAEINPROGRESS);
        return 0;
    }

    pool->running = 1;
    DWORD run_error = 0;
    for (;;) {
        StartQueuedTasks(pool);

        int queued = 0;
        for (IocpTaskNode *node = pool->tasks; node != NULL;
                node = node->next) {
            if (node->task.status == IOCP_PROBE_TASK_QUEUED) {
                queued = 1;
                break;
            }
        }
        if (pool->pending == 0 && !queued)
            break;

        uint64_t now = GetTickCount64();
        CancelExpiredTasks(pool, now);
        DWORD wait_ms = INFINITE;
        uint64_t deadline = NearestDeadline(pool);
        if (deadline != UINT64_MAX) {
            now = GetTickCount64();
            uint64_t remaining = deadline > now ? deadline - now : 0;
            wait_ms = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
        }

        DWORD transferred = 0;
        ULONG_PTR key = 0;
        OVERLAPPED *overlapped = NULL;
        BOOL completed = GetQueuedCompletionStatus(pool->completion_port,
            &transferred, &key, &overlapped, wait_ms);
        if (overlapped == NULL) {
            DWORD wait_error = GetLastError();
            if (wait_error == WAIT_TIMEOUT)
                continue;
            if (run_error == 0)
                run_error = wait_error;
            CancelAllPending(pool);
            continue;
        }

        IocpTaskNode *node = (IocpTaskNode *)key;
        if (node == NULL || node->task.status != IOCP_PROBE_TASK_PENDING ||
            overlapped != &node->overlapped) {
            if (run_error == 0)
                run_error = ERROR_INVALID_DATA;
            continue;
        }
        DWORD completion_error = completed ? 0 : GetLastError();
        CompleteTask(pool, node, completion_error);
    }
    pool->running = 0;
    SetError(error, run_error);
    return run_error == 0;
}
