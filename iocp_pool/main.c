#include "pool.h"

#include <stdint.h>
#include <stdio.h>

#define RESPONSE_CAPACITY 4096

#define HOST_COUNT 29

struct host_info {
    uint32_t ip;
    uint16_t port;
};

static struct host_info hosts[HOST_COUNT] = {
    { 0x17D09414u, 80 }, // 23.208.148.20
    { 0xBC9BB37Fu, 80 }, // 188.155.179.127
    { 0x6D5F98DEu, 80 }, // 109.95.152.222
    { 0x332651DFu, 80 }, // 51.38.81.223
    { 0x226696D1u, 80 }, // 34.102.150.209
    { 0x12F45490u, 80 }, // 18.244.84.144
    { 0x3E1C8FCDu, 80 }, // 62.28.143.205
    { 0x034F187Cu, 80 }, // 3.79.24.124
    { 0x524CAF93u, 80 }, // 82.76.191.147
    { 0x0D26BCC0u, 80 }, // 13.38.188.192
    { 0xBC80E250u, 80 }, // 188.128.226.80
    { 0x34D39D2Eu, 80 }, // 52.211.157.46
    { 0x519F4710u, 80 }, // 81.159.71.16
    { 0x02EE1092u, 80 }, // 2.238.16.146
    { 0xC60D1674u, 80 }, // 198.13.22.116
    { 0x251B528Fu, 80 }, // 37.27.82.143
    { 0x687F26EFu, 80 }, // 104.127.38.239
    { 0x550D92F0u, 80 }, // 85.13.146.240
    { 0x5C2698CBu, 80 }, // 92.38.152.203
    { 0xD4C13488u, 80 }, // 212.193.52.136
    { 0xD4CDE2F6u, 80 }, // 212.77.226.246
    { 0x226F725Eu, 80 }, // 34.111.114.94
    { 0x574C0AD4u, 80 }, // 87.76.10.212
    { 0x641F1616u, 80 }, // 100.31.22.22
    { 0x51AE2B7Bu, 80 }, // 81.174.43.123
    { 0xB2D8FAD8u, 80 }, // 178.216.250.216
    { 0xADECC580u, 80 }, // 173.236.197.128
    { 0x82E139A0u, 80 }, // 130.225.57.160
    { 0x175E9870u, 80 }  // 23.94.152.112
};

typedef struct {
    char operation_id[32];
    char socket_id[32];
    char send_id[32];
    char recv_id[32];
    uint8_t response_buffer[RESPONSE_CAPACITY];
    ByteArray response;
    BOOL submitted;
} HeadOperation;

static int BuildHeadOperation(IocpPool *pool, const struct host_info *host,
    HeadOperation *head, size_t index)
{
    snprintf(head->operation_id, sizeof(head->operation_id), "head-%zu", index);
    snprintf(head->socket_id, sizeof(head->socket_id), "socket-%zu", index);
    snprintf(head->send_id, sizeof(head->send_id), "head-%zu-send", index);
    snprintf(head->recv_id, sizeof(head->recv_id), "head-%zu-recv", index);

    IocpTaskOption connect = { 0 };
    connect.kind = IOCP_TASK_CONNECT;
    connect.conn_opt.addr = host->ip;
    connect.conn_opt.port = host->port;
    connect.conn_opt.timeout_ms = 1500;
    connect.conn_opt.retries = 0;

    static uint8_t request_bytes[] =
        "HEAD / HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    ByteArray request = {
        request_bytes,
        (uint32_t)(sizeof(request_bytes) - 1),
        (uint32_t)(sizeof(request_bytes) - 1)
    };

    IocpTaskOption send = { 0 };
    send.kind = IOCP_TASK_SEND;
    send.sr_opt.kind = IOCP_TASK_SEND;
    send.sr_opt.socket_id = head->socket_id;
    send.sr_opt.payload = &request;

    head->response.buffer = head->response_buffer;
    head->response.len = 0;
    head->response.cap = RESPONSE_CAPACITY;
    IocpTaskOption receive = { 0 };
    receive.kind = IOCP_TASK_RECV;
    receive.sr_opt.kind = IOCP_TASK_RECV;
    receive.sr_opt.socket_id = head->socket_id;
    receive.sr_opt.payload = &head->response;

    DWORD error = IocpPoolCreateOperation(pool, head->operation_id);
    if (error != ERROR_SUCCESS) {
        fprintf(stderr, "could not create HEAD operation for port %u (%lu)\n",
            (unsigned)host->port, (unsigned long)error);
        return 0;
    }
    error = IocpPoolOperationAddTask(pool, head->operation_id, connect,
        head->socket_id);
    if (error != ERROR_SUCCESS) {
        fprintf(stderr, "could not add connect task for port %u (%lu)\n",
            (unsigned)host->port, (unsigned long)error);
        return 0;
    }
    error = IocpPoolOperationAddTask(pool, head->operation_id, send,
        head->send_id);
    if (error != ERROR_SUCCESS) {
        fprintf(stderr, "could not add send task for port %u (%lu)\n",
            (unsigned)host->port, (unsigned long)error);
        return 0;
    }
    error = IocpPoolOperationAddTask(pool, head->operation_id, receive,
        head->recv_id);
    if (error != ERROR_SUCCESS) {
        fprintf(stderr, "could not add receive task for port %u (%lu)\n",
            (unsigned)host->port, (unsigned long)error);
        return 0;
    }
    return 1;
}

int main(void)
{
    IocpPool *pool = IocpPoolNew(HOST_COUNT);
    if (pool == NULL) {
        fprintf(stderr, "could not create IOCP pool\n");
        return 1;
    }

    HeadOperation operations[HOST_COUNT] = { 0 };
    for (size_t i = 0; i < HOST_COUNT; ++i) {
        if (!BuildHeadOperation(pool, &hosts[i], &operations[i], i))
            continue;
        if (IocpPoolSubmitOperation(pool, operations[i].operation_id)
                == ERROR_SUCCESS)
            operations[i].submitted = TRUE;
        else
            fprintf(stderr, "could not submit HEAD operation for port %u\n",
                (unsigned)hosts[i].port);
    }

    DWORD wait_error = IocpPoolWaitOperations(pool);
    if (wait_error != ERROR_SUCCESS) {
        fprintf(stderr, "IOCP wait failed: %lu\n", (unsigned long)wait_error);
        IocpPoolDestroy(pool);
        return 1;
    }

    int failed = 0;
    for (size_t i = 0; i < HOST_COUNT; ++i) {
        HeadOperation *head = &operations[i];
        if (!head->submitted)
            continue;
        IocpOperation result;
        DWORD error = IocpPoolGetOperation(pool, head->operation_id, &result);
        if (error != ERROR_SUCCESS || result.status != IOCP_OPERATION_COMPLETED) {
            fprintf(stderr, "127.0.0.1:%u HEAD failed (Winsock error %lu)\n",
                (unsigned)hosts[i].port, (unsigned long)(error == ERROR_SUCCESS
                    ? result.WSA_error : error));
            failed = 1;
            continue;
        }
        printf("%d:%u HEAD response:\n", hosts[i].ip, (unsigned)hosts[i].port);
        if (head->response.len != 0)
            fwrite(head->response.buffer, 1, head->response.len, stdout);
        putchar('\n');
    }

    IocpPoolDestroy(pool);
    return failed;
}
