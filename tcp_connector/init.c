#include "models.h"
#include <stdlib.h>

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

DWORD TcpConnectorInitEventloop(uint32_t limit, uint32_t flags)
{
    if (limit == 0 || (flags & ~TCP_CONNECTOR_WITH_DRAINER) != 0)
        return ERROR_INVALID_PARAMETER;

    DWORD failure = ERROR_NOT_ENOUGH_MEMORY;
    TcpConnectorLoop *connector = calloc(1, sizeof(*connector));
    if (connector == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    WSADATA winsock_data;
    int wsa_error = WSAStartup(MAKEWORD(2, 2), &winsock_data);
    if (wsa_error != 0) {
        free(connector);
        return (DWORD)wsa_error;
    }

    size_t capacity = (size_t)limit + 16;
    connector->lookup.tasks = HashMap_New(capacity);
    connector->lookup.pending_io = HashMap_New(capacity);
    connector->lookup.sockets = HashMap_New(capacity);
    connector->interf.lock = malloc(sizeof(*connector->interf.lock));
    if (connector->lookup.tasks == NULL || connector->lookup.pending_io == NULL ||
        connector->lookup.sockets == NULL || connector->interf.lock == NULL)
        goto fail_winsock;

    InitializeSRWLock(connector->interf.lock);
    InitializeConditionVariable(&connector->interf.drainer_wake);
    InitializeConditionVariable(&connector->interf.slot_available);
    InitializeConditionVariable(&connector->interf.submitters_done);
    connector->internal.completion_port = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (connector->internal.completion_port == NULL)
        goto fail_winsock;

    connector->internal.connect_ex = GetConnectEx();
    if (connector->internal.connect_ex == NULL)
        goto fail_winsock;

    connector->config.max_pending = limit;
    connector->config.flags = flags;
    atomic_init(&connector->interf.error, TCP_CONNECTOR_SUCCESS);
    atomic_init(&connector->interf.running, 0);
    atomic_init(&connector->interf.drainer_running, FALSE);
    if (!TcpConnectorSetLoop(connector)) {
        failure = ERROR_ALREADY_EXISTS;
        goto fail_winsock;
    }
    if ((flags & TCP_CONNECTOR_WITH_DRAINER) != 0) {
        atomic_store(&connector->interf.drainer_running, TRUE);
        connector->interf.drainer_thread = CreateThread(NULL, 0,
            TcpConnectorDrainerThread, NULL, 0, NULL);
        if (connector->interf.drainer_thread == NULL) {
            failure = GetLastError();
            atomic_store(&connector->interf.drainer_running, FALSE);
            TcpConnectorDetachLoop(connector);
            goto fail_winsock;
        }
    }
    return ERROR_SUCCESS;

fail_winsock:
    if (connector->internal.completion_port != NULL)
        CloseHandle(connector->internal.completion_port);
    HashMap_Free(connector->lookup.tasks);
    HashMap_Free(connector->lookup.pending_io);
    HashMap_Free(connector->lookup.sockets);
    free(connector->interf.lock);
    WSACleanup();
    free(connector);
    return failure;
}

