#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { ULONGLONG started; } Timer;

static Timer *TimerStart(void)
{
    Timer *timer = malloc(sizeof(*timer));
    if (timer != NULL)
        timer->started = GetTickCount64();
    return timer;
}

static uint64_t TimerElapsed(Timer **timer)
{
    if (timer == NULL || *timer == NULL)
        return 0;
    uint64_t elapsed = GetTickCount64() - (*timer)->started;
    free(*timer);
    *timer = NULL;
    return elapsed;
}

typedef struct HttpState HttpState;
typedef struct HttpFuture {
    BOOL completed;
    CONDITION_VARIABLE wakeup;
    SRWLOCK lock;
    DWORD error;
    uint32_t id;
    HttpState *state;
} HttpFuture;

typedef struct {
    HttpFuture *future;
    uint32_t id;
} HttpFutureEntry;

static HttpFutureEntry futs[30] = { 0 };
static HashSet *http_task_id_cache;
static const char *blog_addresses[] = {
    "192.0.78.9",
    "192.178.203.191",
    "162.159.152.4",
    "172.64.150.56",
    "192.0.77.40",
    "35.157.26.135",
    "199.15.163.133",
    "198.185.159.180",
    "74.115.51.7",
    "54.84.72.218",
    "174.143.201.50",
    "104.26.12.56",
    "104.200.22.215",
    "184.196.86.176",
    "188.93.148.37",
    "95.217.30.133",
    "149.154.164.13",
    "151.101.130.217",
    "216.150.1.193",
    "81.19.74.0",
    "138.199.15.193",
    "172.66.40.206",
    "3.164.195.122",
    "66.6.42.252",
    "162.159.128.70",
    "54.219.112.130",
    "104.18.117.121"
};

struct HttpState {
    HttpFuture *future;
    char connect_id[48];
    char receive_id[48];
    char send_id[48];
    ByteArray *request;
    ByteArray *response;
    BOOL receive_submitted;
    BOOL send_submitted;
};

static volatile LONG http_request_sequence;
static BOOL http_verbose;

static ByteArray *HttpByteArrayNew(uint32_t capacity)
{
    ByteArray *array = calloc(1, sizeof(*array));
    if (array == NULL)
        return NULL;
    if (capacity != 0)
        array->buffer = malloc(capacity);
    if (capacity != 0 && array->buffer == NULL) {
        free(array);
        return NULL;
    }
    array->cap = capacity;
    return array;
}

static void HttpByteArrayDestroy(ByteArray *array)
{
    if (array == NULL)
        return;
    free(array->buffer);
    free(array);
}

static ByteArray *HttpByteArrayDup(const ByteArray *source)
{
    if (source == NULL)
        return NULL;
    ByteArray *copy = HttpByteArrayNew(source->len);
    if (copy == NULL)
        return NULL;
    if (source->len != 0)
        memcpy(copy->buffer, source->buffer, source->len);
    copy->len = source->len;
    return copy;
}

static void HttpRecordError(HttpState *state, DWORD error)
{
    if (state == NULL || error == TCP_CONNECTOR_SUCCESS)
        return;
    AcquireSRWLockExclusive(&state->future->lock);
    if (state->future->error == TCP_CONNECTOR_SUCCESS)
        state->future->error = error;
    ReleaseSRWLockExclusive(&state->future->lock);
}

static void HttpCompletedSignalWake(HttpState *state)
{
    HttpFuture *future = state->future;
    AcquireSRWLockExclusive(&future->lock);
    future->completed = TRUE;
    WakeAllConditionVariable(&future->wakeup);
    ReleaseSRWLockExclusive(&future->lock);
}

static BOOL OnReceive(TcpConnectorTask *task);

