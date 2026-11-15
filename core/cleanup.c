#include "cleanup.h"

#include "util.h"
#include "../pool/picker.h"

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

void ProbeCleanupIcmpResult(IcmpResult *result)
{
    free(result);
}

void ProbeCleanupUdpResultList(UdpResultList *results)
{
    if (results == NULL)
        return;
    free(results->results);
    free(results);
}

void ProbeCleanupTcpResultList(TcpResultList *results)
{
    if (results == NULL)
        return;
    free(results->results);
    free(results);
}

void ProbeCleanupHostUpCheckResult(HostUpCheckResult *result)
{
    if (result == NULL)
        return;
    ProbeCleanupIcmpResult(result->iresult);
    ProbeCleanupUdpResultList(result->uresult);
    ProbeCleanupTcpResultList(result->tresult);
    memset(result, 0, sizeof(*result));
}

void ProbeCleanupHostUpCheckOptions(HostupCheckOptions *options)
{
    if (options == NULL)
        return;
    ProbeCleanupPortListItems(&options->udp_options.ports);
    ProbeCleanupPortListItems(&options->tcp_options.ports);
    memset(options, 0, sizeof(*options));
}

void ProbeCleanupLuaState(lua_State *lua)
{
    if (lua != NULL)
        lua_close(lua);
}

void ProbeCleanupWorkerContext(WorkerContext *context)
{
    if (context == NULL)
        return;
    ProbeCleanupHostUpCheckResult(&context->check.result);
    ProbeCleanupLuaState(context->lua);
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
    ProbeCleanupHostUpCheckOptions(&config->options);
    ProbeCleanupPortList(config->ports);
    ProbeCleanupString(config->cidr);
    ProbeCleanupString(config->resume);
    ProbeCleanupString(config->dump);
    ProbeCleanupString(config->script);
    memset(config, 0, sizeof(*config));
}

void HandleListFree(HandleList *list)
{
    if (list == NULL)
        return;
    free(list->handles);
    free(list);
}
