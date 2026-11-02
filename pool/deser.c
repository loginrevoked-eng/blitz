#include "bucket.h"
#include "crud.h"
#include "json.h"
#include "value.h"
#include <stddef.h>
#include <stdint.h>

int BucketAsValue(Bucket *bucket, Value *bucket_value)
{
    if(!bucket || !bucket_value) return 0;

    Value *seed  = ValueNewInt(bucket->permutation_seed);
    Value *index = ValueNewInt(bucket->permutation_current);
    Value *start = ValueNewLong((long)bucket->start);
    if(!seed || !index || !start)
    {
        if(seed)  ValueFree(seed);
        if(index) ValueFree(index);
        if(start) ValueFree(start);
        return 0;
    }

    ValueSet(bucket_value, "permutation_seed", seed);
    ValueSet(bucket_value, "permutation_index", index);
    ValueSet(bucket_value, "start", start);
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

int BucketListDumpJson(BucketList *buckets, uint64_t bucket_width, char **out)
{
    if(!buckets || !out) return 0;

    Value *map = ValueNew(VALUE_MAP, NULL);
    if(!map) return 0;

    Value *width = ValueNewLong((long)bucket_width);
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
    Value *index = ValueGet(bucketmap, "permutation_index");
    Value *start = ValueGet(bucketmap, "start");
    if(!seed || !index || !start) return 0;

    bucket->permutation_seed    = (uint64_t)ValueAsLong(seed);
    bucket->permutation_current = (uint64_t)ValueAsLong(index);
    bucket->start               = (uint64_t)ValueAsLong(start);
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

int BucketListFromJson(char *json, BucketList *buckets, uint64_t *bucket_width)
{
    if(!json || !buckets) return 0;

    Value map;
    if(!ParseJson(&map, json)) return 0;

    int ok = 0;
    Value *list  = ValueGet(&map, "buckets");
    Value *width = ValueGet(&map, "bucket_width");

    if(list && (!bucket_width || width) && BucketListFromValue(list, buckets))
    {
        if(bucket_width) *bucket_width = (uint64_t)ValueAsLong(width);
        ok = 1;
    }

    ValueClear(&map);
    return ok;
}
