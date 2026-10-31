#ifndef PICKER_H
#define PICKER_H

#include <windows.h>
#include <stdint.h>

#include "bucket.h"
#include "cidr.h"

typedef struct {
    SRWLOCK lock;
    CONDITION_VARIABLE cooldown_nonempty;
    CONDITION_VARIABLE eligible_nonempty;
    int stop;
} IpPickerThreading;

typedef struct IpPicker {
    uint32_t bucket_width;
    uint32_t cooldown_period;
    int exclude_private_ranges;
    BucketList *cooldown;
    BucketList *eligible;
    CidrRange *universe;
    uint64_t eligible_count;
    IpPickerThreading threading;
    HANDLE sync_thread;
    uint64_t picked_count;
} IpPicker;

IpPicker *IpPicker_Initialize(CidrRange *universe, uint32_t bucket_width,
                              uint32_t cooldown_period, int exclude_private_ranges);
int InitEligibleList(IpPicker *picker);
int IpPicker_Pick(IpPicker *picker, Ipv4 *out, uint64_t *picked_index);
uint64_t IpPicker_InitialExhaustionCount(const IpPicker *picker);
void IpPicker_Stop(IpPicker *picker);
void IpPicker_RequestStop(IpPicker *picker);
void IpPicker_Finalize(IpPicker *picker);

#endif
