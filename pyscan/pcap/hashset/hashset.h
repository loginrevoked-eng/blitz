#ifndef SYNSCANNER_HASHSET_H
#define SYNSCANNER_HASHSET_H

#include <stddef.h>
#include <stdint.h>
#include "value/value.h"

typedef struct SynScannerHashSetEntry SynScannerHashSetEntry;
typedef void (*SynScannerHashSetU64Callback)(uint64_t value, void *context);

typedef struct {
    SynScannerHashSetEntry **buckets;
    size_t capacity;
} SynScannerHashSet;
typedef SynScannerHashSet SynScannerHasSet;

SynScannerHashSet *SynScannerHashSet_New(size_t capacity);
int SynScannerHashSet_PutU64(SynScannerHasSet *set, uint64_t u64);
int SynScannerHashSet_GetU64(SynScannerHasSet *set, uint64_t u64);
int SynScannerHashSet_ExistsU64(SynScannerHasSet *set, uint64_t u64);
int SynScannerHashSet_RemoveU64(SynScannerHasSet *set, uint64_t u64);
void SynScannerHashSet_ForEachU64(const SynScannerHasSet *set,
                                 SynScannerHashSetU64Callback callback,
                                 void *context);
void SynScannerHashSet_Free(SynScannerHashSet *set);

#endif
