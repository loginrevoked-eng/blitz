#include "bucket.h"
#include "crud.h"
#include "json.h"
#include "value.h"
#include "cidr.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int BucketAsValue(Bucket *bucket, Value *bucket_value)
{
    if(!bucket || !bucket_value) return 0;

    Value *seed  = ValueNewUnsignedInt(bucket->permutation_seed);
    Value *index = ValueNewUnsignedInt(bucket->permutation_current);
    char *start_text = Ipv4Repr(bucket->start);
    char *end_text = Ipv4Repr(bucket->end);
    Value *start = start_text == NULL ? NULL : ValueNewString(start_text);
    Value *end = end_text == NULL ? NULL : ValueNewString(end_text);
    if(!seed || !index || !start || !end)
    {
        if(seed)  ValueFree(seed);
        if(index) ValueFree(index);
        if(start) ValueFree(start);
        if(end) ValueFree(end);
        return 0;
    }

    ValueSet(bucket_value, "permutation_seed", seed);
    ValueSet(bucket_value, "permutation_current", index);
    ValueSet(bucket_value, "start", start);
    ValueSet(bucket_value, "end", end);
    return 1;
}

int BucketListAsValue(BucketList *buckets, Value *buckets_value)
{
    if(!buckets || !buckets_value) return 0;

    for(size_t i = 0; i < buckets->len; i++)
    {
        Value *bucket = ValueNew(VALUE_MAP, NULL);
        if(!bucket) return 0;

        if(!BucketAsValue(&buckets->buckets[i], bucket))
        {
            ValueFree(bucket);
            return 0;
        }

        if(!ValueAppend(buckets_value, bucket))
        {
            ValueFree(bucket);
            return 0;
        }
    }
    return 1;
}



int BucketListDumpJson(BucketList *buckets, uint32_t bucket_width, char **out)
{
    if(!buckets || !out) return 0;

    Value *map = ValueNew(VALUE_MAP, NULL);
    if(!map) return 0;

    Value *width = ValueNewUnsignedInt(bucket_width);
    Value *bucketlist = ValueNew(VALUE_ARRAY, NULL);
    if(!width || !bucketlist)
    {
        if(width)      ValueFree(width);
        if(bucketlist) ValueFree(bucketlist);
        ValueFree(map);
        return 0;
    }

    if(!BucketListAsValue(buckets, bucketlist))
    {
        ValueFree(width);
        ValueFree(bucketlist);
        ValueFree(map);
        return 0;
    }

    ValueSet(map, "bucket_width", width);
    ValueSet(map, "buckets", bucketlist);

    char *json = ValueToJson(map);
    ValueFree(map);
    if(!json) return 0;

    *out = json;
    return 1;
}

int BucketFromValue(Value *bucketmap, Bucket *bucket)
{
    if(!bucketmap || !bucket) return 0;
    if(!ValueIsMap(bucketmap)) return 0;

    Value *seed  = ValueGet(bucketmap, "permutation_seed");
    Value *index = ValueGet(bucketmap, "permutation_current");
    Value *start = ValueGet(bucketmap, "start");
    Value *end = ValueGet(bucketmap, "end");
    if(!seed || !index || !start || !end ||
        seed->kind != VALUE_UNSIGNED_INT || index->kind != VALUE_UNSIGNED_INT) return 0;
    const char *start_text = ValueAsString(start);
    const char *end_text = ValueAsString(end);
    if (start_text == NULL || end_text == NULL) return 0;
    CidrRange *start_range = CidrRangeNew(start_text);
    CidrRange *end_range = CidrRangeNew(end_text);
    if (start_range == NULL || end_range == NULL) {
        free(start_range);
        free(end_range);
        return 0;
    }
    bucket->permutation_seed = ValueAsUnsignedInt(seed);
    bucket->permutation_current = ValueAsUnsignedInt(index);
    bucket->start = start_range->start;
    bucket->end = end_range->start;
    bucket->expires_at = 0;
    free(start_range);
    free(end_range);
    return 1;
}

int BucketListFromValue(Value *bucketlist, BucketList *buckets)
{
    if(!bucketlist || !buckets) return 0;
    if(!ValueIsArray(bucketlist)) return 0;

    size_t len = ValueLen(bucketlist);
    for(size_t i = 0; i < len; i++)
    {
        Value *bucket = ValueAt(bucketlist, i);
        if(!bucket) return 0;

        Bucket b = {0};
        if(!BucketFromValue(bucket, &b)) return 0;
        if(!BucketListPush(buckets, b)) return 0;
    }
    return 1;
}

int BucketListFromJson(char *json, BucketList *buckets, uint32_t *bucket_width)
{
    if(!json || !buckets) return 0;

    Value *map = ValueNewNull();
    if (map == NULL)
        return 0;
    if (!ParseJson(map, json)) {
        ValueFree(map);
        return 0;
    }

    int ok = 0;
    Value *list  = ValueGet(map, "buckets");
    Value *width = ValueGet(map, "bucket_width");

    if(list && (!bucket_width || width) && BucketListFromValue(list, buckets))
    {
        if(bucket_width) {
            ok = width->kind == VALUE_UNSIGNED_INT;
            if (ok) *bucket_width = ValueAsUnsignedInt(width);
        } else ok = 1;
    }

    ValueClear(map);
    ValueFree(map);
    return ok;
}
