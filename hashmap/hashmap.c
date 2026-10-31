#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

struct HashMapEntry {
    char *key;
    uint64_t key_u64;
    int is_u64;
    Value value;
    HashMapEntry *next;
};

static unsigned long HashMap_Hash(const char *key)
{
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*key++))
        hash = ((hash << 5) + hash) + (unsigned long)c;
    return hash;
}

static unsigned long HashMap_HashU64(uint64_t key)
{
    const unsigned char *bytes = (const unsigned char *)&key;
    unsigned long hash = 5381;
    for (size_t i = 0; i < sizeof key; i++)
        hash = ((hash << 5) + hash) + bytes[i];
    return hash;
}

HashMap *HashMap_New(size_t capacity)
{
    if (capacity == 0)
        return NULL;

    HashMap *map = malloc(sizeof *map);
    if (map == NULL)
        return NULL;

    map->buckets = calloc(capacity, sizeof *map->buckets);
    map->capacity = capacity;
    if (map->buckets == NULL) {
        free(map);
        return NULL;
    }
    return map;
}

int HashMap_Set(HashMap *map, const char *key, Value value)
{
    if (map == NULL || key == NULL || map->capacity == 0)
        return 0;

    size_t index = HashMap_Hash(key) % map->capacity;
    HashMapEntry *entry = map->buckets[index];
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

Value *HashMap_Get(HashMap *map, const char *key)
{
    if (map == NULL || key == NULL || map->capacity == 0)
        return NULL;

    size_t index = HashMap_Hash(key) % map->capacity;
    HashMapEntry *entry = map->buckets[index];
    while (entry != NULL) {
        if (!entry->is_u64 && strcmp(entry->key, key) == 0)
            return &entry->value;
        entry = entry->next;
    }
    return NULL;
}

int HashMap_Remove(HashMap *map, const char *key)
{
    if (map == NULL || key == NULL || map->capacity == 0)
        return 0;

    size_t index = HashMap_Hash(key) % map->capacity;
    HashMapEntry **link = &map->buckets[index];
    while (*link != NULL) {
        HashMapEntry *entry = *link;
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

size_t HashMap_RemoveIf(HashMap *map, HashMap_RemovePredicate predicate, void *context)
{
    if (map == NULL || predicate == NULL)
        return 0;

    size_t removed = 0;
    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry **link = &map->buckets[i];
        while (*link != NULL) {
            HashMapEntry *entry = *link;
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

int HashMap_SetU64(HashMap *map, uint64_t key, Value value)
{
    if (map == NULL || map->capacity == 0)
        return 0;
    size_t index = HashMap_HashU64(key) % map->capacity;
    for (HashMapEntry *entry = map->buckets[index]; entry; entry = entry->next) {
        if (entry->is_u64 && entry->key_u64 == key) {
            entry->value = value;
            return 1;
        }
    }
    HashMapEntry *entry = calloc(1, sizeof *entry);
    if (entry == NULL)
        return 0;
    entry->is_u64 = 1;
    entry->key_u64 = key;
    entry->value = value;
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    return 1;
}

Value *HashMap_GetU64(HashMap *map, uint64_t key)
{
    if (map == NULL || map->capacity == 0)
        return NULL;
    size_t index = HashMap_HashU64(key) % map->capacity;
    for (HashMapEntry *entry = map->buckets[index]; entry; entry = entry->next)
        if (entry->is_u64 && entry->key_u64 == key) return &entry->value;
    return NULL;
}

int HashMap_RemoveU64(HashMap *map, uint64_t key)
{
    if (map == NULL || map->capacity == 0)
        return 0;
    size_t index = HashMap_HashU64(key) % map->capacity;
    HashMapEntry **link = &map->buckets[index];
    while (*link) {
        HashMapEntry *entry = *link;
        if (entry->is_u64 && entry->key_u64 == key) {
            *link = entry->next;
            free(entry);
            return 1;
        }
        link = &entry->next;
    }
    return 0;
}

size_t HashMap_RemoveIfU64(HashMap *map, HashMap_RemoveU64Predicate predicate, void *context)
{
    if (map == NULL || predicate == NULL)
        return 0;
    size_t removed = 0;
    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry **link = &map->buckets[i];
        while (*link) {
            HashMapEntry *entry = *link;
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

void HashMap_Free(HashMap *map)
{
    if (map == NULL)
        return;

    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry *entry = map->buckets[i];
        while (entry != NULL) {
            HashMapEntry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(map->buckets);
    free(map);
}
