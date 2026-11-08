#include "bucket.h"
#include "str/str.h"

#include <stdlib.h>
#include <string.h>

Bucket BucketNew(uint32_t permutation_seed, uint32_t permutation_current, Ipv4 start, Ipv4 end)
{
    return (Bucket){
        .permutation_seed = permutation_seed,
        .permutation_current = permutation_current,
        .start = start,
        .end = end,
        .expires_at = 0
    };
}

BucketList *BucketListNewReserve(size_t capacity)
{
    BucketList *list = malloc(sizeof *list);
    if (list == NULL)
        return NULL;

    if (!BucketListInit(list, capacity)) {
        free(list);
        return NULL;
    }

    return list;
}

BucketList *BucketListNew(void)
{
    return BucketListNewReserve(32);
}

void BucketListFree(BucketList *list)
{
    if (list == NULL)
        return;

    BucketListDestroy(list);
    free(list);
}

char *BucketRepr(const Bucket *bucket)
{
    if (bucket == NULL)
        return NULL;

    char *start = Ipv4Repr(bucket->start);
    char *end = Ipv4Repr(bucket->end);
    if (start == NULL || end == NULL) {
        free(start);
        free(end);
        return NULL;
    }
    char *repr = strf("Bucket(seed=0x%08x, permutation_current=%u, start=%s, end=%s, expires_at=%llu)",
                bucket->permutation_seed,
                bucket->permutation_current,
                start, end, bucket->expires_at);
    free(start);
    free(end);
    return repr;
}

static int AppendBucketJson(char **json, const Bucket *bucket, int first)
{
    char *start = Ipv4Repr(bucket->start);
    char *end = Ipv4Repr(bucket->end);
    if (start == NULL || end == NULL) {
        free(start);
        free(end);
        return 0;
    }
    int ok = appendf(json,
        "%s{\"start\":\"%s\",\"end\":\"%s\",\"permutation_seed\":%u,\"permutation_current\":%u,\"expires_at\":\"%llu\"}",
        first ? "" : ",", start, end, bucket->permutation_seed,
        bucket->permutation_current, bucket->expires_at) != NULL;
    free(start);
    free(end);
    return ok;
}

char *BucketListsDumpStr(const BucketList *list1, const BucketList *list2)
{
    if (list1 == NULL || list2 == NULL)
        return NULL;

    char *json = strf("[");
    if (json == NULL)
        return NULL;

    int first = 1;
    const BucketList *lists[] = {list1, list2};
    for (size_t list_index = 0; list_index < 2; list_index++) {
        const BucketList *list = lists[list_index];
        for (size_t i = 0; i < list->len; i++) {
            if (!AppendBucketJson(&json, &list->buckets[i], first)) {
                free(json);
                return NULL;
            }
            first = 0;
        }
    }

    if (appendf(&json, "]") == NULL) {
        free(json);
        return NULL;
    }
    return json;
}

char *BucketListDumpStr(const BucketList *list)
{
    if (list == NULL)
        return NULL;
    BucketList empty = {0};
    return BucketListsDumpStr(list, &empty);
}

int BucketListInit(BucketList *list, size_t capacity)
{
    list->buckets  = NULL;
    list->len      = 0;
    list->capacity = 0;

    if (capacity == 0)
        return 1;

    list->buckets = malloc(capacity * sizeof(Bucket));
    if (!list->buckets)
        return 0;

    list->capacity = capacity;
    return 1;
}

void BucketListDestroy(BucketList *list)
{
    free(list->buckets);
    list->buckets  = NULL;
    list->len      = 0;
    list->capacity = 0;
}

int BucketListIsEmpty(const BucketList *list)
{
    return list->len == 0;
}

Bucket *BucketListAt(BucketList *list, size_t i)
{
    if (list == NULL || i >= list->len)
        return NULL;

    return &list->buckets[i];
}

int BucketListPush(BucketList *list, Bucket bucket)
{
    if (list->len == list->capacity)
    {
        size_t new_capacity = list->capacity ? list->capacity * 2 : 16;
        Bucket *grown = realloc(list->buckets, new_capacity * sizeof(Bucket));
        if (!grown)
            return 0;

        list->buckets  = grown;
        list->capacity = new_capacity;
    }

    list->buckets[list->len++] = bucket;
    return 1;
}

int BucketListPop(BucketList *list, size_t i)
{
    if (i >= list->len)
        return 0;

    memmove(&list->buckets[i], &list->buckets[i + 1], (list->len - i - 1) * sizeof(Bucket));

    list->len--;
    return 1;
}

int BucketListPopFront(BucketList *list)
{
    return BucketListPop(list, 0);
}
