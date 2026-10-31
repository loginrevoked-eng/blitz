#ifndef HASHSET_H
#define HASHSET_H

#include <stddef.h>
#include <stdint.h>
#include "../cmdline/value.h"

typedef struct HashSetEntry HashSetEntry;
typedef void (*HashSetU64Callback)(uint64_t value, void *context);

typedef struct {
    HashSetEntry **buckets;
    size_t capacity;
} HashSet;

HashSet *HashSet_New(size_t capacity);
int HashSet_Add(HashSet *set, Value value);
int HashSet_Contains(const HashSet *set, Value value);
int HashSet_Remove(HashSet *set, Value value);
int HashSet_PutString(HashSet *set, const char *value);
int HashSet_ContainsString(const HashSet *set, const char *value);
int HashSet_RemoveString(HashSet *set, const char *value);
int HashSet_PutU64(HashSet *set, uint64_t value);
int HashSet_ContainsU64(const HashSet *set, uint64_t value);
int HashSet_RemoveU64(HashSet *set, uint64_t value);
void HashSet_ForEachU64(const HashSet *set, HashSetU64Callback callback, void *context);
void HashSet_Free(HashSet *set);

#endif
