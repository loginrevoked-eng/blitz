#ifndef DESER_H
#define DESER_H

#include <stddef.h>
#include <stdint.h>
#include "value/value.h"
#include "bucket.h"

int IpPicker_BucketAsValue(Bucket *bucket, IpPickerValue *bucket_value);
int IpPicker_BucketListAsValue(BucketList *buckets, IpPickerValue *buckets_value);
int IpPicker_BucketListToJson(BucketList *buckets, uint32_t bucket_width, char **out);

int IpPicker_BucketFromValue(IpPickerValue *bucketmap, Bucket *bucket);
int IpPicker_BucketListFromValue(IpPickerValue *bucketlist, BucketList *buckets);
int IpPicker_BucketListFromJson(const char *json, BucketList *buckets, uint32_t *bucket_width);

#endif