static BOOL OnConnect(TcpConnectorTask *task)
{
    HttpState *state = task->userdata;
    if (task->WSA_error != TCP_CONNECTOR_SUCCESS)
        HttpRecordError(state, task->WSA_error);

    TcpConnectorTask receive;
    TcpConnectorTaskInit(&receive);
    BOOL valid = TcpConnectorTaskSetKind(&receive, TCP_CONNECTOR_TASK_RECV) &&
        TcpConnectorTaskSetId(&receive, state->receive_id) &&
        TcpConnectorTaskSetSocketId(&receive, state->connect_id) &&
        TcpConnectorTaskSetPayload(&receive, state->response) &&
        TcpConnectorTaskSetTimeout(&receive, 10000);
    if (!valid) {
        HttpRecordError(state, TCP_CONNECTOR_INVALID_ARGUMENT);
        HttpCompletedSignalWake(state);
        return TRUE;
    }
    TcpConnectorTaskSetCallback(&receive, OnReceive, state);
    DWORD error = TcpConnectorCreateTask(receive.options, receive.id,
        receive.on_completed, receive.userdata);
    if (error != TCP_CONNECTOR_SUCCESS) {
        HttpRecordError(state, error);
        HttpCompletedSignalWake(state);
        return TRUE;
    }
    state->receive_submitted = TRUE;

    if (task->WSA_error != TCP_CONNECTOR_SUCCESS)
        return TRUE;

    TcpConnectorTask send;
    TcpConnectorTaskInit(&send);
    valid = TcpConnectorTaskSetKind(&send, TCP_CONNECTOR_TASK_SEND) &&
        TcpConnectorTaskSetId(&send, state->send_id) &&
        TcpConnectorTaskSetSocketId(&send, state->connect_id) &&
        TcpConnectorTaskSetPayload(&send, state->request);
    if (!valid) {
        HttpRecordError(state, TCP_CONNECTOR_INVALID_ARGUMENT);
        TcpConnectorCancelTask(state->receive_id);
        return TRUE;
    }
    DWORD send_error = TcpConnectorCreateTask(send.options, send.id, NULL, state);
    if (send_error != TCP_CONNECTOR_SUCCESS) {
        HttpRecordError(state, send_error);
        TcpConnectorCancelTask(state->receive_id);
        return TRUE;
    }
    state->send_submitted = TRUE;
    return TRUE;
}

static BOOL OnReceive(TcpConnectorTask *task)
{
    HttpState *state = task->userdata;
    if (task->WSA_error != TCP_CONNECTOR_SUCCESS) {
        HttpRecordError(state, task->WSA_error);
    } else if (http_verbose && state->response->len != 0) {
        //fwrite(state->response->buffer, 1, state->response->len, stdout);
        //fputc('\n', stdout);
        //fflush(stdout);
    }
    HttpCompletedSignalWake(state);
    return TRUE;
}

static HttpFuture *http_get(const char *ipv4_address, uint16_t port, const char *path)
{
    if (ipv4_address == NULL || path == NULL)
        return NULL;

    HttpFuture *future = calloc(1, sizeof(*future));
    HttpState *state = calloc(1, sizeof(*state));
    if (future == NULL || state == NULL) {
        free(future);
        free(state);
        return NULL;
    }
    InitializeSRWLock(&future->lock);
    InitializeConditionVariable(&future->wakeup);
    future->state = state;
    state->future = future;

    LONG sequence = InterlockedIncrement(&http_request_sequence);
    future->id = (uint32_t)sequence;
    snprintf(state->connect_id, sizeof(state->connect_id), "http-%lu-connect",
        (unsigned long)future->id);
    snprintf(state->receive_id, sizeof(state->receive_id), "http-%lu-receive",
        (unsigned long)future->id);
    snprintf(state->send_id, sizeof(state->send_id), "http-%lu-send",
        (unsigned long)future->id);

    char request_text[2048];
    int request_length = snprintf(request_text, sizeof(request_text),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, ipv4_address);
    if (request_length < 0 || (size_t)request_length >= sizeof(request_text)) {
        future->error = TCP_CONNECTOR_INVALID_ARGUMENT;
        HttpCompletedSignalWake(state);
        return future;
    }

    state->request = HttpByteArrayNew((uint32_t)request_length);
    state->response = HttpByteArrayNew(65536);
    if (state->request == NULL || state->response == NULL) {
        future->error = TCP_CONNECTOR_OUT_OF_MEMORY;
        HttpCompletedSignalWake(state);
        return future;
    }
    memcpy(state->request->buffer, request_text, (size_t)request_length);
    state->request->len = (uint32_t)request_length;

    TcpConnectorTask connect;
    TcpConnectorTaskInit(&connect);
    BOOL valid = TcpConnectorTaskSetKind(&connect, TCP_CONNECTOR_TASK_CONNECT) &&
        TcpConnectorTaskSetId(&connect, state->connect_id) &&
        TcpConnectorTaskSetAddr(&connect, ipv4_address) &&
        TcpConnectorTaskSetPort(&connect, port) &&
        TcpConnectorTaskSetTimeout(&connect, 5000) &&
        TcpConnectorTaskSetRetries(&connect, 0);
    if (!valid) {
        future->error = TCP_CONNECTOR_INVALID_ARGUMENT;
        HttpCompletedSignalWake(state);
        return future;
    }
    TcpConnectorTaskSetCallback(&connect, OnConnect, state);
    DWORD error = TcpConnectorCreateTask(connect.options, connect.id,
        connect.on_completed, connect.userdata);
    if (error != TCP_CONNECTOR_SUCCESS) {
        future->error = error;
        HttpCompletedSignalWake(state);
    }
    return future;
}

