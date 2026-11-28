#ifndef POOL_HOOK_H
#define POOL_HOOK_H

#include <stdatomic.h>

typedef struct {
    void *arg;
    atomic_int stop;
} IpPickerInitCallbackContext;

typedef void (*InitCallback)(IpPickerInitCallbackContext *context);
typedef struct InitCallbackHandle InitCallbackHandle;

InitCallbackHandle *InitCallbackStart(InitCallback callback, void *arg);
void InitCallbackRequestStop(InitCallbackHandle *handle);
void InitCallbackWait(InitCallbackHandle *handle);

#endif
