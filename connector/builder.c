#include "connector.h"
#include <string.h>

void TcpConnectorTaskInit(TcpConnectorTask *task)
{
    if (task != NULL)
        *task = (TcpConnectorTask){ .status = TCP_CONNECTOR_TASK_PENDING };
}

BOOL TcpConnectorTaskSetId(TcpConnectorTask *task, const char *task_id)
{
    if (task == NULL || task_id == NULL || task_id[0] == '\0')
        return FALSE;
    task->id = (char *)task_id;
    return TRUE;
}

BOOL TcpConnectorTaskSetKind(TcpConnectorTask *task, TcpConnectorTaskOptionKind kind)
{
    if (task == NULL || (kind != TCP_CONNECTOR_TASK_CONNECT &&
        kind != TCP_CONNECTOR_TASK_SEND && kind != TCP_CONNECTOR_TASK_RECV))
        return FALSE;

    task->options = (TcpConnectorTaskOption){ 0 };
    task->options.kind = kind;
    if (kind == TCP_CONNECTOR_TASK_SEND || kind == TCP_CONNECTOR_TASK_RECV)
        task->options.sr_opt.kind = kind;
    return TRUE;
}

BOOL TcpConnectorTaskSetAddr(TcpConnectorTask *task, const char *ipv4_address)
{
    if (task == NULL || task->options.kind != TCP_CONNECTOR_TASK_CONNECT ||
        ipv4_address == NULL)
        return FALSE;

    unsigned long address = inet_addr(ipv4_address);
    if (address == INADDR_NONE && strcmp(ipv4_address, "255.255.255.255") != 0)
        return FALSE;
    task->options.conn_opt.addr = ntohl((uint32_t)address);
    return TRUE;
}

BOOL TcpConnectorTaskSetPort(TcpConnectorTask *task, uint16_t port)
{
    if (task == NULL || task->options.kind != TCP_CONNECTOR_TASK_CONNECT || port == 0)
        return FALSE;
    task->options.conn_opt.port = port;
    return TRUE;
}

BOOL TcpConnectorTaskSetTimeout(TcpConnectorTask *task, uint32_t timeout_ms)
{
    if (task == NULL)
        return FALSE;
    if (task->options.kind == TCP_CONNECTOR_TASK_CONNECT)
        task->options.conn_opt.timeout_ms = timeout_ms;
    else if (task->options.kind == TCP_CONNECTOR_TASK_SEND ||
        task->options.kind == TCP_CONNECTOR_TASK_RECV)
        task->options.sr_opt.timeout_ms = timeout_ms;
    else
        return FALSE;
    return TRUE;
}

BOOL TcpConnectorTaskSetRetries(TcpConnectorTask *task, uint32_t retries)
{
    if (task == NULL || task->options.kind != TCP_CONNECTOR_TASK_CONNECT)
        return FALSE;
    task->options.conn_opt.retries = retries;
    return TRUE;
}

BOOL TcpConnectorTaskSetSocketId(TcpConnectorTask *task, const char *socket_id)
{
    if (task == NULL || socket_id == NULL || socket_id[0] == '\0' ||
        (task->options.kind != TCP_CONNECTOR_TASK_SEND &&
            task->options.kind != TCP_CONNECTOR_TASK_RECV))
        return FALSE;
    task->options.sr_opt.socket_id = (char *)socket_id;
    return TRUE;
}

BOOL TcpConnectorTaskSetPayload(TcpConnectorTask *task, ByteArray *payload)
{
    if (task == NULL || payload == NULL ||
        (task->options.kind != TCP_CONNECTOR_TASK_SEND &&
            task->options.kind != TCP_CONNECTOR_TASK_RECV))
        return FALSE;
    task->options.sr_opt.payload = payload;
    return TRUE;
}

void TcpConnectorTaskSetCallback(TcpConnectorTask *task,
    TcpConnectorCompletionCallback callback, void *userdata)
{
    if (task == NULL)
        return;
    task->on_completed = callback;
    task->userdata = userdata;
}

