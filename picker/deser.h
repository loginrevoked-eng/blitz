#ifndef DESER_H
#define DESER_H

#include <stddef.h>
#include <stdint.h>
#include "value.h"
#include "bucket.h"

int BucketAsValue(Bucket *bucket, Value *bucket_value);
int BucketListAsValue(BucketList *buckets, Value *buckets_value);
int BucketListDumpJson(BucketList *buckets, uint32_t bucket_width, char **out);

int BucketFromValue(Value *bucketmap, Bucket *bucket);
int BucketListFromValue(Value *bucketlist, BucketList *buckets);
int BucketListFromJson(char *json, BucketList *buckets, uint32_t *bucket_width);

#endif
