#include "public.h"
#include <stdio.h>

static BOOL OnConnect(TcpConnectorTask *task)
{
    printf("%s port %u: status=%u, WSA=%lu\n", task->id,
        (unsigned)task->options.conn_opt.port, (unsigned)task->status,
        (unsigned long)task->WSA_error);
    return TRUE;
}

int main(void)
{
    static const uint16_t ports[] = { 323, 3232, 32321, 434, 545, 2312, 656 };
    const uint32_t task_count = (uint32_t)(sizeof(ports) / sizeof(ports[0]));
    DWORD error = TcpConnectorInitEventloop(task_count,
        TCP_CONNECTOR_WITH_IO_COMPLETION_WORKER, task_count, 2);
    if (error != TCP_CONNECTOR_SUCCESS) {
        fprintf(stderr, "TCP connector init failed: %lu\n", (unsigned long)error);
        return 1;
    }

    char task_ids[sizeof(ports) / sizeof(ports[0])][32];
    uint32_t submitted = 0;
    for (uint32_t i = 0; i < task_count; ++i) {
        TcpConnectorTaskOption options = { 0 };
        options.kind = TCP_CONNECTOR_TASK_CONNECT;
        options.conn_opt.addr = ntohl(inet_addr("127.0.0.1"));
        options.conn_opt.port = ports[i];
        options.conn_opt.timeout_ms = 2000;

        snprintf(task_ids[submitted], sizeof(task_ids[submitted]),
            "localhost-%u", (unsigned)ports[i]);
        error = TcpConnectorCreateTask(options, task_ids[submitted], OnConnect, NULL);
        if (error != TCP_CONNECTOR_SUCCESS) {
            fprintf(stderr, "Could not submit port %u: %lu\n",
                (unsigned)ports[i], (unsigned long)error);
            continue;
        }
        ++submitted;
    }

    for (uint32_t i = 0; i < submitted; ++i) {
        error = TcpConnectorAwaitTask(task_ids[i]);
        if (error != TCP_CONNECTOR_SUCCESS) {
            fprintf(stderr, "Waiting for a task failed: %lu\n",
                (unsigned long)error);
        }
    }

    error = TcpConnectorClearEventloop();
    if (error != TCP_CONNECTOR_SUCCESS) {
        fprintf(stderr, "TCP connector cleanup failed: %lu\n",
            (unsigned long)error);
        return 1;
    }
    return 0;
}
