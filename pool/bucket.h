#ifndef BUCKET_H
#define BUCKET_H

#include <stddef.h>
#include <stdint.h>

#include "cidr.h"   /* your header that defines Ipv4 */

typedef struct {
    uint32_t permutation_seed;
    size_t permutation_current; /* Feistel permutation position; serialized with the bucket. */
    Ipv4     start;
    Ipv4     end;
    int      is_cooling;
    uint64_t expires_at;    /* only meaningful while in cooldown */
} Bucket;

typedef struct {
    Bucket *buckets;
    size_t  len;
    size_t  capacity;
} BucketList;

/*
 * None of these functions lock anything. The caller must hold
 * the owning picker's lock when the list is shared between threads.
 * Functions returning int return 1 on success and 0 on failure.
 */

/* New lists reserve 32 buckets unless a capacity is supplied. */
BucketList *BucketListNew(void);
BucketList *BucketListNewReserve(size_t capacity);
void BucketListFree(BucketList *list);

/* capacity 0 means "allocate lazily on first append". */
int  BucketListInit(BucketList *list, size_t capacity);
void BucketListDestroy(BucketList *list);

int  BucketListIsEmpty(const BucketList *list);

/* Returns a pointer to element i, or NULL when i is out of range. */
Bucket *BucketListAt(BucketList *list, size_t i);

/* Adds to the back. Fails only if growing failed (list unchanged). */
int  BucketListPush(BucketList *list, Bucket bucket);

/* Removes element i and slides the rest left. Order preserved. O(n). */
int  BucketListPop(BucketList *list, size_t i);
int  BucketListPopFront(BucketList *list);

/* Returns an allocated description; the caller must free it. */
char *BucketRepr(const Bucket *bucket);
/* Returns an allocated JSON array; the caller must free it. */
char *BucketListDumpStr(const BucketList *list);
char *BucketListsDumpStr(const BucketList *list1, const BucketList *list2);

#endif /* BUCKET_H */
