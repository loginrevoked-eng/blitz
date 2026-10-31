#include "bucket_deser.h"
#include "value/crud.h"
#include "value/json.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ValueSet/ValueAppend copy the Value shell; the container owns its contents. */
static int Put(Value *container, const char *key, Value *item)
{
    if (container == NULL || item == NULL)
        return 0;
    if (key != NULL) {
        ValueSet(container, key, item);
        free(item);
        return 1;
    }
    if (ValueAppend(container, item) == NULL) {
        ValueDeepFree(item);
        return 0;
    }
    free(item);
    return 1;
}

static Value *NewString(const char *text)
{
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, text, len + 1);
    Value *value = ValueNewString(copy);
    if (value == NULL)
        free(copy);
    return value;
}

static Value *BucketToValue(const Bucket *bucket)
{
    Value *map = ValueNew(VALUE_MAP, NULL);
    if (map == NULL)
        return NULL;

    char start[16];
    char end[16];
    char current[3 * sizeof(size_t) + 1];
    char expiry[3 * sizeof(uint64_t) + 1];
    snprintf(start, sizeof start, "%u.%u.%u.%u",
             (unsigned)((bucket->start >> 24) & 0xff),
             (unsigned)((bucket->start >> 16) & 0xff),
             (unsigned)((bucket->start >> 8) & 0xff),
             (unsigned)(bucket->start & 0xff));
    snprintf(end, sizeof end, "%u.%u.%u.%u",
             (unsigned)((bucket->end >> 24) & 0xff),
             (unsigned)((bucket->end >> 16) & 0xff),
             (unsigned)((bucket->end >> 8) & 0xff),
             (unsigned)(bucket->end & 0xff));
    snprintf(current, sizeof current, "%zu", bucket->permutation_current);
    snprintf(expiry, sizeof expiry, "%llu", (unsigned long long)bucket->expires_at);

    Value *start_value = NewString(start);
    Value *end_value = NewString(end);
    Value *seed_value = ValueNewDouble((double)bucket->permutation_seed);
    Value *current_value = NewString(current);
    Value *cooling_value = ValueNewBool(bucket->is_cooling ? True : False);
    Value *expiry_value = NewString(expiry);
    if (start_value == NULL || end_value == NULL || seed_value == NULL || current_value == NULL ||
        cooling_value == NULL || expiry_value == NULL)
        goto fail;

    if (!Put(map, "start", start_value)) goto fail_no_values;
    start_value = NULL;
    if (!Put(map, "end", end_value)) goto fail_no_values;
    end_value = NULL;
    if (!Put(map, "permutation_seed", seed_value)) goto fail_no_values;
    seed_value = NULL;
    if (!Put(map, "permutation_current", current_value)) goto fail_no_values;
    current_value = NULL;
    if (!Put(map, "is_cooling", cooling_value)) goto fail_no_values;
    cooling_value = NULL;
    if (!Put(map, "expires_at", expiry_value)) goto fail_no_values;
    expiry_value = NULL;
    return map;

fail:
    if (start_value) ValueDeepFree(start_value);
    if (end_value) ValueDeepFree(end_value);
    if (seed_value) ValueDeepFree(seed_value);
    if (current_value) ValueDeepFree(current_value);
    if (cooling_value) ValueDeepFree(cooling_value);
    if (expiry_value) ValueDeepFree(expiry_value);
fail_no_values:
    ValueDeepFree(map);
    return NULL;
}

Value *BucketDumpJson(BucketList *buckets)
{
    if (buckets == NULL)
        return NULL;

    Value *root = ValueNew(VALUE_ARRAY, NULL);
    if (root == NULL)
        return NULL;
    for (size_t i = 0; i < buckets->len; i++) {
        Value *item = BucketToValue(&buckets->buckets[i]);
        if (!Put(root, NULL, item)) {
            ValueDeepFree(root);
            return NULL;
        }
    }
    return root;
}

char *BucketDumpJsonStr(BucketList *buckets)
{
    Value *value = BucketDumpJson(buckets);
    if (value == NULL)
        return NULL;
    char *json = ValueToJson(value);
    ValueDeepFree(value);
    return json;
}

static int ParseIpv4(const char *text, Ipv4 *out)
{
    unsigned a, b, c, d;
    int consumed = 0;
    if (text == NULL || sscanf(text, "%u.%u.%u.%u%n", &a, &b, &c, &d, &consumed) != 4 || text[consumed] != '\0' ||
        a > 255 || b > 255 || c > 255 || d > 255)
        return 0;
    *out = ((Ipv4)a << 24) | ((Ipv4)b << 16) | ((Ipv4)c << 8) | (Ipv4)d;
    return 1;
}

