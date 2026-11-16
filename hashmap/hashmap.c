#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

struct HashMapEntry {
    char *key;
    Value value;
    HashMapEntry *next;
};

static unsigned long HashMap_Hash(const char *key)
{
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*key++) != 0)
        hash = ((hash << 5) + hash) + (unsigned long)c;
    return hash;
}

HashMap *HashMap_New(size_t capacity)
{
    if (capacity == 0)
        return NULL;

    HashMap *map = malloc(sizeof(*map));
    if (map == NULL)
        return NULL;

    map->buckets = calloc(capacity, sizeof(*map->buckets));
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
    for (HashMapEntry *entry = map->buckets[index]; entry != NULL;
         entry = entry->next) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return 1;
        }
    }

    HashMapEntry *entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return 0;
    size_t key_length = strlen(key) + 1;
    entry->key = malloc(key_length);
    if (entry->key == NULL) {
        free(entry);
        return 0;
    }
    memcpy(entry->key, key, key_length);
    entry->value = value;
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    return 1;
}

Value *HashMap_Get(HashMap *map, const char *key)
{
    if (map == NULL || key == NULL || map->capacity == 0)
        return NULL;

    size_t index = HashMap_Hash(key) % map->capacity;
    for (HashMapEntry *entry = map->buckets[index]; entry != NULL;
         entry = entry->next) {
        if (strcmp(entry->key, key) == 0)
            return &entry->value;
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
        if (strcmp(entry->key, key) == 0) {
            *link = entry->next;
            free(entry->key);
            free(entry);
            return 1;
        }
        link = &entry->next;
    }
    return 0;
}

size_t HashMap_RemoveIf(HashMap *map, HashMap_RemovePredicate predicate,
    void *context)
{
    if (map == NULL || predicate == NULL)
        return 0;

    size_t removed = 0;
    for (size_t i = 0; i < map->capacity; ++i) {
        HashMapEntry **link = &map->buckets[i];
        while (*link != NULL) {
            HashMapEntry *entry = *link;
            if (predicate(entry->key, &entry->value, context)) {
                *link = entry->next;
                free(entry->key);
                free(entry);
                ++removed;
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

    for (size_t i = 0; i < map->capacity; ++i) {
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
