#ifndef PICKER_H
#define PICKER_H

#include <windows.h>
#include <stdint.h>

#include "bucket.h"
#include "cidr.h"

typedef enum {
    IP_PICKER_FLAGS_NONE = 0,
    IP_PICKER_RESUMABLE = 1u << 0,
    IP_PICKER_EXCLUDE_PRIVATE_RANGES = 1u << 1
} IpPickerFlags;

typedef struct {
    SRWLOCK lock;
    CONDITION_VARIABLE cooldown_nonempty;
    CONDITION_VARIABLE eligible_nonempty;
    int stop;
} IpPickerThreading;


typedef struct{
    int exclude_private_ranges;
    char *resume_file;
    uint32_t bucket_width;
    uint32_t cooldown_period;
    char *cidr_text;
}IpPickerConfig;


typedef struct IpPicker {
    BucketList *cooldown;
    BucketList *eligible;
    CidrRange *universe;
    IpPickerConfig config;
    IpPickerFlags flags;
    char *dump_file;
    IpPickerThreading threading;
    HANDLE coowldown_worker;
} IpPicker;

int InitEligibleList(IpPicker *picker);
DWORD WINAPI IpPicker_CooldownWorker(LPVOID picker);
int IpPicker_Pick(IpPicker *picker, Ipv4 *out, uint64_t *picked_index);
uint64_t IpPicker_InitialExhaustionCount(const IpPicker *picker);
void IpPicker_Stop(IpPicker *picker);
void IpPicker_RequestStop(IpPicker *picker);
void IpPicker_Finalize(IpPicker *picker);

#endif