static int ParseUnsigned(const char *text, uint64_t max_value, uint64_t *out, int base)
{
    if (text == NULL || *text == '\0' || *text == '-')
        return 0;
    errno = 0;
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, base);
    if (errno == ERANGE || end == text || *end != '\0' || value > max_value)
        return 0;
    *out = (uint64_t)value;
    return 1;
}

static int ValueToSize(const Value *value, size_t *out)
{
    uint64_t parsed;
    if (value == NULL)
        return 0;
    if (value->kind == VALUE_STRING) {
        if (!ParseUnsigned(value->str, SIZE_MAX, &parsed, 10)) return 0;
        *out = (size_t)parsed;
        return 1;
    }
    if (value->kind == VALUE_INT && value->_int >= 0) {
        *out = (size_t)value->_int;
        return 1;
    }
    if (value->kind == VALUE_LONG && value->_long >= 0) {
        *out = (size_t)value->_long;
        return (uint64_t)value->_long <= SIZE_MAX;
    }
    if (value->kind == VALUE_DOUBLE && isfinite(value->_double) && value->_double >= 0.0 &&
        value->_double <= (double)SIZE_MAX && floor(value->_double) == value->_double) {
        *out = (size_t)value->_double;
        return 1;
    }
    return 0;
}

static int ValueToSeed(const Value *value, uint32_t *out)
{
    if (value == NULL)
        return 0;

    if (value->kind == VALUE_INT && value->_int >= 0) {
        *out = (uint32_t)value->_int;
        return (uint64_t)value->_int <= UINT32_MAX;
    }
    if (value->kind == VALUE_LONG && value->_long >= 0) {
        *out = (uint32_t)value->_long;
        return (uint64_t)value->_long <= UINT32_MAX;
    }
    if (value->kind == VALUE_DOUBLE && isfinite(value->_double) &&
        value->_double >= 0.0 && value->_double <= UINT32_MAX &&
        floor(value->_double) == value->_double) {
        *out = (uint32_t)value->_double;
        return 1;
    }
    if (value->kind == VALUE_STRING) {
        uint64_t parsed;
        if (!ParseUnsigned(value->str, UINT32_MAX, &parsed, 16))
            return 0;
        *out = (uint32_t)parsed;
        return 1;
    }
    return 0;
}

static int BucketFromValue(Value *item, Bucket *out)
{
    if (!ValueAsMap(item))
        return 0;

    Value *start_value = ValueGet(item, "start");
    Value *end_value = ValueGet(item, "end");
    Value *seed_value = ValueGet(item, "permutation_seed");
    Value *current_value = ValueGet(item, "permutation_current");
    Value *cooling_value = ValueGet(item, "is_cooling");
    Value *expiry_value = ValueGet(item, "expires_at");
    const char *start = ValueAsString(start_value);
    const char *end = ValueAsString(end_value);
    const char *expiry = ValueAsString(expiry_value);
    uint64_t expiry_number;

    Bucket bucket = {0};
    if (!ParseIpv4(start, &bucket.start) ||
        !ValueToSeed(seed_value, &bucket.permutation_seed))
        return 0;
    if (end_value != NULL) {
        if (!ParseIpv4(end, &bucket.end) || bucket.end < bucket.start)
            return 0;
    } else {
        bucket.end = bucket.start;
    }

    if (current_value != NULL && !ValueToSize(current_value, &bucket.permutation_current))
        return 0;
    if (cooling_value != NULL) {
        if (cooling_value->kind != VALUE_BOOL)
            return 0;
        bucket.is_cooling = ValueAsBool(cooling_value) == True;
    }
    if (expiry_value != NULL) {
        if (!ParseUnsigned(expiry, UINT64_MAX, &expiry_number, 10))
            return 0;
        bucket.expires_at = expiry_number;
    }

    *out = bucket;
    return 1;
}

int BucketLoadJson(BucketList *buckets, Value *json)
{
    if (buckets == NULL || json == NULL)
        return 0;
    ValueArray *items = ValueAsArray(json);
    if (items == NULL)
        return 0;

    size_t old_len = buckets->len;
    for (size_t i = 0; i < items->len; i++) {
        Bucket bucket;
        if (!BucketFromValue(&items->values[i], &bucket) || !BucketListPush(buckets, bucket)) {
            buckets->len = old_len;
            return 0;
        }
    }
    return 1;
}

int BucketLoadJsonStr(BucketList *buckets, char *json)
{
    if (buckets == NULL || json == NULL)
        return 0;
    Value *value = ValueNewNull();
    if (value == NULL)
        return 0;
    int ok = ParseJson(value, json) && BucketLoadJson(buckets, value);
    ValueDeepFree(value);
    return ok;
}

char *BucketListDumpJson(const BucketList *list)
{
    return BucketDumpJsonStr((BucketList *)list);
}

int BucketListLoadJson(BucketList *list, char *json)
{
    return BucketLoadJsonStr(list, json);
}
