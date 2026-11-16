#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <mswsock.h>

#include <stdio.h>

#define PORT_COUNT 3
#define TIMEOUT_MS 1500

static const char *host = "127.0.0.1";
static const unsigned short ports[PORT_COUNT] = { 21, 22, 80 };
static SOCKET sockets[PORT_COUNT];
static OVERLAPPED operations[PORT_COUNT];
static BOOL pending[PORT_COUNT];
static BOOL timed_out[PORT_COUNT];
static size_t pending_count;
static HANDLE completion_port;
static LPFN_CONNECTEX connect_ex;
static struct sockaddr_in local;
static struct sockaddr_in remote;

static HANDLE StartCompletionPort(void)
{
    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return NULL;
    }

    HANDLE completion_port = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (completion_port == NULL) {
        fprintf(stderr, "CreateIoCompletionPort failed: %lu\n",
            (unsigned long)GetLastError());
        WSACleanup();
    }
    return completion_port;
}

static LPFN_CONNECTEX GetConnectEx(void)
{
    SOCKET socket_handle = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (socket_handle == INVALID_SOCKET) {
        fprintf(stderr, "WSASocket failed: %d\n", WSAGetLastError());
        return NULL;
    }

    GUID connect_ex_id = WSAID_CONNECTEX;
    LPFN_CONNECTEX connect_ex = NULL;
    DWORD bytes_returned = 0;
    if (WSAIoctl(socket_handle, SIO_GET_EXTENSION_FUNCTION_POINTER,
            &connect_ex_id, sizeof(connect_ex_id), &connect_ex,
            sizeof(connect_ex), &bytes_returned, NULL, NULL) == SOCKET_ERROR) {
        fprintf(stderr, "WSAIoctl failed: %d\n", WSAGetLastError());
        closesocket(socket_handle);
        return NULL;
    }

    closesocket(socket_handle);
    return connect_ex;
}

static int SetAddresses(const char *host, struct sockaddr_in *local,
    struct sockaddr_in *remote)
{
    struct in_addr ipv4;
    if (InetPtonA(AF_INET, host, &ipv4) != 1) {
        fprintf(stderr, "invalid IPv4 address\n");
        return 0;
    }

    *local = (struct sockaddr_in){ 0 };
    local->sin_family = AF_INET;
    local->sin_addr.s_addr = htonl(INADDR_ANY);

    *remote = (struct sockaddr_in){ 0 };
    remote->sin_family = AF_INET;
    remote->sin_addr = ipv4;
    return 1;
}

static void StartConnect(size_t i)
{
    sockets[i] = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (sockets[i] == INVALID_SOCKET) {
        printf("%s:%u error %d\n", host, ports[i], WSAGetLastError());
        return;
    }

    if (bind(sockets[i], (struct sockaddr *)&local, sizeof(local))
            == SOCKET_ERROR) {
        printf("%s:%u bind error %d\n", host, ports[i], WSAGetLastError());
        closesocket(sockets[i]);
        sockets[i] = INVALID_SOCKET;
        return;
    }
    if (CreateIoCompletionPort((HANDLE)sockets[i], completion_port,
            (ULONG_PTR)(i + 1), 0) == NULL) {
        printf("%s:%u IOCP error %lu\n", host, ports[i],
            (unsigned long)GetLastError());
        closesocket(sockets[i]);
        sockets[i] = INVALID_SOCKET;
        return;
    }

    remote.sin_port = htons(ports[i]);
    BOOL started = connect_ex(sockets[i], (struct sockaddr *)&remote,
        sizeof(remote), NULL, 0, NULL, &operations[i]);
    if (!started && WSAGetLastError() != WSA_IO_PENDING) {
        printf("%s:%u error %d\n", host, ports[i], WSAGetLastError());
        closesocket(sockets[i]);
        sockets[i] = INVALID_SOCKET;
        return;
    }
    pending[i] = TRUE;
    ++pending_count;
}

static void CancelPending(void)
{
    for (size_t i = 0; i < PORT_COUNT; ++i) {
        if (!pending[i])
            continue;

        timed_out[i] = TRUE;
        closesocket(sockets[i]);
        sockets[i] = INVALID_SOCKET;
    }
}

static void WaitCompletion(void)
{
    ULONGLONG deadline = GetTickCount64() + TIMEOUT_MS;
    while (pending_count != 0) {
        ULONGLONG now = GetTickCount64();
        if (now >= deadline)
            break;

        DWORD remaining = (DWORD)(deadline - now);
        DWORD transferred = 0;
        ULONG_PTR key = 0;
        OVERLAPPED *operation = NULL;
        BOOL completed = GetQueuedCompletionStatus(completion_port,
            &transferred, &key, &operation, remaining);
        if (operation == NULL)
            break;

        size_t i = (size_t)key - 1;
        if (i >= PORT_COUNT || !pending[i])
            continue;

        DWORD error = completed ? 0 : GetLastError();
        if (error == 0)
            printf("%s:%u open\n", host, ports[i]);
        else if (error == WSAECONNREFUSED)
            printf("%s:%u closed\n", host, ports[i]);
        else
            printf("%s:%u error %lu\n", host, ports[i],
                (unsigned long)error);

        pending[i] = FALSE;
        --pending_count;
        closesocket(sockets[i]);
        sockets[i] = INVALID_SOCKET;
    }
}

static void WaitCancelCompletion(void)
{
    while (pending_count != 0) {
        DWORD transferred = 0;
        ULONG_PTR key = 0;
        OVERLAPPED *operation = NULL;
        GetQueuedCompletionStatus(completion_port, &transferred, &key,
            &operation, INFINITE);
        if (operation == NULL)
            continue;

        size_t i = (size_t)key - 1;
        if (i >= PORT_COUNT || !pending[i])
            continue;
        if (timed_out[i])
            printf("%s:%u timeout\n", host, ports[i]);
        pending[i] = FALSE;
        --pending_count;
    }
}

int main(void)
{
    completion_port = StartCompletionPort();
    if (completion_port == NULL)
        return 1;

    connect_ex = GetConnectEx();
    if (connect_ex == NULL) {
        CloseHandle(completion_port);
        WSACleanup();
        return 1;
    }

    if (!SetAddresses(host, &local, &remote)) {
        CloseHandle(completion_port);
        WSACleanup();
        return 1;
    }

    for (size_t i = 0; i < PORT_COUNT; ++i)
        StartConnect(i);

    WaitCompletion();
    CancelPending();
    WaitCancelCompletion();

    CloseHandle(completion_port);
    WSACleanup();
    return 0;
}
