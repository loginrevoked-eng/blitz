#ifndef PICKER_H
#define PICKER_H

#include "cidr.h"
#include <stdint.h>
#include <windows.h>

typedef struct IpPicker IpPicker;

IpPicker *IpPicker_New(CidrRange *range, uint32_t spacing, uint32_t interval);
HANDLE IpPicker_StartUpdater(IpPicker *picker);
void IpPicker_StopUpdater(IpPicker *picker);
void IpPicker_Free(IpPicker *picker);
Ipv4 PickRandomIpFromRange(CidrRange *range);
int IsCoolingDown(IpPicker *picker, Ipv4 next);
Ipv4 IpPicker_Pick(IpPicker *picker);

#endif
