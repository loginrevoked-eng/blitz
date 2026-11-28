#include "hashset.h"
#include <stdlib.h>

struct SynScannerHashSetEntry {
    HashSetValue *key;
    struct SynScannerHashSetEntry *next;
};

static size_t HashKey(uint64_t key, size_t capacity)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < sizeof key; ++i) {
        hash ^= (unsigned char)(key >> (i * 8));
        hash *= UINT64_C(1099511628211);
    }
    return (size_t)(hash % capacity);
}

static int EntryMatches(const SynScannerHashSetEntry *entry, uint64_t key)
{
    return entry->key && entry->key->kind == HASHSET_VALUE_UINT64 &&
           entry->key->_uint64 == key;
}

SynScannerHashSet *SynScannerHashSet_New(size_t capacity)
{
    if (capacity == 0)
        return NULL;

    SynScannerHashSet *set = malloc(sizeof *set);
    if (set == NULL)
        return NULL;

    set->buckets = calloc(capacity, sizeof *set->buckets);
    if (set->buckets == NULL) {
        free(set);
        return NULL;
    }
    set->capacity = capacity;
    return set;
}

int SynScannerHashSet_PutU64(SynScannerHasSet *set, uint64_t u64)
{
    if (set == NULL || set->capacity == 0)
        return 0;

    size_t index = HashKey(u64, set->capacity);
    for (SynScannerHashSetEntry *entry = set->buckets[index]; entry; entry = entry->next)
        if (EntryMatches(entry, u64))
            return 0;

    SynScannerHashSetEntry *entry = calloc(1, sizeof *entry);
    if (entry == NULL)
        return 0;
    entry->key = HashSetValue_NewUInt64(u64);
    if (entry->key == NULL) {
        free(entry);
        return 0;
    }
    entry->next = set->buckets[index];
    set->buckets[index] = entry;
    return 1;
}

int SynScannerHashSet_GetU64(SynScannerHasSet *set, uint64_t u64)
{
    if (set == NULL || set->capacity == 0)
        return 0;

    size_t index = HashKey(u64, set->capacity);
    for (const SynScannerHashSetEntry *entry = set->buckets[index]; entry; entry = entry->next)
        if (EntryMatches(entry, u64))
            return 1;
    return 0;
}

int SynScannerHashSet_ExistsU64(SynScannerHasSet *set, uint64_t u64)
{
    return SynScannerHashSet_GetU64(set, u64);
}

int SynScannerHashSet_RemoveU64(SynScannerHasSet *set, uint64_t u64)
{
    if (set == NULL || set->capacity == 0)
        return 0;

    size_t index = HashKey(u64, set->capacity);
    SynScannerHashSetEntry **link = &set->buckets[index];
    while (*link) {
        SynScannerHashSetEntry *entry = *link;
        if (EntryMatches(entry, u64)) {
            *link = entry->next;
            HashSetValue_Free(entry->key);
            free(entry);
            return 1;
        }
        link = &entry->next;
    }
    return 0;
}

void SynScannerHashSet_ForEachU64(const SynScannerHasSet *set,
                                 SynScannerHashSetU64Callback callback,
                                 void *context)
{
    if (set == NULL || callback == NULL)
        return;

    for (size_t i = 0; i < set->capacity; ++i) {
        for (const SynScannerHashSetEntry *entry = set->buckets[i]; entry; entry = entry->next)
            if (entry->key && entry->key->kind == HASHSET_VALUE_UINT64)
                callback(entry->key->_uint64, context);
    }
}

void SynScannerHashSet_Free(SynScannerHashSet *set)
{
    if (set == NULL)
        return;

    for (size_t i = 0; i < set->capacity; ++i) {
        SynScannerHashSetEntry *entry = set->buckets[i];
        while (entry) {
            SynScannerHashSetEntry *next = entry->next;
            HashSetValue_Free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(set->buckets);
    free(set);
}
