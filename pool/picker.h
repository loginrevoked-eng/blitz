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


typedef struct{
    int exclude_private_ranges;
    int resumeable;
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
    IpPickerThreading threading;
    HANDLE coowldown_worker;
} IpPicker;

typedef struct {
    uint32_t base;
    uint8_t prefix;
} Cidr4;

static const Cidr4 EXCLUDED[] = {
    { 0x00000000,  8 },  /* 0.0.0.0/8        */
    { 0x0A000000,  8 },  /* 10.0.0.0/8       */
    { 0x64400000, 10 },  /* 100.64.0.0/10    */
    { 0x7F000000,  8 },  /* 127.0.0.0/8      */
    { 0xA9FE0000, 16 },  /* 169.254.0.0/16   */
    { 0xAC100000, 12 },  /* 172.16.0.0/12    */
    { 0xC0000000, 24 },  /* 192.0.0.0/24     */
    { 0xC0000200, 24 },  /* 192.0.2.0/24     */
    { 0xC0A80000, 16 },  /* 192.168.0.0/16   */
    { 0xC6120000, 15 },  /* 198.18.0.0/15    */
    { 0xC6336400, 24 },  /* 198.51.100.0/24  */
    { 0xCB007100, 24 },  /* 203.0.113.0/24   */
    { 0xE0000000,  4 },  /* 224.0.0.0/4      */
    { 0xF0000000,  4 },  /* 240.0.0.0/4      */
};



int InitEligibleList(IpPicker *picker);
DWORD WINAPI IpPicker_CooldownWorker(LPVOID picker);
int IpPicker_Pick(IpPicker *picker, Ipv4 *out, uint64_t *picked_index);
uint64_t IpPicker_InitialExhaustionCount(const IpPicker *picker);
void IpPicker_Stop(IpPicker *picker);
void IpPicker_RequestStop(IpPicker *picker);
void IpPicker_Finalize(IpPicker *picker);

#endif
