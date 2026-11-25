#include "cleanup.h"

#include "picker.h"

#include <stdlib.h>
#include <string.h>

void ProbeCleanupString(char *string)
{
    free(string);
}

void ProbeCleanupCidrRange(CidrRange *range)
{
    free(range);
}

void ProbeCleanupPortListItems(PortList *ports)
{
    if (ports == NULL)
        return;
    free(ports->items);
    ports->items = NULL;
    ports->len = 0;
}

void ProbeCleanupPortList(PortList *ports)
{
    if (ports == NULL)
        return;
    ProbeCleanupPortListItems(ports);
    free(ports);
}

void ProbeCleanupWorkerContext(WorkerContext *context)
{
    free(context);
}

void ProbeCleanupLock(SRWLOCK *lock)
{
    free(lock);
}

void ProbeCleanupPicker(IpPicker *picker)
{
    IpPicker_Finalize(picker);
}

void ProbeConfig_Free(ProbeConfig *config)
{
    if (config == NULL)
        return;
    ProbeCleanupPortList(config->ports);
    ProbeCleanupString(config->cidr);
    ProbeCleanupString(config->resume);
    ProbeCleanupString(config->dump);
    memset(config, 0, sizeof(*config));
}

void HandleListFree(HandleList *list)
{
    if (list == NULL)
        return;
    free(list->handles);
    free(list);
}
