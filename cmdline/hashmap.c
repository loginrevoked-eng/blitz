#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

struct MoolahHashMapEntry {
    char *key;
    uint64_t key_u64;
    int is_u64;
    MoolahValue value;
    MoolahHashMapEntry *next;
};

static unsigned long MoolahHashMap_Hash(const char *key)
{
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*key++))
        hash = ((hash << 5) + hash) + (unsigned long)c;
    return hash;
}

static unsigned long MoolahHashMap_HashU64(uint64_t key)
{
    const unsigned char *bytes = (const unsigned char *)&key;
    unsigned long hash = 5381;
    for (size_t i = 0; i < sizeof key; i++)
        hash = ((hash << 5) + hash) + bytes[i];
    return hash;
}

int MoolahHashMap_Set(MoolahHashMap *map, const char *key, MoolahValue value)
{
    if (map == NULL || key == NULL || map->capacity == 0)
        return 0;

    size_t index = MoolahHashMap_Hash(key) % map->capacity;
    MoolahHashMapEntry *entry = map->buckets[index];
    while (entry != NULL) {
        if (!entry->is_u64 && strcmp(entry->key, key) == 0) {
            entry->value = value;
            return 1;
        }
        entry = entry->next;
    }

    entry = malloc(sizeof *entry);
    if (entry == NULL)
        return 0;
    entry->key = strdup(key);
    if (entry->key == NULL) {
        free(entry);
        return 0;
    }
    entry->value = value;
    entry->is_u64 = 0;
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    return 1;
}

MoolahValue *MoolahHashMap_Get(MoolahHashMap *map, const char *key)
{
    if (map == NULL || key == NULL || map->capacity == 0)
        return NULL;

    size_t index = MoolahHashMap_Hash(key) % map->capacity;
    MoolahHashMapEntry *entry = map->buckets[index];
    while (entry != NULL) {
        if (!entry->is_u64 && strcmp(entry->key, key) == 0)
            return &entry->value;
        entry = entry->next;
    }
    return NULL;
}

int MoolahHashMap_Remove(MoolahHashMap *map, const char *key)
{
    if (map == NULL || key == NULL || map->capacity == 0)
        return 0;

    size_t index = MoolahHashMap_Hash(key) % map->capacity;
    MoolahHashMapEntry **link = &map->buckets[index];
    while (*link != NULL) {
        MoolahHashMapEntry *entry = *link;
        if (!entry->is_u64 && strcmp(entry->key, key) == 0) {
            *link = entry->next;
            free(entry->key);
            free(entry);
            return 1;
        }
        link = &entry->next;
    }
    return 0;
}

size_t MoolahHashMap_RemoveIf(MoolahHashMap *map, MoolahHashMap_RemovePredicate predicate, void *context)
{
    if (map == NULL || predicate == NULL)
        return 0;

    size_t removed = 0;
    for (size_t i = 0; i < map->capacity; i++) {
        MoolahHashMapEntry **link = &map->buckets[i];
        while (*link != NULL) {
            MoolahHashMapEntry *entry = *link;
            if (!entry->is_u64 && predicate(entry->key, &entry->value, context)) {
                *link = entry->next;
                free(entry->key);
                free(entry);
                removed++;
            } else {
                link = &entry->next;
            }
        }
    }
    return removed;
}

int MoolahHashMap_SetU64(MoolahHashMap *map, uint64_t key, MoolahValue value)
{
    if (map == NULL || map->capacity == 0)
        return 0;
    size_t index = MoolahHashMap_HashU64(key) % map->capacity;
    for (MoolahHashMapEntry *entry = map->buckets[index]; entry; entry = entry->next) {
        if (entry->is_u64 && entry->key_u64 == key) {
            entry->value = value;
            return 1;
        }
    }
    MoolahHashMapEntry *entry = calloc(1, sizeof *entry);
    if (entry == NULL)
        return 0;
    entry->is_u64 = 1;
    entry->key_u64 = key;
    entry->value = value;
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    return 1;
}

MoolahValue *MoolahHashMap_GetU64(MoolahHashMap *map, uint64_t key)
{
    if (map == NULL || map->capacity == 0)
        return NULL;
    size_t index = MoolahHashMap_HashU64(key) % map->capacity;
    for (MoolahHashMapEntry *entry = map->buckets[index]; entry; entry = entry->next)
        if (entry->is_u64 && entry->key_u64 == key) return &entry->value;
    return NULL;
}

int MoolahHashMap_RemoveU64(MoolahHashMap *map, uint64_t key)
{
    if (map == NULL || map->capacity == 0)
        return 0;
    size_t index = MoolahHashMap_HashU64(key) % map->capacity;
    MoolahHashMapEntry **link = &map->buckets[index];
    while (*link) {
        MoolahHashMapEntry *entry = *link;
        if (entry->is_u64 && entry->key_u64 == key) {
            *link = entry->next;
            free(entry);
            return 1;
        }
        link = &entry->next;
    }
    return 0;
}

size_t MoolahHashMap_RemoveIfU64(MoolahHashMap *map, MoolahHashMap_RemoveU64Predicate predicate, void *context)
{
    if (map == NULL || predicate == NULL)
        return 0;
    size_t removed = 0;
    for (size_t i = 0; i < map->capacity; i++) {
        MoolahHashMapEntry **link = &map->buckets[i];
        while (*link) {
            MoolahHashMapEntry *entry = *link;
            if (entry->is_u64 && predicate(entry->key_u64, &entry->value, context)) {
                *link = entry->next;
                free(entry);
                removed++;
            } else {
                link = &entry->next;
            }
        }
    }
    return removed;
}

void MoolahHashMap_Free(MoolahHashMap *map)
{
    if (map == NULL)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        MoolahHashMapEntry *entry = map->buckets[i];
        while (entry != NULL) {
            MoolahHashMapEntry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(map->buckets);
    free(map);
}
