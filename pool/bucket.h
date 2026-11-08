#ifndef BUCKET_H
#define BUCKET_H

#include <stddef.h>
#include <stdint.h>

#include "cidr.h"

typedef struct {
    uint32_t permutation_seed;
    uint32_t permutation_current;
    Ipv4     start;
    Ipv4     end;
    uint64_t expires_at;
} Bucket;

typedef struct {
    Bucket *buckets;
    size_t  len;
    size_t  capacity;
} BucketList;

Bucket BucketNew(uint32_t permutation_seed, uint32_t permutation_current, Ipv4 start, Ipv4 end);


BucketList *BucketListNew(void);
BucketList *BucketListNewReserve(size_t capacity);
void BucketListFree(BucketList *list);


int  BucketListInit(BucketList *list, size_t capacity);
void BucketListDestroy(BucketList *list);

int  BucketListIsEmpty(const BucketList *list);


Bucket *BucketListAt(BucketList *list, size_t i);


int  BucketListPush(BucketList *list, Bucket bucket);


int  BucketListPop(BucketList *list, size_t i);
int  BucketListPopFront(BucketList *list);


char *BucketRepr(const Bucket *bucket);

char *BucketListDumpStr(const BucketList *list);
char *BucketListsDumpStr(const BucketList *list1, const BucketList *list2);

#endif
