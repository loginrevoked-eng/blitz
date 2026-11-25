#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include "value.h"

typedef struct HashMapEntry {
    char *key;
    Value value;
    struct HashMapEntry *next;
} HashMapEntry;
typedef struct {
    struct HashMap *map;
    HashMapEntry *current;
    HashMapEntry *next;
    size_t bucket;
} HashMapIter;

struct HashMap {
    HashMapEntry **buckets;
    size_t capacity;
};

HashMap *HashMap_New(size_t capacity);
int HashMap_Set(HashMap *map, const char *key, Value value);
Value *HashMap_Get(HashMap *map, const char *key);
int HashMap_Remove(HashMap *map, const char *key);
Value *HashMap_IterNext(HashMap *map, HashMapIter *iter);
int HashMap_IterEnd(HashMap *map, HashMapIter *iter);
const char *HashMap_IterKey(const HashMapIter *iter);
int HashMap_IterRemove(HashMap *map, HashMapIter *iter);
void HashMap_Free(HashMap *map);

#endif
