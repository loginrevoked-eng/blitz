#include <uv.h>

#include <stdio.h>
#include <stdlib.h>

#define TARGET_HOST "127.0.0.1"
#define TARGET_PORT 80
#define REQUEST_TIMEOUT_MS 5000
#define READ_BUFFER_SIZE 4096

typedef struct {
    uv_tcp_t tcp;
    uv_connect_t connect_request;
    uv_write_t write_request;
    uv_timer_t timeout;
    char request[128];
} HttpClient;

static void CloseClient(HttpClient *client)
{
    uv_read_stop((uv_stream_t *)&client->tcp);

    if (!uv_is_closing((uv_handle_t *)&client->tcp))
        uv_close((uv_handle_t *)&client->tcp, NULL);
    if (!uv_is_closing((uv_handle_t *)&client->timeout)) {
        uv_timer_stop(&client->timeout);
        uv_close((uv_handle_t *)&client->timeout, NULL);
    }
}

static void OnTimeout(uv_timer_t *timer)
{
    HttpClient *client = timer->data;
    fprintf(stderr, "request timed out after %u ms\n", REQUEST_TIMEOUT_MS);
    CloseClient(client);
}

static void AllocateReadBuffer(uv_handle_t *handle, size_t suggested_size,
    uv_buf_t *buffer)
{
    (void)handle;
    (void)suggested_size;

    char *memory = malloc(READ_BUFFER_SIZE);
    *buffer = uv_buf_init(memory, memory == NULL ? 0 : READ_BUFFER_SIZE);
}

static void OnRead(uv_stream_t *stream, ssize_t bytes_read,
    const uv_buf_t *buffer)
{
    HttpClient *client = stream->data;

    if (bytes_read > 0) {
        fwrite(buffer->base, 1, (size_t)bytes_read, stdout);
        fflush(stdout);
    } else if (bytes_read < 0) {
        if (bytes_read != UV_EOF)
            fprintf(stderr, "read failed: %s\n", uv_strerror((int)bytes_read));
        CloseClient(client);
    }

    free(buffer->base);
}

static void OnWrite(uv_write_t *request, int status)
{
    HttpClient *client = request->data;
    if (status < 0) {
        fprintf(stderr, "write failed: %s\n", uv_strerror(status));
        CloseClient(client);
    }
}

static void OnConnect(uv_connect_t *request, int status)
{
    HttpClient *client = request->data;
    if (status < 0) {
        fprintf(stderr, "connect failed: %s\n", uv_strerror(status));
        CloseClient(client);
        return;
    }

    int read_status = uv_read_start((uv_stream_t *)&client->tcp,
        AllocateReadBuffer, OnRead);
    if (read_status < 0) {
        fprintf(stderr, "could not start reading: %s\n",
            uv_strerror(read_status));
        CloseClient(client);
        return;
    }

    int request_length = snprintf(client->request, sizeof(client->request),
        "HEAD / HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    if (request_length < 0 || (size_t)request_length >= sizeof(client->request)) {
        fprintf(stderr, "could not build HTTP request\n");
        CloseClient(client);
        return;
    }

    client->write_request.data = client;
    uv_buf_t buffer = uv_buf_init(client->request, (unsigned int)request_length);
    int write_status = uv_write(&client->write_request,
        (uv_stream_t *)&client->tcp, &buffer, 1, OnWrite);
    if (write_status < 0) {
        fprintf(stderr, "could not queue HTTP request: %s\n",
            uv_strerror(write_status));
        CloseClient(client);
    }
}

int main(void)
{
    uv_loop_t *loop = uv_default_loop();
    HttpClient client = { 0 };
    struct sockaddr_in address;

    int status = uv_ip4_addr(TARGET_HOST, TARGET_PORT, &address);
    if (status < 0) {
        fprintf(stderr, "invalid target address: %s\n", uv_strerror(status));
        return EXIT_FAILURE;
    }

    status = uv_tcp_init(loop, &client.tcp);
    if (status < 0) {
        fprintf(stderr, "could not initialize TCP handle: %s\n",
            uv_strerror(status));
        return EXIT_FAILURE;
    }
    client.tcp.data = &client;

    status = uv_timer_init(loop, &client.timeout);
    if (status < 0) {
        fprintf(stderr, "could not initialize timeout: %s\n",
            uv_strerror(status));
        uv_close((uv_handle_t *)&client.tcp, NULL);
        uv_run(loop, UV_RUN_DEFAULT);
        return EXIT_FAILURE;
    }
    client.timeout.data = &client;

    status = uv_timer_start(&client.timeout, OnTimeout,
        REQUEST_TIMEOUT_MS, 0);
    if (status < 0) {
        fprintf(stderr, "could not start timeout: %s\n", uv_strerror(status));
        CloseClient(&client);
        uv_run(loop, UV_RUN_DEFAULT);
        return EXIT_FAILURE;
    }

    client.connect_request.data = &client;
    status = uv_tcp_connect(&client.connect_request, &client.tcp,
        (const struct sockaddr *)&address, OnConnect);
    if (status < 0) {
        fprintf(stderr, "could not queue connect: %s\n", uv_strerror(status));
        CloseClient(&client);
    }

    uv_run(loop, UV_RUN_DEFAULT);
    status = uv_loop_close(loop);
    if (status < 0) {
        fprintf(stderr, "could not close event loop: %s\n", uv_strerror(status));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
