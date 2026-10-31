#include <windows.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include "fiestel.h"
#include "logging.h"
#include "picker.h"

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

static uint64_t RandomU64(uint64_t least, uint64_t highest);

static int AppendBuckets(BucketList *eligible, uint64_t start, uint64_t end,
                         uint32_t bucket_width)
{
    for (uint64_t offset = start; offset <= end;) {
        uint64_t actual_width = end - offset + 1;
        if (actual_width > bucket_width)
            actual_width = bucket_width;
        Bucket bucket = {
            .permutation_seed = (uint32_t)RandomU64(0, UINT32_MAX),
            .permutation_current = 0,
            .start = (Ipv4)offset,
            .end = (Ipv4)(offset + actual_width - 1),
            .is_cooling = 0,
            .expires_at = 0
        };
        if (!BucketListPush(eligible, bucket))
            return 0;
        offset += actual_width;
    }
    return 1;
}


static uint64_t NowNs(void)
{
    static LARGE_INTEGER freq;
    if(freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&freq);
    }

    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);

    uint64_t f = (uint64_t)freq.QuadPart;
    uint64_t v = (uint64_t)c.QuadPart;
    return (v / f) * 1000000000ULL + (v % f) * 1000000000ULL / f;
}

static DWORD RemainingMs(uint64_t expires_at, uint64_t now)
{
    if(expires_at <= now)
    {
        return 0;
    }

    uint64_t ms = (expires_at - now + 999999ULL) / 1000000ULL;
    if(ms >= INFINITE)
    {
        ms = INFINITE - 1;
    }
    return (DWORD)ms;
}

static uint64_t RandomU64(uint64_t least, uint64_t highest)
{
    static __declspec(thread) int seeded = 0;
    if(!seeded)
    {
        srand((unsigned)(NowNs() ^ ((uint64_t)GetCurrentThreadId() << 16)));
        seeded = 1;
    }

    uint64_t r = 0;
    for(int i = 0; i < 5; i++) r = (r << 15) ^ (uint64_t)rand();
    return least + r % (highest - least + 1);
}


static void IpPicker_SetEligible(IpPicker *picker, Bucket bucket)
{
    bucket.is_cooling = 0;
    bucket.expires_at = 0;
    BucketListPush(picker->eligible, bucket);
}

static void IpPicker_PutCooldown(IpPicker *picker, size_t bucket_index)
{
    Bucket clone = *BucketListAt(picker->eligible, bucket_index);
    BucketListPop(picker->eligible, bucket_index);

    uint64_t bucket_width = clone.end - clone.start + 1;
    if(clone.permutation_current >= bucket_width)
    {
        Debug("bucket dropped, all of its ip addresses have been picked\n");
        if(BucketListIsEmpty(picker->eligible) && BucketListIsEmpty(picker->cooldown))
        {
            WakeAllConditionVariable(&picker->threading.eligible_nonempty);
        }
        return;
    }

    clone.is_cooling = 1;
    clone.expires_at = NowNs() + (uint64_t)picker->cooldown_period * 1000000ULL;
    BucketListPush(picker->cooldown, clone);
    WakeConditionVariable(&picker->threading.cooldown_nonempty);
}


static DWORD WINAPI CooldownPopExpiredBuckets(LPVOID picker_void)
{
    IpPicker *picker = (IpPicker*)picker_void;
    BucketList *cooldown = picker->cooldown;

    AcquireSRWLockExclusive(&picker->threading.lock);

    while(!picker->threading.stop)
    {
        if(BucketListIsEmpty(cooldown))
        {
            SleepConditionVariableSRW(&picker->threading.cooldown_nonempty, &picker->threading.lock, INFINITE, 0);
            continue;
        }

        uint64_t now = NowNs();
        int moved = 0;

        while(!BucketListIsEmpty(cooldown))
        {
            Bucket *front = BucketListAt(cooldown, 0);
            if(front->expires_at > now)
            {
                break;
            }

            Bucket expired = *front;
            BucketListPopFront(cooldown);
            IpPicker_SetEligible(picker, expired);
            moved = 1;
        }

        if(moved)
        {
            WakeAllConditionVariable(&picker->threading.eligible_nonempty);
        }

        if(BucketListIsEmpty(cooldown))
        {
            continue;
        }

        DWORD wait_ms = RemainingMs(BucketListAt(cooldown, 0)->expires_at, now);
        SleepConditionVariableSRW(&picker->threading.cooldown_nonempty, &picker->threading.lock, wait_ms, 0);
    }

    ReleaseSRWLockExclusive(&picker->threading.lock);
    return 0;
}


