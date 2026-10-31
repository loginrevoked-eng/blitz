#include "picker.h"
#include "logging.h"
#include "hashmap/hashmap.h"
#include "hashset/hashset.h"
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>

struct IpPicker {
    uint32_t spacing;
    uint32_t interval;
    HashSet *visited_ips;
    HashSet *cooldown;
    HashMap *cool_down_timers;
    CidrRange *range;
    CRITICAL_SECTION lock;
    volatile LONG running;
};

static int IpPicker_RangeStart(const IpPicker *picker, Ipv4 ip, Ipv4 *range_start)
{
    if (picker == NULL || picker->range == NULL || range_start == NULL ||
        ip < picker->range->start || ip > picker->range->end)
        return 0;

    size_t batch_index = BatchIndexFromIp(picker->range, picker->spacing, ip);
    if (batch_index == SIZE_MAX)
        return 0;

    uint64_t width = (uint64_t)picker->spacing + 1;
    *range_start = picker->range->start + (uint64_t)batch_index * width;
    return 1;
}

IpPicker *IpPicker_New(CidrRange *range, uint32_t spacing, uint32_t interval)
{
    if (range == NULL || range->start > range->end)
        return NULL;

    IpPicker *picker = calloc(1, sizeof *picker);
    if (picker == NULL)
        return NULL;
    picker->spacing = spacing;
    picker->interval = interval;
    picker->range = range;
    picker->visited_ips = HashSet_New(64);
    picker->cooldown = HashSet_New(64);
    picker->cool_down_timers = HashMap_New(64);
    if (picker->visited_ips == NULL || picker->cooldown == NULL || picker->cool_down_timers == NULL ||
        !InitializeCriticalSectionAndSpinCount(&picker->lock, 4000)) {
        HashSet_Free(picker->visited_ips);
        HashSet_Free(picker->cooldown);
        HashMap_Free(picker->cool_down_timers);
        free(picker);
        return NULL;
    }
    return picker;
}

Ipv4 PickRandomIpFromRange(CidrRange *range)
{
    if (range == NULL || range->start > range->end)
        Panic("Cannot pick an IP from an invalid range\n");

    uint64_t length = CidrRangeLen(range);
    uint64_t random = ((uint64_t)(unsigned)rand() << 32) | (unsigned)rand();
    return range->start + random % length;
}

typedef struct {
    IpPicker *picker;
    time_t now;
} CooldownExpiryContext;

static int CooldownExpired(uint64_t range_start, const Value *timer, void *p)
{
    CooldownExpiryContext *context = p;
    if (timer->kind != VALUE_LONG || difftime(context->now, (time_t)timer->l) < context->picker->interval)
        return 0;
    HashSet_RemoveU64(context->picker->cooldown, range_start);
    return 1;
}

static DWORD WINAPI UpdateCooldownMap(LPVOID p)
{
    IpPicker *picker = p;
    while (InterlockedCompareExchange(&picker->running, 1, 1)) {
        CooldownExpiryContext context = { .picker = picker, .now = time(NULL) };
        EnterCriticalSection(&picker->lock);
        HashMap_RemoveIfU64(picker->cool_down_timers, CooldownExpired, &context);
        LeaveCriticalSection(&picker->lock);
        Sleep(1000);
    }
    return 0;
}

HANDLE IpPicker_StartUpdater(IpPicker *picker)
{
    if (picker == NULL)
        return NULL;
    InterlockedExchange(&picker->running, 1);
    HANDLE thread = CreateThread(NULL, 0, UpdateCooldownMap, picker, 0, NULL);
    if (thread == NULL)
        InterlockedExchange(&picker->running, 0);
    return thread;
}

void IpPicker_StopUpdater(IpPicker *picker)
{
    if (picker == NULL)
        return;
    InterlockedExchange(&picker->running, 0);
}

static int IsCoolingDownUnlocked(IpPicker *picker, Ipv4 next)
{
    size_t batch_index = BatchIndexFromIp(picker->range, picker->spacing, next);
    if (batch_index == SIZE_MAX)
        return 0;
    uint64_t width = (uint64_t)picker->spacing + 1;
    Ipv4 range_start = picker->range->start + (uint64_t)batch_index * width;
    return HashSet_ContainsU64(picker->cooldown, range_start);
}

int IsCoolingDown(IpPicker *picker, Ipv4 next)
{
    if (picker == NULL)
        return 0;
    EnterCriticalSection(&picker->lock);
    int result = IsCoolingDownUnlocked(picker, next);
    LeaveCriticalSection(&picker->lock);
    return result;
}

Ipv4 IpPicker_Pick(IpPicker *picker)
{
    if (picker == NULL)
        Panic("Cannot pick an IP with a null picker\n");

    for (;;) {
        Ipv4 next = PickRandomIpFromRange(picker->range);
        Ipv4 range_start;
        if (!IpPicker_RangeStart(picker, next, &range_start))
            Panic("Failed to map IP to its range start\n");

        EnterCriticalSection(&picker->lock);
        int cooling_down = IsCoolingDownUnlocked(picker, next);
        int visited = HashSet_ContainsU64(picker->visited_ips, next);
        if (!cooling_down && !visited) {
            if (!HashSet_PutU64(picker->visited_ips, next) ||
                !HashSet_PutU64(picker->cooldown, range_start)) {
                LeaveCriticalSection(&picker->lock);
                Panic("Failed to record picked IP or range cooldown\n");
            }
            Value timer = { .kind = VALUE_LONG, .l = (long)time(NULL) };
            if (!HashMap_SetU64(picker->cool_down_timers, range_start, timer)) {
                HashSet_RemoveU64(picker->cooldown, range_start);
                LeaveCriticalSection(&picker->lock);
                Panic("Failed to record IP range cooldown time\n");
            }
            LeaveCriticalSection(&picker->lock);
            //Debug("IpPicker_Pick candidate=%" PRIu64 " range_start=%" PRIu64 " accepted\n",
            //      next, range_start);
            return next;
        }
        LeaveCriticalSection(&picker->lock);
        //Debug("IpPicker_Pick candidate=%" PRIu64 " range_start=%" PRIu64
        //      " rejected (cooldown=%d, visited=%d)\n",
        //      next, range_start, cooling_down, visited);
    }
}

void IpPicker_Free(IpPicker *picker)
{
    if (picker == NULL)
        return;
    HashSet_Free(picker->visited_ips);
    HashSet_Free(picker->cooldown);
    HashMap_Free(picker->cool_down_timers);
    DeleteCriticalSection(&picker->lock);
    free(picker);
}
