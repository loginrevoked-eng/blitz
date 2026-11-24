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

DWORD TcpConnectorInitEventloop(uint32_t limit, uint32_t flags,
    uint32_t callback_queue_capacity, uint32_t callback_thread_count)
{
    if (limit == 0 || callback_queue_capacity == 0 || callback_thread_count == 0 ||
        callback_queue_capacity > LONG_MAX || callback_thread_count > LONG_MAX ||
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
    connector->control.lock = malloc(sizeof(*connector->control.lock));
    if (connector->lookup.tasks == NULL || connector->lookup.pending_io == NULL ||
        connector->lookup.sockets == NULL || connector->control.lock == NULL)
        goto fail_winsock;

    InitializeSRWLock(connector->control.lock);
    InitializeConditionVariable(&connector->control.slot_available);
    InitializeConditionVariable(&connector->control.submitters_done);
    InitializeConditionVariable(&connector->control.task_finished);
    InitializeConditionVariable(&connector->control.callback_space);
    connector->control.callback_semaphore = CreateSemaphore(NULL, 0,
        LONG_MAX, NULL);
    connector->control.callback_threads = calloc(callback_thread_count,
        sizeof(*connector->control.callback_threads));
    if (connector->control.callback_semaphore == NULL ||
        connector->control.callback_threads == NULL)
        goto fail_winsock;
    connector->control.callback_thread_count = callback_thread_count;
    connector->control.callback_queue_capacity = callback_queue_capacity;
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
    if (!TcpConnectorSetLoop(connector)) {
        failure = TCP_CONNECTOR_ALREADY_EXISTS;
        goto fail_winsock;
    }
    uint32_t started_callback_threads = 0;
    for (; started_callback_threads < callback_thread_count;
         ++started_callback_threads) {
        connector->control.callback_threads[started_callback_threads] = CreateThread(
            NULL, 0, TcpConnectorCallbackThread, connector, 0, NULL);
        if (connector->control.callback_threads[started_callback_threads] == NULL) {
            failure = GetLastError();
            goto fail_threads;
        }
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
    AcquireSRWLockExclusive(connector->control.lock);
    connector->control.callback_stopping = TRUE;
    if (started_callback_threads != 0)
        ReleaseSemaphore(connector->control.callback_semaphore,
            (LONG)started_callback_threads, NULL);
    ReleaseSRWLockExclusive(connector->control.lock);
    for (uint32_t i = 0; i < started_callback_threads; ++i) {
        WaitForSingleObject(connector->control.callback_threads[i], INFINITE);
        CloseHandle(connector->control.callback_threads[i]);
    }
fail_winsock:
    free(connector->control.callback_threads);
    if (connector->control.callback_semaphore != NULL)
        CloseHandle(connector->control.callback_semaphore);
    if (connector->internal.completion_port != NULL)
        CloseHandle(connector->internal.completion_port);
    HashMap_Free(connector->lookup.tasks);
    HashMap_Free(connector->lookup.pending_io);
    HashMap_Free(connector->lookup.sockets);
    free(connector->control.lock);
    WSACleanup();
    free(connector);
    return failure;
}

