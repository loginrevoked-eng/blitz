#include "http.h"
#include "../ev/tcp/public.h"
#include <ctype.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ws2tcpip.h>

#define HTTP_READ_SIZE 4096
#define HTTP_RESPONSE_LIMIT (16u * 1024u * 1024u)

typedef enum {
    HTTP_STAGE_CONNECT,
    HTTP_STAGE_SEND,
    HTTP_STAGE_RECEIVE
} HttpTaskStage;

struct HttpFuture {
    SRWLOCK lock;
    CONDITION_VARIABLE finished;
    char connect_task_id[64];
    char send_task_id[64];
    char host[256];
    char *request;
    size_t request_length;
    char *response_data;
    size_t response_length;
    size_t response_capacity;
    uint16_t port;
    uint32_t address;
    uint32_t next_read_id;
    uint64_t request_id;
    DWORD error;
    unsigned int status_code;
    BOOL complete;
};

static atomic_uint_fast64_t http_request_sequence;

static BOOL HttpTaskCompleted(TcpConnectorTask *task);

static HttpResponse HttpMakeError(DWORD error)
{
    HttpResponse response = { .error = error };
    return response;
}

static void HttpComplete(HttpFuture *future, DWORD error)
{
    AcquireSRWLockExclusive(&future->lock);
    if (!future->complete) {
        future->error = error;
        future->complete = TRUE;
        WakeAllConditionVariable(&future->finished);
    }
    ReleaseSRWLockExclusive(&future->lock);
}

static BOOL HttpContainsUnsafeUrlChar(const char *value)
{
    for (const unsigned char *ch = (const unsigned char *)value; *ch != '\0'; ++ch) {
        if (*ch <= 32 || *ch == 127)
            return TRUE;
    }
    return FALSE;
}

static DWORD HttpParseUrl(HttpFuture *future, const char *url, char **path_out)
{
    if (url == NULL || path_out == NULL)
        return HTTP_INVALID_ARGUMENT;
    if (strlen(url) > 8192)
        return HTTP_INVALID_URL;
    if (strncmp(url, "http://", 7) != 0)
        return strncmp(url, "https://", 8) == 0 ? HTTP_UNSUPPORTED_SCHEME : HTTP_INVALID_URL;
    if (HttpContainsUnsafeUrlChar(url))
        return HTTP_INVALID_URL;

    const char *authority = url + 7;
    const char *path = strpbrk(authority, "/?#");
    size_t authority_length = path == NULL ? strlen(authority) : (size_t)(path - authority);
    if (authority_length == 0)
        return HTTP_INVALID_URL;

    const char *port_text = NULL;
    const char *colon = memchr(authority, ':', authority_length);
    size_t host_length = authority_length;
    future->port = 80;
    if (colon != NULL) {
        host_length = (size_t)(colon - authority);
        port_text = colon + 1;
        if (port_text >= authority + authority_length)
            return HTTP_INVALID_URL;
        char port_buffer[8];
        size_t port_length = (size_t)(authority + authority_length - port_text);
        if (port_length >= sizeof(port_buffer))
            return HTTP_INVALID_URL;
        memcpy(port_buffer, port_text, port_length);
        port_buffer[port_length] = '\0';
        char *end = NULL;
        unsigned long port = strtoul(port_buffer, &end, 10);
        if (*end != '\0' || port == 0 || port > UINT16_MAX)
            return HTTP_INVALID_URL;
        future->port = (uint16_t)port;
    }
    if (host_length == 0 || host_length >= sizeof(future->host))
        return HTTP_INVALID_URL;
    for (size_t i = 0; i < host_length; ++i) {
        if (authority[i] == '@' || authority[i] == '[' || authority[i] == ']')
            return HTTP_INVALID_URL;
    }
    memcpy(future->host, authority, host_length);
    future->host[host_length] = '\0';

    char service[8];
    snprintf(service, sizeof(service), "%u", (unsigned)future->port);
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo *addresses = NULL;
    WSADATA winsock_data;
    if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0)
        return HTTP_NAME_RESOLUTION_FAILED;
    int resolve_error = getaddrinfo(future->host, service, &hints, &addresses);
    WSACleanup();
    if (resolve_error != 0 || addresses == NULL)
        return HTTP_NAME_RESOLUTION_FAILED;
    future->address = ntohl(((struct sockaddr_in *)addresses->ai_addr)->sin_addr.s_addr);
    freeaddrinfo(addresses);

    const char *fragment = path == NULL ? NULL : strchr(path, '#');
    size_t path_length = path == NULL ? 0 : (fragment == NULL ? strlen(path) : (size_t)(fragment - path));
    if (path_length == 0) {
        *path_out = malloc(2);
        if (*path_out != NULL)
            memcpy(*path_out, "/", 2);
    } else if (path[0] == '?') {
        *path_out = malloc(path_length + 2);
        if (*path_out != NULL) {
            (*path_out)[0] = '/';
            memcpy(*path_out + 1, path, path_length);
            (*path_out)[path_length + 1] = '\0';
        }
    } else {
        *path_out = malloc(path_length + 1);
        if (*path_out != NULL) {
            memcpy(*path_out, path, path_length);
            (*path_out)[path_length] = '\0';
        }
    }
    return *path_out == NULL ? HTTP_OUT_OF_MEMORY : HTTP_SUCCESS;
}

