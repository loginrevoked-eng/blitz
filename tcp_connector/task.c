#include "models.h"
typedef struct {
    TcpConnectorLoop *connector;
    int started;
} StartQueuedContext;

static int StartQueuedEntry(const char *key, const Value *value,
    void *context)
{
    (void)key;
    StartQueuedContext *start = context;
    TcpConnectorTask *task = value->kind == VALUE_POINTER ? value->ptr : NULL;
    if (task != NULL && task->status == TCP_CONNECTOR_TASK_QUEUED &&
        atomic_load(&start->connector->interf.running) < start->connector->config.max_pending) {
        start->started = 1;
        TcpConnectorStartTask(start->connector, task);
    }
    return 0;
}

void TcpConnectorStartQueued(TcpConnectorLoop *connector)
{
    while (atomic_load(&connector->interf.running) < connector->config.max_pending) {
        StartQueuedContext context = { connector, 0 };
        HashMap_RemoveIf(connector->lookup.tasks, StartQueuedEntry, &context);
        if (!context.started)
            break;
    }
}
