#include "hook.h"

#include <windows.h>
#include <stdlib.h>

struct InitCallbackHandle {
    HANDLE thread;
    InitCallback callback;
    IpPickerInitCallbackContext context;
};

static DWORD WINAPI InitCallbackWorker(LPVOID handle_void)
{
    InitCallbackHandle *handle = handle_void;
    handle->callback(&handle->context);
    return 0;
}

InitCallbackHandle *InitCallbackStart(InitCallback callback, void *arg)
{
    InitCallbackHandle *handle = calloc(1, sizeof *handle);
    if (handle == NULL)
        return NULL;

    handle->callback = callback;
    handle->context.arg = arg;
    atomic_init(&handle->context.stop, 0);
    if (callback != NULL) {
        handle->thread = CreateThread(NULL, 0, InitCallbackWorker, handle, 0, NULL);
        if (handle->thread == NULL) {
            free(handle);
            return NULL;
        }
    }
    return handle;
}

void InitCallbackRequestStop(InitCallbackHandle *handle)
{
    if (handle == NULL)
        return;
    atomic_store(&handle->context.stop, 1);
}

void InitCallbackWait(InitCallbackHandle *handle)
{
    if (handle == NULL)
        return;

    HANDLE thread = handle->thread;
    handle->thread = NULL;

    if (thread != NULL) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
    free(handle);
}