static DWORD HttpBuildRequest(HttpFuture *future, const char *path)
{
    BOOL include_port = future->port != 80;
    int request_length = include_port
        ? snprintf(NULL, 0, "GET %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\nAccept: */*\r\n\r\n", path, future->host, future->port)
        : snprintf(NULL, 0, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nAccept: */*\r\n\r\n", path, future->host);
    if (request_length < 0 || (unsigned long)request_length > UINT32_MAX)
        return HTTP_INVALID_URL;
    future->request = malloc((size_t)request_length + 1);
    if (future->request == NULL)
        return HTTP_OUT_OF_MEMORY;
    if (include_port)
        snprintf(future->request, (size_t)request_length + 1, "GET %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\nAccept: */*\r\n\r\n", path, future->host, future->port);
    else
        snprintf(future->request, (size_t)request_length + 1, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nAccept: */*\r\n\r\n", path, future->host);
    future->request_length = (size_t)request_length;
    return HTTP_SUCCESS;
}

static ByteArray *HttpByteArrayNew(uint32_t capacity)
{
    ByteArray *array = calloc(1, sizeof(*array));
    if (array == NULL)
        return NULL;
    array->buffer = malloc(capacity);
    if (array->buffer == NULL) {
        free(array);
        return NULL;
    }
    array->cap = capacity;
    return array;
}

static void HttpByteArrayDestroy(ByteArray *array)
{
    if (array != NULL) {
        free(array->buffer);
        free(array);
    }
}

static DWORD HttpSubmitSend(HttpFuture *future)
{
    ByteArray *payload = HttpByteArrayNew((uint32_t)future->request_length);
    if (payload == NULL)
        return HTTP_OUT_OF_MEMORY;
    memcpy(payload->buffer, future->request, future->request_length);
    payload->len = (uint32_t)future->request_length;
    TcpConnectorTaskOption options = { 0 };
    options.kind = TCP_CONNECTOR_TASK_SEND;
    options.sr_opt.kind = TCP_CONNECTOR_TASK_SEND;
    options.sr_opt.payload = payload;
    options.sr_opt.socket_id = future->connect_task_id;
    DWORD error = TcpConnectorCreateTask(options, future->send_task_id, HttpTaskCompleted, future);
    if (error != TCP_CONNECTOR_SUCCESS)
        HttpByteArrayDestroy(payload);
    return error;
}

static DWORD HttpSubmitReceive(HttpFuture *future)
{
    ByteArray *payload = HttpByteArrayNew(HTTP_READ_SIZE);
    if (payload == NULL)
        return HTTP_OUT_OF_MEMORY;
    char task_id[64];
    snprintf(task_id, sizeof(task_id), "http-%llu-recv-%u",
        (unsigned long long)future->request_id, (unsigned)future->next_read_id++);
    TcpConnectorTaskOption options = { 0 };
    options.kind = TCP_CONNECTOR_TASK_RECV;
    options.sr_opt.kind = TCP_CONNECTOR_TASK_RECV;
    options.sr_opt.payload = payload;
    options.sr_opt.socket_id = future->connect_task_id;
    DWORD error = TcpConnectorCreateTask(options, task_id, HttpTaskCompleted, future);
    if (error != TCP_CONNECTOR_SUCCESS)
        HttpByteArrayDestroy(payload);
    return error;
}

static const char *HttpHeaderEnd(const char *data, size_t length)
{
    if (data == NULL || length < 4)
        return NULL;
    for (size_t i = 0; i + 3 < length; ++i) {
        if (memcmp(data + i, "\r\n\r\n", 4) == 0)
            return data + i + 4;
    }
    return NULL;
}

static BOOL HttpHeaderNameEquals(const char *start, size_t length, const char *name)
{
    size_t name_length = strlen(name);
    if (length != name_length)
        return FALSE;
    for (size_t i = 0; i < length; ++i) {
        if (tolower((unsigned char)start[i]) != tolower((unsigned char)name[i]))
            return FALSE;
    }
    return TRUE;
}

static BOOL HttpHasCompleteContentLength(HttpFuture *future, size_t header_length)
{
    const char *data = future->response_data;
    size_t cursor = 0;
    while (cursor + 1 < header_length) {
        const char *line_end = strstr(data + cursor, "\r\n");
        if (line_end == NULL || (size_t)(line_end - data) >= header_length)
            return FALSE;
        const char *colon = memchr(data + cursor, ':', (size_t)(line_end - (data + cursor)));
        if (colon != NULL && HttpHeaderNameEquals(data + cursor, (size_t)(colon - (data + cursor)), "Content-Length")) {
            const char *value = colon + 1;
            while (value < line_end && (*value == ' ' || *value == '\t'))
                ++value;
            char *end = NULL;
            unsigned long long body_length = _strtoui64(value, &end, 10);
            if (end == value || (end < line_end && *end != ' ' && *end != '\t') ||
                body_length > HTTP_RESPONSE_LIMIT)
                return FALSE;
            return future->response_length >= header_length + (size_t)body_length;
        }
        cursor = (size_t)(line_end - data) + 2;
    }
    return FALSE;
}

static unsigned int HttpParseStatusCode(const char *data, size_t length)
{
    if (data == NULL || length < 12 || memcmp(data, "HTTP/", 5) != 0)
        return 0;
    const char *line_end = strstr(data, "\r\n");
    if (line_end == NULL || (size_t)(line_end - data) > length)
        return 0;
    const char *space = memchr(data, ' ', (size_t)(line_end - data));
    if (space == NULL || line_end - space < 4 || !isdigit((unsigned char)space[1]) ||
        !isdigit((unsigned char)space[2]) || !isdigit((unsigned char)space[3]))
        return 0;
    return (unsigned int)(space[1] - '0') * 100 + (unsigned int)(space[2] - '0') * 10 + (unsigned int)(space[3] - '0');
}

static DWORD HttpAppendResponse(HttpFuture *future, const char *data, size_t length)
{
    if (length > HTTP_RESPONSE_LIMIT - future->response_length)
        return HTTP_RESPONSE_TOO_LARGE;
    size_t required = future->response_length + length + 1;
    if (required > future->response_capacity) {
        size_t capacity = future->response_capacity == 0 ? HTTP_READ_SIZE : future->response_capacity;
        while (capacity < required) {
            if (capacity > HTTP_RESPONSE_LIMIT / 2) {
                capacity = HTTP_RESPONSE_LIMIT + 1;
                break;
            }
            capacity *= 2;
        }
        char *response = realloc(future->response_data, capacity);
        if (response == NULL)
            return HTTP_OUT_OF_MEMORY;
        future->response_data = response;
        future->response_capacity = capacity;
    }
    memcpy(future->response_data + future->response_length, data, length);
    future->response_length += length;
    future->response_data[future->response_length] = '\0';
    return HTTP_SUCCESS;
}

static void HttpFinishResponse(HttpFuture *future, DWORD error)
{
    future->status_code = HttpParseStatusCode(future->response_data, future->response_length);
    if (error == HTTP_SUCCESS && future->status_code == 0)
        error = HTTP_INVALID_RESPONSE;
    TcpConnectorCloseSocketById(future->connect_task_id);
    HttpComplete(future, error);
}

static BOOL HttpTaskCompleted(TcpConnectorTask *task)
{
    HttpFuture *future = task->userdata;
    DWORD error = task->WSA_error;
    if (task->options.kind == TCP_CONNECTOR_TASK_SEND)
        HttpByteArrayDestroy(task->options.sr_opt.payload);
    if (task->options.kind == TCP_CONNECTOR_TASK_RECV) {
        ByteArray *payload = task->options.sr_opt.payload;
        uint32_t received = payload->len;
        if (error == TCP_CONNECTOR_SUCCESS && received != 0)
            error = HttpAppendResponse(future, (const char *)payload->buffer, received);
        HttpByteArrayDestroy(payload);
        if (error != TCP_CONNECTOR_SUCCESS) {
            HttpFinishResponse(future, error);
            return TRUE;
        }
        const char *header_end = HttpHeaderEnd(future->response_data, future->response_length);
        if (received == 0 || (header_end != NULL && HttpHasCompleteContentLength(future, (size_t)(header_end - future->response_data)))) {
            HttpFinishResponse(future, HTTP_SUCCESS);
            return TRUE;
        }
        error = HttpSubmitReceive(future);
        if (error != TCP_CONNECTOR_SUCCESS)
            HttpFinishResponse(future, error);
        return TRUE;
    }
    if (error != TCP_CONNECTOR_SUCCESS) {
        HttpFinishResponse(future, error);
        return TRUE;
    }
    if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT) {
        error = HttpSubmitSend(future);
        if (error != TCP_CONNECTOR_SUCCESS)
            HttpFinishResponse(future, error);
    } else {
        error = HttpSubmitReceive(future);
        if (error != TCP_CONNECTOR_SUCCESS)
            HttpFinishResponse(future, error);
    }
    return TRUE;
}

HttpFuture *http_get(const char *url)
{
    HttpFuture *future = calloc(1, sizeof(*future));
    if (future == NULL)
        return NULL;
    InitializeSRWLock(&future->lock);
    InitializeConditionVariable(&future->finished);
    future->request_id = atomic_fetch_add(&http_request_sequence, 1) + 1;
    snprintf(future->connect_task_id, sizeof(future->connect_task_id), "http-%llu-connect", (unsigned long long)future->request_id);
    snprintf(future->send_task_id, sizeof(future->send_task_id), "http-%llu-send", (unsigned long long)future->request_id);

    char *path = NULL;
    DWORD error = HttpParseUrl(future, url, &path);
    if (error == HTTP_SUCCESS)
        error = HttpBuildRequest(future, path);
    free(path);
    if (error != HTTP_SUCCESS) {
        HttpComplete(future, error);
        return future;
    }

    TcpConnectorTaskOption options = { 0 };
    options.kind = TCP_CONNECTOR_TASK_CONNECT;
    options.conn_opt.addr = future->address;
    options.conn_opt.port = future->port;
    DWORD timeout_ms = 30000;
    options.conn_opt.timeout_ms = timeout_ms;
    error = TcpConnectorCreateTask(options, future->connect_task_id, HttpTaskCompleted, future);
    if (error != TCP_CONNECTOR_SUCCESS)
        HttpComplete(future, error);
    return future;
}

HttpResponse http_await(HttpFuture *future)
{
    if (future == NULL)
        return HttpMakeError(HTTP_INVALID_ARGUMENT);
    AcquireSRWLockExclusive(&future->lock);
    while (!future->complete)
        SleepConditionVariableSRW(&future->finished, &future->lock, INFINITE, 0);
    HttpResponse response = { .error = future->error, .status_code = future->status_code, .data = future->response_data, .length = future->response_length };
    future->response_data = NULL;
    ReleaseSRWLockExclusive(&future->lock);
    free(future->request);
    free(future);
    return response;
}

HttpResponse http_get_sync(const char *url)
{
    HttpFuture *future = http_get(url);
    return future == NULL ? HttpMakeError(HTTP_OUT_OF_MEMORY) : http_await(future);
}

void http_future_destroy(HttpFuture *future)
{
    if (future == NULL)
        return;
    AcquireSRWLockExclusive(&future->lock);
    while (!future->complete)
        SleepConditionVariableSRW(&future->finished, &future->lock, INFINITE, 0);
    ReleaseSRWLockExclusive(&future->lock);
    free(future->request);
    free(future->response_data);
    free(future);
}

void http_response_destroy(HttpResponse *response)
{
    if (response == NULL)
        return;
    free(response->data);
    *response = (HttpResponse){ 0 };
}

