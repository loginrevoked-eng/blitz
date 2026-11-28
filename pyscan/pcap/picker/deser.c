#include "bucket.h"
#include "value/public.h"
#include "cidr.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int IpPicker_BucketAsValue(Bucket *bucket, IpPickerValue *bucket_value)
{
    if(!bucket || !bucket_value) return 0;

    IpPickerValue *seed  = IpPickerValue_NewUnsignedInt(bucket->permutation_seed);
    IpPickerValue *index = IpPickerValue_NewUnsignedInt(bucket->permutation_current);
    char *start_text = Ipv4Repr(bucket->start);
    char *end_text = Ipv4Repr(bucket->end);
    IpPickerValue *start = start_text == NULL ? NULL : IpPickerValue_NewString(start_text);
    IpPickerValue *end = end_text == NULL ? NULL : IpPickerValue_NewString(end_text);
    if(!seed || !index || !start || !end)
    {
        if(seed)  IpPickerValue_Free(seed);
        if(index) IpPickerValue_Free(index);
        if(start) IpPickerValue_Free(start);
        if(end) IpPickerValue_Free(end);
        return 0;
    }

    IpPickerValue_Set(bucket_value, "permutation_seed", seed);
    IpPickerValue_Set(bucket_value, "permutation_current", index);
    IpPickerValue_Set(bucket_value, "start", start);
    IpPickerValue_Set(bucket_value, "end", end);
    free(seed);
    free(index);
    free(start);
    free(end);
    return 1;
}

int IpPicker_BucketListAsValue(BucketList *buckets, IpPickerValue *buckets_value)
{
    if(!buckets || !buckets_value) return 0;

    for(size_t i = 0; i < buckets->len; i++)
    {
        IpPickerValue *bucket = IpPickerValue_New(IPPICKER_VALUE_MAP, NULL);
        if(!bucket) return 0;

        if(!IpPicker_BucketAsValue(&buckets->buckets[i], bucket))
        {
            IpPickerValue_Free(bucket);
            return 0;
        }

        if(!IpPickerValue_Append(buckets_value, bucket))
        {
            IpPickerValue_Free(bucket);
            return 0;
        }
        free(bucket);
    }
    return 1;
}



int IpPicker_BucketListToJson(BucketList *buckets, uint32_t bucket_width, char **out)
{
    if(!buckets || !out) return 0;

    IpPickerValue *map = IpPickerValue_New(IPPICKER_VALUE_MAP, NULL);
    if(!map) return 0;

    IpPickerValue *width = IpPickerValue_NewUnsignedInt(bucket_width);
    IpPickerValue *bucketlist = IpPickerValue_New(IPPICKER_VALUE_ARRAY, NULL);
    if(!width || !bucketlist)
    {
        if(width)      IpPickerValue_Free(width);
        if(bucketlist) IpPickerValue_Free(bucketlist);
        IpPickerValue_Free(map);
        return 0;
    }

    if(!IpPicker_BucketListAsValue(buckets, bucketlist))
    {
        IpPickerValue_Free(width);
        IpPickerValue_Free(bucketlist);
        IpPickerValue_Free(map);
        return 0;
    }

    IpPickerValue_Set(map, "bucket_width", width);
    IpPickerValue_Set(map, "buckets", bucketlist);
    free(width);
    free(bucketlist);

    char *json = IpPickerValue_ToJson(map);
    IpPickerValue_Free(map);
    if(!json) return 0;

    *out = json;
    return 1;
}

int IpPicker_BucketFromValue(IpPickerValue *bucketmap, Bucket *bucket)
{
    if(!bucketmap || !bucket) return 0;
    if(!IpPickerValue_IsMap(bucketmap)) return 0;

    IpPickerValue *seed  = IpPickerValue_Get(bucketmap, "permutation_seed");
    IpPickerValue *index = IpPickerValue_Get(bucketmap, "permutation_current");
    IpPickerValue *start = IpPickerValue_Get(bucketmap, "start");
    IpPickerValue *end = IpPickerValue_Get(bucketmap, "end");
    if(!seed || !index || !start || !end ||
        seed->kind != IPPICKER_VALUE_UNSIGNED_INT || index->kind != IPPICKER_VALUE_UNSIGNED_INT) return 0;
    const char *start_text = IpPickerValue_AsString(start);
    const char *end_text = IpPickerValue_AsString(end);
    if (start_text == NULL || end_text == NULL) return 0;
    CidrRange *start_range = CidrRangeNew(start_text);
    CidrRange *end_range = CidrRangeNew(end_text);
    if (start_range == NULL || end_range == NULL) {
        free(start_range);
        free(end_range);
        return 0;
    }
    bucket->permutation_seed = IpPickerValue_AsUnsignedInt(seed);
    bucket->permutation_current = IpPickerValue_AsUnsignedInt(index);
    bucket->start = start_range->start;
    bucket->end = end_range->start;
    bucket->expires_at = 0;
    free(start_range);
    free(end_range);
    return 1;
}

int IpPicker_BucketListFromValue(IpPickerValue *bucketlist, BucketList *buckets)
{
    if(!bucketlist || !buckets) return 0;
    if(!IpPickerValue_IsArray(bucketlist)) return 0;

    size_t len = IpPickerValue_Len(bucketlist);
    for(size_t i = 0; i < len; i++)
    {
        IpPickerValue *bucket = IpPickerValue_At(bucketlist, i);
        if(!bucket) return 0;

        Bucket b = {0};
        if(!IpPicker_BucketFromValue(bucket, &b)) return 0;
        if(!BucketListPush(buckets, b)) return 0;
    }
    return 1;
}

int IpPicker_BucketListFromJson(const char *json, BucketList *buckets, uint32_t *bucket_width)
{
    if(!json || !buckets) return 0;

    IpPickerValue *map = IpPickerValue_NewNull();
    if (map == NULL)
        return 0;
    if (!IpPickerValue_ParseJson(map, json)) {
        IpPickerValue_Free(map);
        return 0;
    }

    int ok = 0;
    IpPickerValue *list  = IpPickerValue_Get(map, "buckets");
    IpPickerValue *width = IpPickerValue_Get(map, "bucket_width");

    if(list && (!bucket_width || width) && IpPicker_BucketListFromValue(list, buckets))
    {
        if(bucket_width) {
            ok = width->kind == IPPICKER_VALUE_UNSIGNED_INT;
            if (ok) *bucket_width = IpPickerValue_AsUnsignedInt(width);
        } else ok = 1;
    }

    IpPickerValue_Clear(map);
    IpPickerValue_Free(map);
    return ok;
}
