#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include "../cmdline/value.h"

typedef struct HashMapEntry HashMapEntry;

typedef struct {
    HashMapEntry **buckets;
    size_t capacity;
} HashMap;

typedef int (*HashMap_RemovePredicate)(const char *key, const Value *value, void *context);
typedef int (*HashMap_RemoveU64Predicate)(uint64_t key, const Value *value, void *context);

HashMap *HashMap_New(size_t capacity);
int HashMap_Set(HashMap *map, const char *key, Value value);
Value *HashMap_Get(HashMap *map, const char *key);
int HashMap_Remove(HashMap *map, const char *key);
size_t HashMap_RemoveIf(HashMap *map, HashMap_RemovePredicate predicate, void *context);
int HashMap_SetU64(HashMap *map, uint64_t key, Value value);
Value *HashMap_GetU64(HashMap *map, uint64_t key);
int HashMap_RemoveU64(HashMap *map, uint64_t key);
size_t HashMap_RemoveIfU64(HashMap *map, HashMap_RemoveU64Predicate predicate, void *context);
void HashMap_Free(HashMap *map);

#endif
