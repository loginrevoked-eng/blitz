#include "resume.h"

#include "deser.h"
#include "fs.h"

#include <stdint.h>
#include <stdlib.h>

static Bucket *FindBucket(BucketList *buckets, Ipv4 start)
{
    size_t low = 0;
    size_t high = buckets->len;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (buckets->buckets[middle].start < start)
            low = middle + 1;
        else
            high = middle;
    }
    return low < buckets->len && buckets->buckets[low].start == start
        ? &buckets->buckets[low] : NULL;
}

int IpPicker_ResumeBuckets(IpPicker *picker)
{
    if (picker == NULL)
        return 0;
    if (!picker->config.resumeable || picker->config.resume_file == NULL)
        return 1;

    char *json = Fs_ReadFile(picker->config.resume_file, NULL);
    if (json == NULL)
        return Fs_LastError() == FS_ERR_NOT_FOUND;

    BucketList *loaded = BucketListNew();
    uint64_t bucket_width = 0;
    int valid = loaded != NULL && BucketListFromJson(json, loaded, &bucket_width) &&
        (uint32_t)bucket_width == picker->config.bucket_width;
    free(json);
    if (!valid) {
        BucketListFree(loaded);
        return 0;
    }

    for (size_t i = 0; i < loaded->len; i++) {
        Bucket *bucket = &loaded->buckets[i];
        bucket->start = (Ipv4)(uint32_t)bucket->start;
        uint32_t current = (uint32_t)bucket->permutation_current;
        Bucket *expected = FindBucket(picker->eligible, bucket->start);
        if (expected == NULL || expected->permutation_current == SIZE_MAX) {
            BucketListFree(loaded);
            return 0;
        }

        uint64_t width = expected->end - expected->start + 1;
        if ((uint64_t)current >= width) {
            BucketListFree(loaded);
            return 0;
        }

        bucket->permutation_seed = (uint32_t)bucket->permutation_seed;
        bucket->permutation_current = current;
        bucket->end = expected->end;
        bucket->is_cooling = 0;
        bucket->expires_at = 0;
        expected->permutation_current = SIZE_MAX;
    }

    BucketListFree(picker->eligible);
    picker->eligible = loaded;
    return 1;
}

int IpPickerDumpBuckets(IpPicker *picker)
{
    if (picker == NULL)
        return 0;
    if (!picker->config.resumeable || picker->config.resume_file == NULL)
        return 1;
    if (picker->eligible->len > SIZE_MAX - picker->cooldown->len)
        return 0;

    BucketList *buckets = BucketListNewReserve(picker->eligible->len + picker->cooldown->len);
    if (buckets == NULL)
        return 0;
    for (size_t i = 0; i < picker->eligible->len; i++)
        if (!BucketListPush(buckets, picker->eligible->buckets[i]))
            goto fail;
    for (size_t i = 0; i < picker->cooldown->len; i++)
        if (!BucketListPush(buckets, picker->cooldown->buckets[i]))
            goto fail;

    char *json = NULL;
    if (!BucketListDumpJson(buckets, picker->config.bucket_width, &json))
        goto fail;

    int written = Fs_WriteFile(picker->config.resume_file, json, strlen(json));
    free(json);
    BucketListFree(buckets);
    return written;

fail:
    BucketListFree(buckets);
    return 0;
}
