#ifndef HASHSET_VALUE_MAP_H
#define HASHSET_VALUE_MAP_H

#include <stddef.h>

#include "value.h"

typedef struct HashSetValueMapEntry {
    char *key;
    HashSetValue value;
    struct HashSetValueMapEntry *next;
} HashSetValueMapEntry;

typedef struct HashSetValueMap {
    HashSetValueMapEntry **buckets;
    size_t capacity;
} HashSetValueMap;


typedef struct {
    const char *start;
    size_t len;
}HashSetValueMapStringView;

HashSetValueMap *HashSetValueMap_New(size_t capacity);
int HashSetValueMap_Set(HashSetValueMap *map, const char *key, HashSetValue value);
HashSetValue *HashSetValueMap_Get(HashSetValueMap *map, const char *key);
void HashSetValueMap_Free(HashSetValueMap *map);
size_t HashSetValueMap_Len(HashSetValueMap *map);

#endif
