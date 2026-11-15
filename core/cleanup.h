#ifndef CORE_CLEANUP_H
#define CORE_CLEANUP_H

#include "probe.h"

void ProbeCleanupString(char *string);
void ProbeCleanupCidrRange(CidrRange *range);
void ProbeCleanupPortListItems(PortList *ports);
void ProbeCleanupPortList(PortList *ports);
void ProbeCleanupIcmpResult(IcmpResult *result);
void ProbeCleanupUdpResultList(UdpResultList *results);
void ProbeCleanupTcpResultList(TcpResultList *results);
void ProbeCleanupHostUpCheckResult(HostUpCheckResult *result);
void ProbeCleanupHostUpCheckOptions(HostupCheckOptions *options);
void ProbeCleanupLuaState(lua_State *lua);
void ProbeCleanupWorkerContext(WorkerContext *context);
void ProbeCleanupLock(SRWLOCK *lock);
void ProbeCleanupPicker(IpPicker *picker);

void ProbeConfig_Free(ProbeConfig *config);
void HandleListFree(HandleList *list);

#endif
