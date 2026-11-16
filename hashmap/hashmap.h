#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include "../value/value.h"

typedef struct HashMapEntry HashMapEntry;

struct HashMap {
    HashMapEntry **buckets;
    size_t capacity;
};

typedef int (*HashMap_RemovePredicate)(const char *key, const Value *value,
    void *context);

HashMap *HashMap_New(size_t capacity);
int HashMap_Set(HashMap *map, const char *key, Value value);
Value *HashMap_Get(HashMap *map, const char *key);
int HashMap_Remove(HashMap *map, const char *key);
size_t HashMap_RemoveIf(HashMap *map, HashMap_RemovePredicate predicate, void *context);
void HashMap_Free(HashMap *map);

#endif
