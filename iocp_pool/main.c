#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>

#include "iocp-pool.h"

static const char *TaskStatusName(IocpProbeTaskStatus status)
{
    switch (status) {
    case IOCP_PROBE_TASK_QUEUED: return "queued";
    case IOCP_PROBE_TASK_PENDING: return "pending";
    case IOCP_PROBE_TASK_COMPLETED: return "completed";
    default: return "unknown";
    }
}

int main(void)
{
    struct in_addr address;
    if (InetPtonA(AF_INET, "127.0.0.1", &address) != 1) {
        fprintf(stderr, "could not parse example address\n");
        return EXIT_FAILURE;
    }

    DWORD error = 0;
    IocpPool *pool = IocpPoolCreate(32, 1500, &error);
    if (pool == NULL) {
        fprintf(stderr, "could not create IOCP pool (error %lu)\n",
            (unsigned long)error);
        return EXIT_FAILURE;
    }

    IocpProbeOption first = { 0 };
    first.probe.addr = ntohl(address.s_addr);
    first.probe.timeout_ms = 1000;
    first.port = 80;

    IocpProbeOption second = first;
    second.port = 22;

    uint64_t first_id = 0;
    uint64_t second_id = 0;
    int ok = IocpPoolAdd(pool, &first, &first_id, &error) &&
        IocpPoolAdd(pool, &second, &second_id, &error);
    if (!ok) {
        fprintf(stderr, "could not add probe task (error %lu)\n",
            (unsigned long)error);
        IocpPoolDestroy(pool);
        return EXIT_FAILURE;
    }

    /* Update a queued task before running it. */
    first.port = 443;
    if (!IocpPoolUpdate(pool, first_id, &first, &error)) {
        fprintf(stderr, "could not update probe task (error %lu)\n",
            (unsigned long)error);
        IocpPoolDestroy(pool);
        return EXIT_FAILURE;
    }

    if (!IocpPoolRun(pool, &error)) {
        fprintf(stderr, "IOCP run failed (error %lu)\n",
            (unsigned long)error);
        IocpPoolDestroy(pool);
        return EXIT_FAILURE;
    }

    const uint64_t task_ids[] = { first_id, second_id };
    for (size_t i = 0; i < sizeof(task_ids) / sizeof(task_ids[0]); ++i) {
        IocpProbeTask task;
        if (!IocpPoolGet(pool, task_ids[i], &task)) {
            fprintf(stderr, "could not read task %llu\n",
                (unsigned long long)task_ids[i]);
            continue;
        }
        printf("127.0.0.1:%u %s",
            (unsigned)task.options.port, TaskStatusName(task.status));
        if (task.error != 0)
            printf(" (error %lu)", (unsigned long)task.error);
        putchar('\n');

        if (!IocpPoolRemove(pool, task.id, &error)) {
            fprintf(stderr, "could not remove task %llu (error %lu)\n",
                (unsigned long long)task.id, (unsigned long)error);
        }
    }

    IocpPoolDestroy(pool);
    return EXIT_SUCCESS;
}
