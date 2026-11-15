#ifndef MOOLAH_HASHMAP_H
#define MOOLAH_HASHMAP_H

#include <stddef.h>
#include <stdint.h>
#include "value.h"

typedef struct MoolahHashMapEntry MoolahHashMapEntry;

typedef struct {
    MoolahHashMapEntry **buckets;
    size_t capacity;
} MoolahHashMap;

typedef int (*MoolahHashMap_RemovePredicate)(const char *key, const MoolahValue *value, void *context);
typedef int (*MoolahHashMap_RemoveU64Predicate)(uint64_t key, const MoolahValue *value, void *context);

MoolahHashMap *MoolahHashMap_New(size_t capacity);
int MoolahHashMap_Set(MoolahHashMap *map, const char *key, MoolahValue value);
MoolahValue *MoolahHashMap_Get(MoolahHashMap *map, const char *key);
int MoolahHashMap_Remove(MoolahHashMap *map, const char *key);
size_t MoolahHashMap_RemoveIf(MoolahHashMap *map, MoolahHashMap_RemovePredicate predicate, void *context);
int MoolahHashMap_SetU64(MoolahHashMap *map, uint64_t key, MoolahValue value);
MoolahValue *MoolahHashMap_GetU64(MoolahHashMap *map, uint64_t key);
int MoolahHashMap_RemoveU64(MoolahHashMap *map, uint64_t key);
size_t MoolahHashMap_RemoveIfU64(MoolahHashMap *map, MoolahHashMap_RemoveU64Predicate predicate, void *context);
void MoolahHashMap_Free(MoolahHashMap *map);

#endif