static ByteArray *http_await(HttpFuture *future)
{
    if (future == NULL)
        return NULL;

    AcquireSRWLockExclusive(&future->lock);
    while (!future->completed) {
        if (!SleepConditionVariableSRW(&future->wakeup, &future->lock, INFINITE, 0)) {
            ReleaseSRWLockExclusive(&future->lock);
            return NULL;
        }
    }
    ByteArray *response = future->error == TCP_CONNECTOR_SUCCESS
        ? future->state->response : NULL;
    ReleaseSRWLockExclusive(&future->lock);
    return response;
}

static void http_future_destroy(HttpFuture *future)
{
    if (future == NULL)
        return;
    http_await(future);
    HttpState *state = future->state;
    if (state != NULL && state->send_submitted)
        TcpConnectorAwaitTask(state->send_id);
    if (state != NULL) {
        HttpByteArrayDestroy(state->request);
        HttpByteArrayDestroy(state->response);
        free(state);
    }
    free(future);
}

ByteArray *http_get_sync(const char *ipv4_address, uint16_t port,
    const char *path)
{
    HttpFuture *future = http_get(ipv4_address, port, path);
    if (future == NULL)
        return NULL;
    ByteArray *response = HttpByteArrayDup(http_await(future));
    http_future_destroy(future);
    return response;
}

static ByteArray *http_wait_any(int future_count, HttpFutureEntry futures[],
    HttpFutureEntry **completed_entry)
{
    if (completed_entry != NULL)
        *completed_entry = NULL;
    if (future_count <= 0 || futures == NULL || completed_entry == NULL)
        return NULL;

    for (;;) {
        char **receive_ids = malloc((size_t)future_count * sizeof(*receive_ids));
        if (receive_ids == NULL)
            return NULL;
        for (int i = 0; i < future_count; ++i) {
            HttpFuture *future = futures[i].future;
            if (future == NULL) {
                free(receive_ids);
                return NULL;
            }
            receive_ids[i] = future->state->receive_id;
        }

        if (http_task_id_cache == NULL)
            http_task_id_cache = HashSet_New((size_t)future_count);
        char *completed_task_id = TcpConnectorTasksWaitAny(TcpConnectorGet(),
            receive_ids, future_count, http_task_id_cache);
        free(receive_ids);
        if (completed_task_id == NULL) {
            HashSet_Free(http_task_id_cache);
            http_task_id_cache = NULL;
            return NULL;
        }

        for (int i = 0; i < future_count; ++i) {
            if (strcmp(completed_task_id, futures[i].future->state->receive_id) == 0) {
                ByteArray *response = http_await(futures[i].future);
                if (future_count == 1) {
                    HashSet_Free(http_task_id_cache);
                    http_task_id_cache = NULL;
                }
                *completed_entry = &futures[i];
                return response;
            }
        }
    }
}

