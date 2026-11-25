#include "internal.h"
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

DWORD TcpConnectorInitEventloop(uint32_t limit, uint32_t flags,
    uint32_t callback_queue_initial_capacity)
{
    if (limit == 0 || callback_queue_initial_capacity == 0 ||
        (flags & ~TCP_CONNECTOR_WITH_IO_COMPLETION_WORKER) != 0)
        return TCP_CONNECTOR_INVALID_ARGUMENT;

    DWORD failure = TCP_CONNECTOR_OUT_OF_MEMORY;
    TcpConnectorLoop *connector = calloc(1, sizeof(*connector));
    if (connector == NULL)
        return TCP_CONNECTOR_OUT_OF_MEMORY;

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
    if (connector->lookup.tasks == NULL || connector->lookup.pending_io == NULL ||
        connector->lookup.sockets == NULL)
        goto fail_winsock;

    InitializeSRWLock(&connector->control.state_lock);
    InitializeSRWLock(&connector->control.callback_lock);
    InitializeConditionVariable(&connector->control.slot_available);
    InitializeConditionVariable(&connector->control.submitters_done);
    InitializeConditionVariable(&connector->control.task_finished);
    InitializeConditionVariable(&connector->control.callback_available);
    if (!TcpConnectorTaskCallbackQueueInit(&connector->control.callback_queue,
            callback_queue_initial_capacity))
        goto fail_winsock;
    connector->internal.completion_port = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (connector->internal.completion_port == NULL)
        goto fail_winsock;

    connector->internal.connect_ex = GetConnectEx();
    if (connector->internal.connect_ex == NULL)
        goto fail_winsock;

    connector->config.max_pending = limit;
    connector->config.flags = flags;
    atomic_init(&connector->control.error, TCP_CONNECTOR_SUCCESS);
    atomic_init(&connector->control.running, 0);
    atomic_init(&connector->control.io_completion_worker_stop, FALSE);
    atomic_init(&connector->control.callback_runner_stop, FALSE);
    if (!TcpConnectorSetLoop(connector)) {
        failure = TCP_CONNECTOR_ALREADY_EXISTS;
        goto fail_winsock;
    }
    connector->control.callback_thread = CreateThread(NULL, 0,
        TcpConnectorTaskCallbackRunner, connector, 0, NULL);
    if (connector->control.callback_thread == NULL) {
        failure = GetLastError();
        goto fail_threads;
    }
    if ((flags & TCP_CONNECTOR_WITH_IO_COMPLETION_WORKER) != 0) {
        connector->control.io_completion_worker = CreateThread(NULL, 0,
            TcpConnectorIoCompletionWorker, NULL, 0, NULL);
        if (connector->control.io_completion_worker == NULL) {
            failure = GetLastError();
            goto fail_threads;
        }
    }
    return TCP_CONNECTOR_SUCCESS;

fail_threads:
    TcpConnectorDetachLoop(connector);
    TcpConnectorLockCallbackLock(connector);
    connector->control.callback_stopping = TRUE;
    atomic_store(&connector->control.callback_runner_stop, TRUE);
    TcpConnectorSignalAllCallbackAvailable(connector);
    TcpConnectorUnlockCallbackLock(connector);
    if (connector->control.callback_thread != NULL) {
        WaitForSingleObject(connector->control.callback_thread, INFINITE);
        CloseHandle(connector->control.callback_thread);
    }
fail_winsock:
    TcpConnectorTaskCallbackQueueDestroy(&connector->control.callback_queue);
    if (connector->internal.completion_port != NULL)
        CloseHandle(connector->internal.completion_port);
    HashMap_Free(connector->lookup.tasks);
    HashMap_Free(connector->lookup.pending_io);
    HashMap_Free(connector->lookup.sockets);
    WSACleanup();
    free(connector);
    return failure;
}

