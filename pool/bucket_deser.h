#ifndef BUCKET_DESER_H
#define BUCKET_DESER_H

#include "bucket.h"
#include "value/value.h"

Value *BucketDumpJson(BucketList *buckets);
char *BucketDumpJsonStr(BucketList *buckets);
int BucketLoadJson(BucketList *buckets, Value *json);
int BucketLoadJsonStr(BucketList *buckets, char *json);

/* Compatibility with the original string-only helpers. */
char *BucketListDumpJson(const BucketList *list);
int BucketListLoadJson(BucketList *list, char *json);

#endif
