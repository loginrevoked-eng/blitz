#include <windows.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include "cidr.h"
#include "fiestel.h"
#include "logging.h"
#include "picker.h"
#include "random.h"
#include "resume.h"
#include "time.h"





static Bucket BucketForRange(uint64_t start, uint64_t end, uint32_t bucket_width, uint64_t *actual_width)
{
    *actual_width = end - start + 1;
    if (*actual_width > bucket_width)
        *actual_width = bucket_width;
    uint32_t seed = FiestelSeed();
    Ipv4 start_ip = start;
    Ipv4 end_ip = start + *actual_width - 1;

    return BucketNew(seed, 0, start_ip, end_ip);
}

static int AppendBuckets(BucketList *eligible, uint64_t start, uint64_t end, uint32_t bucket_width)
{
    for (uint64_t offset = start; offset <= end;) {
        uint64_t actual_width;
        Bucket bucket = BucketForRange(offset, end, bucket_width, &actual_width);
        if (!BucketListPush(eligible, bucket))
            return 0;
        offset += actual_width;
    }
    return 1;
}

static int ExcludeRanges(BucketList *eligible, uint64_t start, uint64_t universe_end, uint32_t bucket_width)
{
    uint64_t cursor = start;
    for (size_t i = 0; i < sizeof EXCLUDED / sizeof *EXCLUDED; i++) {
        uint64_t excluded_start = EXCLUDED[i].base;
        uint64_t excluded_end = excluded_start |
            (0xFFFFFFFFu >> EXCLUDED[i].prefix);
        if (excluded_end < cursor || excluded_start > universe_end)
            continue;
        if (excluded_start > cursor &&
            !AppendBuckets(eligible, cursor, excluded_start - 1, bucket_width))
            return 0;
        if (excluded_end >= universe_end)
            return 1;
        if (excluded_end >= cursor)
            cursor = excluded_end + 1;
    }

    return cursor > universe_end || AppendBuckets(eligible, cursor, universe_end, bucket_width);
}


static void IpPicker_SetEligible(IpPicker *picker, Bucket bucket)
{
    bucket.is_cooling = 0;
    bucket.expires_at = 0;
    BucketListPush(picker->eligible, bucket);
}

DWORD WINAPI IpPicker_CooldownWorker(LPVOID picker_void)
{
    IpPicker *picker = (IpPicker *)picker_void;
    BucketList *cooldown = picker->cooldown;

    AcquireSRWLockExclusive(&picker->threading.lock);
    while (!picker->threading.stop) {
        if (BucketListIsEmpty(cooldown)) {
            SleepConditionVariableSRW(&picker->threading.cooldown_nonempty, &picker->threading.lock, INFINITE, 0);
            continue;
        }

        uint64_t now = TimeNowNs();
        int moved = 0;
        while (!BucketListIsEmpty(cooldown)) {
            Bucket *front = BucketListAt(cooldown, 0);
            if (front->expires_at > now)
                break;

            Bucket expired = *front;
            BucketListPopFront(cooldown);
            IpPicker_SetEligible(picker, expired);
            moved = 1;
        }

        if (moved)
            WakeAllConditionVariable(&picker->threading.eligible_nonempty);
        if (BucketListIsEmpty(cooldown))
            continue;

        DWORD wait_ms = TimeRemainingMs(BucketListAt(cooldown, 0)->expires_at, now);
        SleepConditionVariableSRW(&picker->threading.cooldown_nonempty, &picker->threading.lock, wait_ms, 0);
    }

    ReleaseSRWLockExclusive(&picker->threading.lock);
    return 0;
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
    clone.expires_at = TimeNowNs() +
        (uint64_t)picker->config.cooldown_period * 1000000ULL;
    BucketListPush(picker->cooldown, clone);
    WakeConditionVariable(&picker->threading.cooldown_nonempty);
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

    uint64_t index_at_pick = bucket->permutation_current - 1;
    IpPicker_PutCooldown(picker, draw);

    ReleaseSRWLockExclusive(&picker->threading.lock);
    *out = pick;
    if (picked_index != NULL)
        *picked_index = index_at_pick;
    return 1;
}

uint64_t IpPicker_InitialExhaustionCount(const IpPicker *picker)
{
    return picker == NULL || picker->eligible == NULL ? 0 : (uint64_t)picker->eligible->len;
}

void IpPicker_Stop(IpPicker *picker)
{
    if (picker == NULL)
        return;

    IpPicker_RequestStop(picker);
    if (picker->coowldown_worker == NULL)
        return;

    WaitForSingleObject(picker->coowldown_worker, INFINITE);
    CloseHandle(picker->coowldown_worker);
    picker->coowldown_worker = NULL;
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
    if (picker == NULL || picker->universe == NULL || picker->config.bucket_width == 0 ||
        picker->universe->start > picker->universe->end)
        return 0;

    uint64_t range_len = CidrRangeLen(picker->universe);
    if (range_len == 0)
        return 0;

    BucketList *eligible = BucketListNew();
    if (eligible == NULL)
        return 0;

    uint64_t start = picker->universe->start;
    uint64_t universe_end = picker->universe->end;
    int initialized = picker->config.exclude_private_ranges
        ? ExcludeRanges(eligible, start, universe_end, picker->config.bucket_width)
        : AppendBuckets(eligible, start, universe_end, picker->config.bucket_width);
    if (!initialized) {
        BucketListFree(eligible);
        return 0;
    }

    BucketListFree(picker->eligible);
    picker->eligible = eligible;
    return 1;
}

void IpPicker_Finalize(IpPicker *picker)
{
    if(!picker)
    {
        return;
    }

    if(picker->coowldown_worker)
    {
        IpPicker_Stop(picker);
    }

    IpPickerDumpBuckets(picker);
    BucketListFree(picker->cooldown);
    BucketListFree(picker->eligible);
    free(picker->universe);
    free(picker);
}
