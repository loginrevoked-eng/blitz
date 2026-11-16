#ifndef IO_ASYNC_H
#define IO_ASYNC_H

#include <winsock2.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    IO_ASYNC_TCP_OPEN,
    IO_ASYNC_TCP_CLOSED,
    IO_ASYNC_TCP_TIMEOUT,
    IO_ASYNC_TCP_ERROR
} IoAsyncTcpState;

typedef struct {
    uint16_t port;
    IoAsyncTcpState state;
    int error;
} IoAsyncTcpResult;

/* IOCP/ConnectEx batch; address is host byte order. WSAStartup is required. */
int IoIocpTcpProbeBatch(uint32_t address, const uint16_t *ports,
    size_t port_count, DWORD timeout_ms, IoAsyncTcpResult *results,
    int *batch_error);

#endif
