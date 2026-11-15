#ifndef CORE_UPCHECK_UTIL_H
#define CORE_UPCHECK_UTIL_H

#include "probe.h"

HandleList *HandleListNew(void);
int HandleListPush(HandleList *list, HANDLE handle);
HANDLE HandleListPop(HandleList *list);
HANDLE HandleListAt(const HandleList *list, uint32_t index);
void HandleListFree(HandleList *list);
PortList *PortListCopy(PortList *ports);

#endif
