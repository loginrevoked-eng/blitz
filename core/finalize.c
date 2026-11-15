#include <winsock2.h>

#include "finalize.h"
#include "cleanup.h"

static int ProbeWaitWorkers(ProbeContext *context)
{
    if (context == NULL || context->workers == NULL)
        return context != NULL;

    int all_waited = 1;
    for (uint32_t i = 0; i < context->workers->len; ++i) {
        HANDLE worker = context->workers->handles[i];
        if (worker == NULL)
            continue;

        DWORD wait_result = WaitForSingleObject(worker, INFINITE);
        if (wait_result != WAIT_OBJECT_0) {
            all_waited = 0;
            continue;
        }

        if (!CloseHandle(worker)) {
            all_waited = 0;
            continue;
        }
        context->workers->handles[i] = NULL;
    }
    return all_waited;
}

int ProbeFinalize(ProbeContext *context)
{
    if (context == NULL)
        return 0;

    if (!ProbeWaitWorkers(context))
        return 0;

    HandleListFree(context->workers);
    context->workers = NULL;

    if (context->picker != NULL) {
        ProbeCleanupPicker(context->picker);
        context->picker = NULL;
    }

    ProbeConfig_Free(&context->config);
    ProbeCleanupLock(context->lock);
    context->lock = NULL;
    if (context->winsock_started) {
        WSACleanup();
        context->winsock_started = 0;
    }
    return 1;
}