int IpPicker_Pick(IpPicker *picker, Ipv4 *out, uint64_t *picked_index)
{
    AcquireSRWLockExclusive(&picker->threading.lock);

    if(BucketListIsEmpty(picker->eligible) && !picker->threading.stop)
    {
        Warn("Pool Exhausted! waiting for a bucket to come off cooldown...\n");
    }

    while(BucketListIsEmpty(picker->eligible) && !picker->threading.stop)
    {
        if(BucketListIsEmpty(picker->cooldown))
        {
            break;
        }
        SleepConditionVariableSRW(&picker->threading.eligible_nonempty, &picker->threading.lock, INFINITE, 0);
    }

    if(picker->threading.stop || BucketListIsEmpty(picker->eligible))
    {
        ReleaseSRWLockExclusive(&picker->threading.lock);
        return 0;
    }

    size_t draw = (size_t)RandomU64(0, picker->eligible->len - 1);
    Bucket *bucket = BucketListAt(picker->eligible, draw);

    uint32_t index;
    uint32_t bucket_width = (uint32_t)(bucket->end - bucket->start + 1);
    FeistelPermute(bucket->permutation_seed, (uint32_t)bucket->permutation_current,
                   bucket_width, &index);
    bucket->permutation_current++;

    Ipv4 pick = bucket->start + index;
    Debug("picked %u as the next ip\n", (unsigned)pick);

    uint64_t index_at_pick = picker->picked_count;
    IpPicker_PutCooldown(picker, draw);
    picker->picked_count++;

    ReleaseSRWLockExclusive(&picker->threading.lock);
    *out = pick;
    if (picked_index != NULL)
        *picked_index = index_at_pick;
    return 1;
}

uint64_t IpPicker_InitialExhaustionCount(const IpPicker *picker)
{
    return picker == NULL || picker->eligible == NULL
        ? 0 : (uint64_t)picker->eligible->len;
}

void IpPicker_Stop(IpPicker *picker)
{
    if (picker == NULL)
        return;

    IpPicker_RequestStop(picker);
    if (picker->sync_thread == NULL)
        return;

    WaitForSingleObject(picker->sync_thread, INFINITE);
    CloseHandle(picker->sync_thread);
    picker->sync_thread = NULL;
}

void IpPicker_RequestStop(IpPicker *picker)
{
    if (picker == NULL)
        return;

    AcquireSRWLockExclusive(&picker->threading.lock);
    picker->threading.stop = 1;
    ReleaseSRWLockExclusive(&picker->threading.lock);

    WakeAllConditionVariable(&picker->threading.cooldown_nonempty);
    WakeAllConditionVariable(&picker->threading.eligible_nonempty);
}

int InitEligibleList(IpPicker *picker)
{
    if (picker == NULL || picker->universe == NULL || picker->bucket_width == 0 ||
        picker->universe->start > picker->universe->end)
        return 0;

    uint64_t range_len = CidrRangeLen(picker->universe);
    if (range_len == 0)
        return 0;

    BucketList *eligible = BucketListNew();
    if (eligible == NULL)
        return 0;

    uint64_t cursor = picker->universe->start;
    uint64_t universe_end = picker->universe->end;
    if (picker->exclude_private_ranges) {
        for (size_t i = 0; i < sizeof EXCLUDED / sizeof *EXCLUDED; i++) {
            uint64_t excluded_start = EXCLUDED[i].base;
            uint64_t excluded_end = excluded_start |
                (0xFFFFFFFFu >> EXCLUDED[i].prefix);
            if (excluded_end < cursor || excluded_start > universe_end)
                continue;
            if (excluded_start > cursor &&
                !AppendBuckets(eligible, cursor, excluded_start - 1,
                               picker->bucket_width)) {
                BucketListFree(eligible);
                return 0;
            }
            if (excluded_end >= universe_end) {
                cursor = universe_end + 1;
                break;
            }
            if (excluded_end >= cursor)
                cursor = excluded_end + 1;
        }
    }
    if (cursor <= universe_end &&
        !AppendBuckets(eligible, cursor, universe_end, picker->bucket_width)) {
        BucketListFree(eligible);
        return 0;
    }

    BucketListFree(picker->eligible);
    picker->eligible = eligible;
    picker->eligible_count = 0;
    for (size_t i = 0; i < eligible->len; i++) {
        Bucket *bucket = BucketListAt(eligible, i);
        picker->eligible_count += (uint64_t)bucket->end - bucket->start + 1;
    }
    return 1;
}

IpPicker *IpPicker_Initialize(CidrRange *universe, uint32_t bucket_width,
                              uint32_t cooldown_period, int exclude_private_ranges)
{
    if (universe == NULL || universe->start > universe->end || bucket_width == 0)
        return NULL;

    IpPicker *picker = calloc(1, sizeof(IpPicker));
    if(!picker)
    {
        return NULL;
    }

    picker->bucket_width = bucket_width;
    picker->cooldown_period = cooldown_period;
    picker->exclude_private_ranges = exclude_private_ranges;
    picker->universe = universe;
    picker->picked_count = 0;
    picker->cooldown = BucketListNew();
    if (picker->cooldown == NULL || !InitEligibleList(picker)) {
        BucketListFree(picker->cooldown);
        BucketListFree(picker->eligible);
        free(picker);
        return NULL;
    }

    InitializeSRWLock(&picker->threading.lock);
    InitializeConditionVariable(&picker->threading.cooldown_nonempty);
    InitializeConditionVariable(&picker->threading.eligible_nonempty);
    picker->threading.stop = 0;

    picker->sync_thread = CreateThread(NULL, 0, CooldownPopExpiredBuckets, picker, 0, NULL);
    if(!picker->sync_thread)
    {
        BucketListFree(picker->cooldown);
        BucketListFree(picker->eligible);
        free(picker);
        return NULL;
    }

    return picker;
}

void IpPicker_Finalize(IpPicker *picker)
{
    if(!picker)
    {
        return;
    }

    if(picker->sync_thread)
    {
        IpPicker_Stop(picker);
    }

    BucketListFree(picker->cooldown);
    BucketListFree(picker->eligible);
    free(picker);
}