static BOOL TestAsync(void)
{
    const uint32_t blog_count =
        (uint32_t)(sizeof(blog_addresses) / sizeof(blog_addresses[0]));
    HttpFutureEntry pending[30];
    int pending_count = 0;
    BOOL succeeded = TRUE;
    for (uint32_t i = 0; i < blog_count; ++i) {
        futs[i].future = http_get(blog_addresses[i], 80, "/");
        if (futs[i].future != NULL) {
            futs[i].id = futs[i].future->id;
            pending[pending_count++] = futs[i];
        } else {
            succeeded = FALSE;
            fprintf(stderr, "Could not start HTTP request %u\n", i);
        }
    }

    Timer *start = TimerStart();
    while (pending_count > 0) {
        Timer *request_start = TimerStart();
        HttpFutureEntry *completed = NULL;
        ByteArray *response = http_wait_any(pending_count, pending, &completed);
        uint64_t request_elapsed = TimerElapsed(&request_start);
        if (completed == NULL) {
            succeeded = FALSE;
            fprintf(stderr, "Waiting for an HTTP future failed\n");
            break;
        }
        uint32_t address_index = 0;
        while (address_index < blog_count &&
            futs[address_index].future != completed->future)
            ++address_index;
        printf("TestAsync %s: %llu ms\n",
            address_index < blog_count ? blog_addresses[address_index] : "unknown",
            (unsigned long long)request_elapsed);
        if (response == NULL) {
            succeeded = FALSE;
            fprintf(stderr, "HTTP request %lu failed\n", (unsigned long)completed->id);
        }
        int completed_index = (int)(completed - pending);
        pending[completed_index] = pending[--pending_count];
    }
    uint64_t await_elapsed = TimerElapsed(&start);
    printf("TestAsync awaited %u HTTP futures in %llu ms\n", blog_count,
        (unsigned long long)await_elapsed);

    for (uint32_t i = 0; i < blog_count; ++i) {
        if (futs[i].future == NULL)
            continue;
        http_future_destroy(futs[i].future);
        futs[i].future = NULL;
    }
    return succeeded;
}

static BOOL TestSync(void)
{
    const uint32_t blog_count =
        (uint32_t)(sizeof(blog_addresses) / sizeof(blog_addresses[0]));
    BOOL succeeded = TRUE;
    Timer *start = TimerStart();
    for (uint32_t i = 0; i < blog_count; ++i) {
        Timer *request_start = TimerStart();
        ByteArray *response = http_get_sync(blog_addresses[i], 80, "/");
        uint64_t request_elapsed = TimerElapsed(&request_start);
        printf("TestSync %s: %llu ms\n", blog_addresses[i],
            (unsigned long long)request_elapsed);
        if (response == NULL)
            succeeded = FALSE;
        HttpByteArrayDestroy(response);
    }
    uint64_t total_elapsed = TimerElapsed(&start);
    printf("TestSync completed %u HTTP GETs in %llu ms\n", blog_count,
        (unsigned long long)total_elapsed);
    return succeeded;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--verbose") == 0)
            http_verbose = TRUE;
    }

    DWORD error = TcpConnectorInitEventloop(8, TCP_CONNECTOR_WITH_IO_COMPLETION_WORKER, 30);
    if (error != TCP_CONNECTOR_SUCCESS) {
        fprintf(stderr, "TCP connector init failed: %lu\n", (unsigned long)error);
        return 1;
    }

    BOOL succeeded = TestAsync();
    if (!TestSync())
        succeeded = FALSE;

    error = TcpConnectorClearEventloop();
    if (error != TCP_CONNECTOR_SUCCESS) {
        fprintf(stderr, "TCP connector cleanup failed: %lu\n", (unsigned long)error);
        return 1;
    }
    return succeeded ? 0 : 1;
}
