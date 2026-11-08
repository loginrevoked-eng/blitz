#include "registry.h"

#include <stdlib.h>
#include <windows.h>

#include "cmdline/value.h"
#include "hashmap/hashmap.h"

typedef struct {
    RegistryCallback callback;
    void *context;
} RegistryCallbackEntry;

struct Registry {
    HashMap *callbacks;
    size_t count;
    SRWLOCK lock;
};

static const char *RegistryServiceName(RegistryService service)
{
    switch (service) {
    case REGISTRY_SERVICE_HTTP: return "http";
    case REGISTRY_SERVICE_FTP: return "ftp";
    case REGISTRY_SERVICE_RTSP: return "rtsp";
    case REGISTRY_SERVICE_SSH: return "ssh";
    default: return NULL;
    }
}

static RegistryCallbackEntry *RegistryEntryNew(RegistryCallback callback,
    void *context)
{
    if (callback == NULL)
        return NULL;

    RegistryCallbackEntry *entry = malloc(sizeof(*entry));
    if (entry == NULL)
        return NULL;
    entry->callback = callback;
    entry->context = context;
    return entry;
}

static int RegistryStoreCallback(Registry *registry, RegistryService service,
    RegistryCallback callback, void *context, int create_only, int update_only)
{
    const char *key = RegistryServiceName(service);
    if (registry == NULL || key == NULL || callback == NULL)
        return 0;

    RegistryCallbackEntry *replacement = RegistryEntryNew(callback, context);
    if (replacement == NULL)
        return 0;

    AcquireSRWLockExclusive(&registry->lock);
    Value *existing = HashMap_Get(registry->callbacks, key);
    if ((create_only && existing != NULL) || (update_only && existing == NULL)) {
        ReleaseSRWLockExclusive(&registry->lock);
        free(replacement);
        return 0;
    }

    RegistryCallbackEntry *old_entry = NULL;
    if (existing != NULL && existing->kind == VALUE_POINTER)
        old_entry = existing->p;

    Value value = { .kind = VALUE_POINTER, .p = replacement };
    int stored = HashMap_Set(registry->callbacks, key, value);
    if (stored) {
        if (existing == NULL)
            ++registry->count;
        free(old_entry);
    }
    ReleaseSRWLockExclusive(&registry->lock);

    if (!stored)
        free(replacement);
    return stored;
}

Registry *RegistryNew(size_t capacity)
{
    if (capacity == 0)
        capacity = 16;

    Registry *registry = calloc(1, sizeof(*registry));
    if (registry == NULL)
        return NULL;
    registry->callbacks = HashMap_New(capacity);
    if (registry->callbacks == NULL) {
        free(registry);
        return NULL;
    }
    InitializeSRWLock(&registry->lock);
    return registry;
}

int RegistryCreateCallback(Registry *registry, RegistryService service,
    RegistryCallback callback, void *context)
{
    return RegistryStoreCallback(registry, service, callback, context, 1, 0);
}

int RegistrySetCallback(Registry *registry, RegistryService service,
    RegistryCallback callback, void *context)
{
    return RegistryStoreCallback(registry, service, callback, context, 0, 0);
}

int RegistryUpdateCallback(Registry *registry, RegistryService service,
    RegistryCallback callback, void *context)
{
    return RegistryStoreCallback(registry, service, callback, context, 0, 1);
}

RegistryCallback RegistryFindCallback(Registry *registry,
    const RegistryFinding *finding, void **context)
{
    if (context != NULL)
        *context = NULL;
    if (registry == NULL || finding == NULL)
        return NULL;

    const char *key = RegistryServiceName(finding->service);
    if (key == NULL)
        return NULL;

    AcquireSRWLockShared(&registry->lock);
    Value *value = HashMap_Get(registry->callbacks, key);
    RegistryCallbackEntry *entry = value != NULL && value->kind == VALUE_POINTER
        ? value->p : NULL;
    RegistryCallback callback = entry != NULL ? entry->callback : NULL;
    if (entry != NULL && context != NULL)
        *context = entry->context;
    ReleaseSRWLockShared(&registry->lock);
    return callback;
}

int RegistryRemoveCallback(Registry *registry, RegistryService service)
{
    const char *key = RegistryServiceName(service);
    if (registry == NULL || key == NULL)
        return 0;

    AcquireSRWLockExclusive(&registry->lock);
    Value *value = HashMap_Get(registry->callbacks, key);
    RegistryCallbackEntry *entry = value != NULL && value->kind == VALUE_POINTER
        ? value->p : NULL;
    int removed = HashMap_Remove(registry->callbacks, key);
    if (removed) {
        --registry->count;
        free(entry);
    }
    ReleaseSRWLockExclusive(&registry->lock);
    return removed;
}

static int FreeCallbackEntry(const char *key, const Value *value, void *context)
{
    (void)key;
    (void)context;
    if (value != NULL && value->kind == VALUE_POINTER)
        free(value->p);
    return 1;
}

void RegistryClear(Registry *registry)
{
    if (registry == NULL)
        return;

    AcquireSRWLockExclusive(&registry->lock);
    HashMap_RemoveIf(registry->callbacks, FreeCallbackEntry, NULL);
    registry->count = 0;
    ReleaseSRWLockExclusive(&registry->lock);
}

size_t RegistryCount(Registry *registry)
{
    if (registry == NULL)
        return 0;

    AcquireSRWLockShared(&registry->lock);
    size_t count = registry->count;
    ReleaseSRWLockShared(&registry->lock);
    return count;
}

void RegistryFree(Registry *registry)
{
    if (registry == NULL)
        return;
    RegistryClear(registry);
    HashMap_Free(registry->callbacks);
    free(registry);
}
